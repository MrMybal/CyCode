#include "EndpointSecurity.h"
#include "cycode/Error.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cycode::detail {
std::string nativeEndpointSecret() {
    std::array<unsigned char,32> bytes{};
#ifdef _WIN32
    if(BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
        throw std::runtime_error("Cannot obtain native endpoint OS entropy");
#else
    if(getentropy(bytes.data(),bytes.size())!=0)
        throw std::runtime_error("Cannot obtain native endpoint OS entropy");
#endif
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(bytes.size()*2);
    for(const auto byte:bytes){result+=hex[byte>>4];result+=hex[byte&15];}
    return result;
}

bool nativeEndpointSecretMatches(const std::string &candidate,const std::string &expected) noexcept {
    if(expected.size()!=64 || candidate.size()!=expected.size())return false;
    volatile unsigned difference=0;
    for(size_t i=0;i<expected.size();++i)
        difference=difference|static_cast<unsigned>(static_cast<unsigned char>(candidate[i])^static_cast<unsigned char>(expected[i]));
    return difference==0;
}

namespace {
#ifdef _WIN32
struct OwnedHandle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~OwnedHandle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
struct LocalMemory {
    HLOCAL value=nullptr;
    ~LocalMemory(){if(value)LocalFree(value);}
};
// An explicit protected DACL is supplied at CREATE_NEW, before writing the
// credential. Default/inherited directory permissions are not relied upon.
class DescriptorSecurity {
  public:
    DescriptorSecurity() {
        OwnedHandle token;
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.value))
            throw std::runtime_error("Cannot inspect native endpoint user identity");
        DWORD bytes=0;
        GetTokenInformation(token.value,TokenUser,nullptr,0,&bytes);
        if(!bytes)throw std::runtime_error("Cannot inspect native endpoint user identity");
        std::vector<unsigned char> data(bytes);
        if(!GetTokenInformation(token.value,TokenUser,data.data(),bytes,&bytes))
            throw std::runtime_error("Cannot inspect native endpoint user identity");
        const auto sid=reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid;
        userSid_.resize(GetLengthSid(sid));
        if(!CopySid(static_cast<DWORD>(userSid_.size()),userSid_.data(),sid))
            throw std::runtime_error("Cannot retain native endpoint user identity");
        LPWSTR text=nullptr;
        if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid,&text))
            throw std::runtime_error("Cannot encode native endpoint user identity");
        LocalMemory user{reinterpret_cast<HLOCAL>(text)};
        const auto sddl=std::wstring(L"D:P(A;;FA;;;SY)(A;;FA;;;")+text+L")";
        PSECURITY_DESCRIPTOR descriptor=nullptr;
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))
            throw std::runtime_error("Cannot create native endpoint security descriptor");
        memory_.value=descriptor;
        attributes_={sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
    }
    SECURITY_ATTRIBUTES *get(){return &attributes_;}
    void verify(HANDLE file) const {
        PSECURITY_DESCRIPTOR actual=nullptr;PACL acl=nullptr;
        if(GetSecurityInfo(file,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&actual)!=ERROR_SUCCESS)
            throw std::runtime_error("Cannot verify private native descriptor permissions");
        LocalMemory memory{actual};
        SECURITY_DESCRIPTOR_CONTROL control=0;DWORD revision=0;
        if(!acl || !IsValidAcl(acl) || !GetSecurityDescriptorControl(actual,&control,&revision) || !(control&SE_DACL_PROTECTED))
            throw std::runtime_error("File system did not enforce private native descriptor permissions");
        bool user=false,system=false;
        for(DWORD index=0;index<acl->AceCount;++index) {
            void *value=nullptr;
            if(!GetAce(acl,index,&value))throw std::runtime_error("Invalid native descriptor access entry");
            const auto ace=static_cast<ACCESS_ALLOWED_ACE *>(value);
            if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceFlags || ace->Mask!=FILE_ALL_ACCESS)
                throw std::runtime_error("Unexpected native descriptor access permissions");
            const auto sid=static_cast<PSID>(&ace->SidStart);
            if(!IsValidSid(sid))throw std::runtime_error("Invalid native descriptor access identity");
            const bool own=EqualSid(sid,const_cast<unsigned char *>(userSid_.data()))!=FALSE;
            const bool localSystem=IsWellKnownSid(sid,WinLocalSystemSid)!=FALSE;
            if(!own && !localSystem)throw std::runtime_error("Native descriptor grants another user access");
            user=user||own;system=system||localSystem;
        }
        if(!user || !system)throw std::runtime_error("Incomplete native descriptor access permissions");
    }
  private:
    std::vector<unsigned char> userSid_;
    LocalMemory memory_;
    SECURITY_ATTRIBUTES attributes_{};
};
#else
struct OwnedDescriptor {
    int value=-1;
    ~OwnedDescriptor(){if(value>=0)::close(value);}
};
#endif
}

void publishNativeDescriptor(const std::filesystem::path &path,const std::string &bytes,
                             const std::optional<std::string> &baseline) {
    if(bytes.size()>maxTextFileBytes)throw std::runtime_error("Native descriptor exceeds its byte limit");
    auto check=[&] {
        if(std::filesystem::is_symlink(path))throw std::runtime_error("Native descriptor must not be a symbolic link");
        if(baseline) {
            if(readTextFileBytes(path)!=*baseline)
                throw NativeError(ErrorCode::DiskConflict,"Native descriptor changed before publication");
        } else if(std::filesystem::exists(path))throw NativeError(ErrorCode::DiskConflict,"Native descriptor already exists");
    };
    check();
    // Independent randomness: the credential is never used in a file name. The name is a
    // sibling that does not extend the descriptor's own name: see writeTextFileAtomic
    // (common/src/TextFile.cpp) for the antivirus behavioural detection this avoids.
    const auto temporary=path.parent_path()/(".cycode-endpoint-"+nativeEndpointSecret()+".tmp");
    struct Cleanup {
        std::filesystem::path path;bool owned=false;
        ~Cleanup(){if(owned){std::error_code error;std::filesystem::remove(path,error);}}
    } cleanup{temporary};
#ifdef _WIN32
    DescriptorSecurity security;
    OwnedHandle file{CreateFileW(temporary.c_str(),GENERIC_WRITE|READ_CONTROL,0,security.get(),CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create private native descriptor");
    cleanup.owned=true;
    security.verify(file.value);
    size_t done=0;
    while(done<bytes.size()) {
        DWORD written=0;
        if(!WriteFile(file.value,bytes.data()+done,static_cast<DWORD>(std::min<size_t>(bytes.size()-done,65536)),&written,nullptr) || !written)
            throw std::runtime_error("Cannot write private native descriptor");
        done+=written;
    }
    if(!FlushFileBuffers(file.value))throw std::runtime_error("Cannot flush private native descriptor");
    if(!CloseHandle(file.value))throw std::runtime_error("Cannot close private native descriptor");
    file.value=INVALID_HANDLE_VALUE;
    check();
    // Move the newly protected file itself: ReplaceFile would merge attributes
    // from a previous descriptor and could retain its permissive ACL.
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(baseline?MOVEFILE_REPLACE_EXISTING:0)))
        throw std::runtime_error("Cannot publish private native descriptor");
#else
    OwnedDescriptor file{::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600)};
    if(file.value<0)throw std::runtime_error("Cannot create private native descriptor");
    cleanup.owned=true;
    if(fchmod(file.value,0600)!=0)throw std::runtime_error("Cannot restrict native descriptor permissions");
    size_t done=0;
    while(done<bytes.size()) {
        const auto count=::write(file.value,bytes.data()+done,bytes.size()-done);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)throw std::runtime_error("Cannot write private native descriptor");
        done+=static_cast<size_t>(count);
    }
    if(fsync(file.value)!=0)throw std::runtime_error("Cannot flush private native descriptor");
    const auto descriptor=file.value;file.value=-1;
    if(::close(descriptor)!=0)throw std::runtime_error("Cannot close private native descriptor");
    check();
    if(baseline) {
        if(::rename(temporary.c_str(),path.c_str())!=0)throw std::runtime_error("Cannot publish private native descriptor");
    } else {
        // Exclusive publication when there was no baseline; never overwrite a
        // file created by another endpoint between the check and this commit.
        if(::link(temporary.c_str(),path.c_str())!=0)throw std::runtime_error("Cannot publish private native descriptor");
        if(::unlink(temporary.c_str())!=0)return; // Cleanup retries; final private descriptor is already published.
    }
#endif
    cleanup.owned=false;
}
} // namespace cycode::detail

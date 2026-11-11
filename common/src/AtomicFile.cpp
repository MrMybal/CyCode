#include "cycode/AtomicFile.h"
#include "cycode/Error.h"
#include "cycode/NativeTrace.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace cycode {
namespace {
#ifdef _WIN32
struct OwnedHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~OwnedHandle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
struct LocalMemory {
    HLOCAL value = nullptr;
    ~LocalMemory() {
        if (value)
            LocalFree(value);
    }
};
// An explicit protected DACL is supplied at CREATE_NEW, before writing the
// credential. Default/inherited directory permissions are not relied upon.
class DescriptorSecurity {
  public:
    DescriptorSecurity() {
        OwnedHandle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value))
            throw std::runtime_error("Cannot inspect native endpoint user identity");
        DWORD bytes = 0;
        GetTokenInformation(token.value, TokenUser, nullptr, 0, &bytes);
        if (!bytes)
            throw std::runtime_error("Cannot inspect native endpoint user identity");
        std::vector<unsigned char> data(bytes);
        if (!GetTokenInformation(token.value, TokenUser, data.data(), bytes, &bytes))
            throw std::runtime_error("Cannot inspect native endpoint user identity");
        const auto sid = reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid;
        userSid_.resize(GetLengthSid(sid));
        if (!CopySid(static_cast<DWORD>(userSid_.size()), userSid_.data(), sid))
            throw std::runtime_error("Cannot retain native endpoint user identity");
        LPWSTR text = nullptr;
        if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &text))
            throw std::runtime_error("Cannot encode native endpoint user identity");
        LocalMemory user{reinterpret_cast<HLOCAL>(text)};
        const auto sddl = std::wstring(L"D:P(A;;FA;;;SY)(A;;FA;;;") + text + L")";
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor,
                                                                  nullptr))
            throw std::runtime_error("Cannot create native endpoint security descriptor");
        memory_.value = descriptor;
        attributes_ = {sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    }
    SECURITY_ATTRIBUTES *get() {
        return &attributes_;
    }
    void verify(HANDLE file) const {
        PSECURITY_DESCRIPTOR actual = nullptr;
        PACL acl = nullptr;
        if (GetSecurityInfo(file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr,
                            &actual) != ERROR_SUCCESS)
            throw std::runtime_error("Cannot verify private native descriptor permissions");
        LocalMemory memory{actual};
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        if (!acl || !IsValidAcl(acl) || !GetSecurityDescriptorControl(actual, &control, &revision) ||
            !(control & SE_DACL_PROTECTED))
            throw std::runtime_error("File system did not enforce private native descriptor permissions");
        bool user = false, system = false;
        for (DWORD index = 0; index < acl->AceCount; ++index) {
            void *value = nullptr;
            if (!GetAce(acl, index, &value))
                throw std::runtime_error("Invalid native descriptor access entry");
            const auto ace = static_cast<ACCESS_ALLOWED_ACE *>(value);
            if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE || ace->Header.AceFlags ||
                ace->Mask != FILE_ALL_ACCESS)
                throw std::runtime_error("Unexpected native descriptor access permissions");
            const auto sid = static_cast<PSID>(&ace->SidStart);
            if (!IsValidSid(sid))
                throw std::runtime_error("Invalid native descriptor access identity");
            const bool own = EqualSid(sid, const_cast<unsigned char *>(userSid_.data())) != FALSE;
            const bool localSystem = IsWellKnownSid(sid, WinLocalSystemSid) != FALSE;
            if (!own && !localSystem)
                throw std::runtime_error("Native descriptor grants another user access");
            user = user || own;
            system = system || localSystem;
        }
        if (!user || !system)
            throw std::runtime_error("Incomplete native descriptor access permissions");
    }

  private:
    std::vector<unsigned char> userSid_;
    LocalMemory memory_;
    SECURITY_ATTRIBUTES attributes_{};
};
#endif
std::string metadataBytes(const std::filesystem::path &path) {
    constexpr size_t limit = 32 * 1024 * 1024;
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read atomic metadata baseline");
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || size > std::streamoff(limit))
        throw std::runtime_error("Atomic metadata exceeds 32 MiB");
    input.seekg(0);
    std::string text(static_cast<size_t>(size), '\0');
    input.read(text.data(), size);
    if ((!input && size) || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Atomic metadata changed during read");
    return text;
}
} // namespace
void writeFileAtomic(const std::filesystem::path &path, std::string_view bytes,
                     const AtomicFileOptions &options, const std::function<void()> &validate) {
    NativeTraceScope trace(NativeTraceEvent::AtomicWrite);
    if (validate)
        validate();
    if (options.replacementAttempts < 1 || options.replacementAttempts > 50)
        throw std::invalid_argument("Atomic replacement attempts must be between 1 and 50");
    const bool replace = options.publication != AtomicPublication::CreateExclusive;
    const bool preserve = options.publication == AtomicPublication::ReplacePreservingMetadata;
    auto temporary = path;
    // The temporary must not be the target's own name plus a long suffix. Kaspersky behavioural
    // analysis (PDM:Trojan.Win32.Generic) stopped the process after two or three saves when it was
    // named "<target>.cycode-save-<numbers>[.tmp]"; a sibling name that does not extend the target
    // passed, and flush, write-through and ReplaceFileW were not the trigger (2026-11-10 campaign).
    // The process id separates live processes, the counter separates saves; CREATE_NEW/O_EXCL
    // below still refuses a name that a crashed process with the same id left behind.
    static std::atomic<unsigned> sequence{0};
#ifdef _WIN32
    const auto process = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto process = static_cast<unsigned long>(::getpid());
#endif
    for (int attempt = 0; attempt < 16; ++attempt) {
        temporary = path.parent_path() /
                    (".cycode-save-" + std::to_string(process) + "-" + std::to_string(++sequence) + ".tmp");
        std::error_code ignored;
        if (!std::filesystem::exists(temporary, ignored))
            break;
    }
    struct Cleanup {
        std::filesystem::path path;
        bool owned = false;
        ~Cleanup() {
            if (owned) {
                NativeTraceScope trace(NativeTraceEvent::TemporaryCleanup);
                std::error_code ec;
                const bool removed = std::filesystem::remove(path, ec);
                trace.finish(ec        ? NativeTraceResult::Failure
                             : removed ? NativeTraceResult::Success
                                       : NativeTraceResult::Missing,
                             static_cast<std::uint64_t>(ec.value()));
            }
        }
    } cleanup{temporary};
#ifdef _WIN32
    std::optional<DescriptorSecurity> security;
    if (options.access == AtomicAccess::PrivateUser)
        security.emplace();
    HANDLE handle = INVALID_HANDLE_VALUE;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryCreate);
        handle =
            CreateFileW(temporary.c_str(), GENERIC_WRITE | (security ? READ_CONTROL : 0), 0,
                        security ? security->get() : nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        phase.finish(handle == INVALID_HANDLE_VALUE ? NativeTraceResult::Failure : NativeTraceResult::Success,
                     handle == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    }
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned = true;
    if (security) {
        try {
            security->verify(handle);
        } catch (...) {
            CloseHandle(handle);
            throw;
        }
    }
    size_t done = 0;
    bool okay = true;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryWrite);
        DWORD error = 0;
        while (done < bytes.size()) {
            DWORD written = 0;
            const bool wrote = WriteFile(handle, bytes.data() + done,
                                         static_cast<DWORD>(std::min<size_t>(bytes.size() - done, 65536)),
                                         &written, nullptr) != 0;
            if (!wrote || !written) {
                error = wrote ? ERROR_WRITE_FAULT : GetLastError();
                okay = false;
                break;
            }
            done += written;
        }
        phase.finish(okay ? NativeTraceResult::Success : NativeTraceResult::Failure, error);
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryFlush);
        const bool flushed = FlushFileBuffers(handle) != 0;
        phase.finish(flushed ? NativeTraceResult::Success : NativeTraceResult::Failure,
                     flushed ? 0 : GetLastError());
        okay = flushed && okay;
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryClose);
        const bool closed = CloseHandle(handle) != 0;
        phase.finish(closed ? NativeTraceResult::Success : NativeTraceResult::Failure,
                     closed ? 0 : GetLastError());
        okay = closed && okay;
    }
    if (!okay)
        throw std::runtime_error("Cannot write temporary save file");
    if (validate)
        validate();
    bool replaced = false;
    DWORD replacementError = 0;
    {
        NativeTraceScope phase(replace ? NativeTraceEvent::ReplaceExisting : NativeTraceEvent::PublishNew);
        for (unsigned attempt = 0; attempt < options.replacementAttempts; ++attempt) {
            if (attempt && validate)
                validate();
            replaced =
                preserve
                    ? bool(ReplaceFileW(path.c_str(), temporary.c_str(), nullptr,
                                        REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
                    : bool(MoveFileExW(temporary.c_str(), path.c_str(),
                                       MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)));
            if (replaced)
                break;
            replacementError = GetLastError();
            // ReplaceFile can partially move a target on failure: never retry it.
            if (preserve ||
                (replacementError != ERROR_SHARING_VIOLATION && replacementError != ERROR_LOCK_VIOLATION &&
                 replacementError != ERROR_ACCESS_DENIED) ||
                attempt + 1 == options.replacementAttempts)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        phase.finish(replaced ? NativeTraceResult::Success : NativeTraceResult::Failure, replacementError);
    }
    if (!replaced) {
        // ReplaceFile may fail after removing/moving the original. Preserve the
        // replacement bytes instead of deleting the only remaining save artifact.
        if (preserve && (replacementError == ERROR_UNABLE_TO_MOVE_REPLACEMENT ||
                         replacementError == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)) {
            cleanup.owned = false;
            NativeTraceScope retained(NativeTraceEvent::TemporaryRetained);
            retained.finish(NativeTraceResult::Skipped, replacementError);
            throw std::runtime_error(
                "Atomic replacement partially moved files; temporary cleanup was skipped");
        }
        throw std::runtime_error("Cannot atomically save file");
    }
    // Both successful APIs consume the temporary pathname. It no longer names
    // our file: a later occupant must never be deleted by this cleanup guard.
    cleanup.owned = false;
#else
    int handle = -1;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryCreate);
        handle = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
        phase.finish(handle < 0 ? NativeTraceResult::Failure : NativeTraceResult::Success,
                     handle < 0 ? errno : 0);
    }
    if (handle < 0)
        throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned = true;
    if (options.access == AtomicAccess::PrivateUser) {
        struct stat status {};
        if (::fchmod(handle, 0600) != 0 || ::fstat(handle, &status) != 0 || (status.st_mode & 0777) != 0600 ||
            status.st_uid != ::geteuid()) {
            ::close(handle);
            throw std::runtime_error("Cannot enforce private atomic file permissions");
        }
    }
    size_t done = 0;
    bool okay = true;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryWrite);
        int error = 0;
        while (done < bytes.size()) {
            const auto count =
                ::write(handle, bytes.data() + done, std::min<size_t>(bytes.size() - done, 65536));
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0) {
                error = count < 0 ? errno : EIO;
                okay = false;
                break;
            }
            done += static_cast<size_t>(count);
        }
        phase.finish(okay ? NativeTraceResult::Success : NativeTraceResult::Failure, error);
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryFlush);
        const bool flushed = ::fsync(handle) == 0;
        phase.finish(flushed ? NativeTraceResult::Success : NativeTraceResult::Failure, flushed ? 0 : errno);
        okay = flushed && okay;
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryClose);
        const bool closed = ::close(handle) == 0;
        phase.finish(closed ? NativeTraceResult::Success : NativeTraceResult::Failure, closed ? 0 : errno);
        okay = closed && okay;
    }
    if (!okay)
        throw std::runtime_error("Cannot write temporary save file");
    if (preserve)
        std::filesystem::permissions(temporary, std::filesystem::status(path).permissions());
    if (validate)
        validate();
    if (replace) {
        NativeTraceScope phase(NativeTraceEvent::ReplaceExisting);
        std::filesystem::rename(temporary, path);
        cleanup.owned = false;
        phase.finish(NativeTraceResult::Success);
    } else {
        NativeTraceScope phase(NativeTraceEvent::PublishNew);
        const bool linked = ::link(temporary.c_str(), path.c_str()) == 0;
        phase.finish(linked ? NativeTraceResult::Success : NativeTraceResult::Failure, linked ? 0 : errno);
        if (!linked)
            throw std::runtime_error("Cannot exclusively create file");
        // link() leaves our temporary name in place; its cleanup remains required.
    }
#endif
    trace.finish(NativeTraceResult::Success);
}
void writeMetadataFileAtomic(const std::filesystem::path &path, const std::string &text,
                             unsigned replacementAttempts) {
    if (text.size() > 32 * 1024 * 1024)
        throw std::runtime_error("Atomic metadata exceeds 32 MiB");
    if (std::filesystem::is_symlink(path))
        throw std::runtime_error("Linked atomic metadata refused");
    const std::optional<std::string> baseline =
        std::filesystem::exists(path) ? std::optional<std::string>(metadataBytes(path)) : std::nullopt;
    if (baseline && *baseline == text)
        return;
    std::filesystem::create_directories(path.parent_path());
    AtomicFileOptions options;
    options.publication = baseline ? AtomicPublication::Replace : AtomicPublication::CreateExclusive;
    options.replacementAttempts = replacementAttempts;
    writeFileAtomic(path, text, options, [&] {
        if (std::filesystem::is_symlink(path) ||
            (baseline ? metadataBytes(path) != *baseline : std::filesystem::exists(path)))
            throw NativeError(ErrorCode::DiskConflict,
                              "DiskConflict: generated metadata changed before publication");
    });
}
} // namespace cycode

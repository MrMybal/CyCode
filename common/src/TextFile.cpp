#include "cycode/Error.h"
#include "cycode/NativeTrace.h"
#include <cerrno>
#include "cycode/TextFile.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace cycode {
std::string readTextFileBytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read file");
    in.seekg(0, std::ios::end);
    const auto length = in.tellg();
    if (length < 0 || length > static_cast<std::streamoff>(maxTextFileBytes))
        throw std::runtime_error("Editor limit: 4 MiB per file");
    in.seekg(0);
    std::string text(static_cast<size_t>(length), '\0');
    in.read(text.data(), length);
    if ((!in && length) || in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("File changed during read");
    return text;
}
void validateEditorText(const std::string &text) {
    if (text.find('\0') != std::string::npos) throw std::runtime_error("Binary files cannot be edited as text");
    if (text.size() > maxTextFileBytes) throw std::runtime_error("Editor limit: 4 MiB per file");
    (void)nlohmann::json(text).dump();
}
std::string normalizeEditorText(const std::string &text) {
    std::string result; result.reserve(text.size());
    for (size_t i=0; i<text.size(); ++i) {
        if (text[i]=='\r') { result+='\n'; if (i+1<text.size() && text[i+1]=='\n') ++i; }
        else result+=text[i];
    }
    return result;
}
std::string encodeEditorText(const std::string &text, bool bom, bool crlf, bool crOnly) {
    validateEditorText(text);
    std::string result=bom ? "\xef\xbb\xbf" : "";
    result.reserve(text.size()+result.size());
    for (char c : text) {
        if (c=='\n' && crlf) result+='\r';
        result+=(c=='\n' && crOnly) ? '\r' : c;
    }
    if (result.size()>maxTextFileBytes) throw std::runtime_error("Editor limit: 4 MiB per file");
    return result;
}
void writeTextFileAtomic(const std::filesystem::path &path, const std::string &bytes,
                         const std::optional<std::string> &baseline) {
    NativeTraceScope trace(NativeTraceEvent::AtomicWrite);
    auto check=[&] {
        if (baseline) {
            if (readTextFileBytes(path)!=*baseline) throw NativeError(ErrorCode::DiskConflict,"DiskConflict: file changed outside the editor");
        } else if (std::filesystem::exists(path)) throw std::runtime_error("File already exists");
    };
    check();
    if (baseline) {
        const auto permissions=std::filesystem::status(path).permissions();
        using P=std::filesystem::perms;
        if ((permissions & (P::owner_write|P::group_write|P::others_write))==P::none)
            throw NativeError(ErrorCode::ReadOnly,"File is read-only");
    }
    auto temporary=path;
    // The temporary must not be the target's own name plus a long suffix. Kaspersky behavioural
    // analysis (PDM:Trojan.Win32.Generic) stopped the process after two or three saves when it was
    // named "<target>.cycode-save-<numbers>[.tmp]"; a sibling name that does not extend the target
    // passed, and flush, write-through and ReplaceFileW were not the trigger (2026-11-10 campaign).
    // The process id separates live processes, the counter separates saves; CREATE_NEW/O_EXCL
    // below still refuses a name that a crashed process with the same id left behind.
    static std::atomic<unsigned> sequence{0};
#ifdef _WIN32
    const auto process=static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto process=static_cast<unsigned long>(::getpid());
#endif
    for (int attempt=0;attempt<16;++attempt) {
        temporary=path.parent_path()/(".cycode-save-"+std::to_string(process)+"-"+std::to_string(++sequence)+".tmp");
        std::error_code ignored;
        if (!std::filesystem::exists(temporary,ignored)) break;
    }
    struct Cleanup {
        std::filesystem::path path;
        bool owned=false;
        ~Cleanup() {
            if (owned) {
                NativeTraceScope trace(NativeTraceEvent::TemporaryCleanup);
                std::error_code ec;
                const bool removed=std::filesystem::remove(path,ec);
                trace.finish(ec ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                             static_cast<std::uint64_t>(ec.value()));
            }
        }
    } cleanup{temporary};
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryCreate);
        handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        phase.finish(handle==INVALID_HANDLE_VALUE ? NativeTraceResult::Failure : NativeTraceResult::Success,
                     handle==INVALID_HANDLE_VALUE ? GetLastError() : 0);
    }
    if (handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned=true;
    size_t done=0; bool okay=true;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryWrite);
        DWORD error=0;
        while (done<bytes.size()) {
            DWORD written=0;
            const bool wrote=WriteFile(handle,bytes.data()+done,static_cast<DWORD>(std::min<size_t>(bytes.size()-done,65536)),&written,nullptr)!=0;
            if (!wrote || !written) {
                error=wrote ? ERROR_WRITE_FAULT : GetLastError();okay=false;break;
            }
            done+=written;
        }
        phase.finish(okay ? NativeTraceResult::Success : NativeTraceResult::Failure,error);
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryFlush);
        const bool flushed=FlushFileBuffers(handle)!=0;
        phase.finish(flushed ? NativeTraceResult::Success : NativeTraceResult::Failure,flushed ? 0 : GetLastError());
        okay=flushed && okay;
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryClose);
        const bool closed=CloseHandle(handle)!=0;
        phase.finish(closed ? NativeTraceResult::Success : NativeTraceResult::Failure,closed ? 0 : GetLastError());
        okay=closed && okay;
    }
    if (!okay) throw std::runtime_error("Cannot write temporary save file");
    check();
    bool replaced=false; DWORD replacementError=0;
    {
        NativeTraceScope phase(baseline ? NativeTraceEvent::ReplaceExisting : NativeTraceEvent::PublishNew);
        replaced=baseline ? bool(ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr))
                          : bool(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH));
        if (!replaced) replacementError=GetLastError();
        phase.finish(replaced ? NativeTraceResult::Success : NativeTraceResult::Failure,replacementError);
    }
    if (!replaced) {
        // ReplaceFile may fail after removing/moving the original. Preserve the
        // replacement bytes instead of deleting the only remaining save artifact.
        if (baseline && (replacementError==ERROR_UNABLE_TO_MOVE_REPLACEMENT ||
                         replacementError==ERROR_UNABLE_TO_MOVE_REPLACEMENT_2)) {
            cleanup.owned=false;
            NativeTraceScope retained(NativeTraceEvent::TemporaryRetained);
            retained.finish(NativeTraceResult::Skipped,replacementError);
            throw std::runtime_error("Atomic replacement partially moved files; temporary cleanup was skipped");
        }
        throw std::runtime_error("Cannot atomically save file");
    }
    // Both successful APIs consume the temporary pathname. It no longer names
    // our file: a later occupant must never be deleted by this cleanup guard.
    cleanup.owned=false;
#else
    int handle=-1;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryCreate);
        handle=::open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);
        phase.finish(handle<0 ? NativeTraceResult::Failure : NativeTraceResult::Success,handle<0 ? errno : 0);
    }
    if (handle<0) throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned=true;
    size_t done=0; bool okay=true;
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryWrite);
        int error=0;
        while (done<bytes.size()) {
            const auto count=::write(handle,bytes.data()+done,bytes.size()-done);
            if (count<=0) {error=count<0 ? errno : EIO;okay=false;break;}
            done+=static_cast<size_t>(count);
        }
        phase.finish(okay ? NativeTraceResult::Success : NativeTraceResult::Failure,error);
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryFlush);
        const bool flushed=::fsync(handle)==0;
        phase.finish(flushed ? NativeTraceResult::Success : NativeTraceResult::Failure,flushed ? 0 : errno);
        okay=flushed && okay;
    }
    {
        NativeTraceScope phase(NativeTraceEvent::TemporaryClose);
        const bool closed=::close(handle)==0;
        phase.finish(closed ? NativeTraceResult::Success : NativeTraceResult::Failure,closed ? 0 : errno);
        okay=closed && okay;
    }
    if (!okay) throw std::runtime_error("Cannot write temporary save file");
    if (baseline) std::filesystem::permissions(temporary,std::filesystem::status(path).permissions());
    check();
    if (baseline) {
        NativeTraceScope phase(NativeTraceEvent::ReplaceExisting);
        std::filesystem::rename(temporary,path);
        cleanup.owned=false;
        phase.finish(NativeTraceResult::Success);
    } else {
        NativeTraceScope phase(NativeTraceEvent::PublishNew);
        const bool linked=::link(temporary.c_str(),path.c_str())==0;
        phase.finish(linked ? NativeTraceResult::Success : NativeTraceResult::Failure,linked ? 0 : errno);
        if (!linked) throw std::runtime_error("Cannot exclusively create file");
        // link() leaves our temporary name in place; its cleanup remains required.
    }
#endif
    trace.finish(NativeTraceResult::Success);
}

} // namespace cycode

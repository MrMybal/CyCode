#include "cycode/TextFile.h"
#include <algorithm>
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
    auto check=[&] {
        if (baseline) {
            if (readTextFileBytes(path)!=*baseline) throw std::runtime_error("DiskConflict: file changed outside the editor");
        } else if (std::filesystem::exists(path)) throw std::runtime_error("File already exists");
    };
    check();
    if (baseline) {
        const auto permissions=std::filesystem::status(path).permissions();
        using P=std::filesystem::perms;
        if ((permissions & (P::owner_write|P::group_write|P::others_write))==P::none)
            throw std::runtime_error("File is read-only");
    }
    auto temporary=path;
    temporary+=".cycode-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(std::random_device{}());
    struct Cleanup { std::filesystem::path path; bool owned=false; ~Cleanup() { if (owned) { std::error_code ec; std::filesystem::remove(path,ec); } } } cleanup{temporary};
#ifdef _WIN32
    auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned=true;
    size_t done=0; bool okay=true;
    while (done<bytes.size()) {
        DWORD written=0;
        if (!WriteFile(handle,bytes.data()+done,static_cast<DWORD>(std::min<size_t>(bytes.size()-done,65536)),&written,nullptr) || !written) { okay=false; break; }
        done+=written;
    }
    okay=FlushFileBuffers(handle) && okay; CloseHandle(handle);
    if (!okay) throw std::runtime_error("Cannot write temporary save file");
    check();
    const bool replaced=baseline ? bool(ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr))
                                 : bool(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH));
    if (!replaced) throw std::runtime_error("Cannot atomically save file");
#else
    auto handle=::open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);
    if (handle<0) throw std::runtime_error("Cannot create temporary save file");
    cleanup.owned=true;
    size_t done=0; bool okay=true;
    while (done<bytes.size()) { const auto count=::write(handle,bytes.data()+done,bytes.size()-done); if (count<=0) { okay=false; break; } done+=static_cast<size_t>(count); }
    okay=(::fsync(handle)==0) && okay; ::close(handle);
    if (!okay) throw std::runtime_error("Cannot write temporary save file");
    if (baseline) std::filesystem::permissions(temporary,std::filesystem::status(path).permissions());
    check();
    if (baseline) std::filesystem::rename(temporary,path);
    else if (::link(temporary.c_str(),path.c_str())!=0) throw std::runtime_error("Cannot exclusively create file");
#endif
}
} // namespace cycode

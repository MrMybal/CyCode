#include "cycode/FileTransactions.h"
#include <algorithm>
#include <cerrno>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif
namespace cycode {
void writeFileExclusive(const std::filesystem::path &path, std::string_view bytes) {
#ifdef _WIN32
    const auto handle =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create an exclusive file");
    size_t at = 0;
    bool ok = true;
    while (at < bytes.size()) {
        DWORD written = 0;
        if (!WriteFile(handle, bytes.data() + at,
                       static_cast<DWORD>(std::min<size_t>(bytes.size() - at, 1048576)), &written, nullptr) ||
            !written) {
            ok = false;
            break;
        }
        at += written;
    }
    if (ok)
        ok = FlushFileBuffers(handle) != 0;
    const bool closed = CloseHandle(handle) != 0;
    if (!ok || !closed)
        throw std::runtime_error("Exclusive file write failed; its partial payload was retained");
#else
    const auto fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        throw std::runtime_error("Cannot create an exclusive file");
    size_t at = 0;
    bool ok = true;
    while (at < bytes.size()) {
        const auto count = ::write(fd, bytes.data() + at, std::min<size_t>(bytes.size() - at, 1048576));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            ok = false;
            break;
        }
        at += static_cast<size_t>(count);
    }
    if (ok)
        ok = ::fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    if (!ok || !closed)
        throw std::runtime_error("Exclusive file write failed; its partial payload was retained");
#endif
}
std::string directoryIdentity(const std::filesystem::path &path) {
#ifdef _WIN32
    const auto handle = CreateFileW(
        path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot inspect project destination directory");
    BY_HANDLE_FILE_INFORMATION info{};
    const bool ok = GetFileInformationByHandle(handle, &info) != 0;
    CloseHandle(handle);
    if (!ok || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Project destination must be an ordinary directory");
    return std::to_string(info.dwVolumeSerialNumber) + ":" + std::to_string(info.nFileIndexHigh) + ":" +
           std::to_string(info.nFileIndexLow);
#else
    struct stat info {};
    if (::lstat(path.c_str(), &info) != 0 || !S_ISDIR(info.st_mode))
        throw std::runtime_error("Project destination must be an ordinary directory");
    return std::to_string(info.st_dev) + ":" + std::to_string(info.st_ino);
#endif
}
void moveExclusive(const std::filesystem::path &from, const std::filesystem::path &to) {
    namespace fs = std::filesystem;
    for (const auto &path : {from, to}) {
        const auto absolute = fs::absolute(path).lexically_normal(), canonical = fs::weakly_canonical(path);
#ifdef _WIN32
        const bool same = _wcsicmp(absolute.c_str(), canonical.c_str()) == 0;
#else
        const bool same = absolute == canonical;
#endif
        if (!same || fs::is_symlink(fs::symlink_status(path)))
            throw std::runtime_error("File transactions cannot traverse links or redirected directories");
    }
    if (fs::symlink_status(to).type() != fs::file_type::not_found)
        throw std::runtime_error("Destination already exists");
#ifdef _WIN32
    const bool ok = MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#elif defined(__linux__)
    const bool ok = ::syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), 1) == 0;
#elif defined(__APPLE__)
    const bool ok = ::renamex_np(from.c_str(), to.c_str(), RENAME_EXCL) == 0;
#else
    const bool ok = false;
#endif
    if (!ok)
        throw std::runtime_error(
            "Cannot move exclusively (permissions, cross-volume move or unsupported filesystem)");
}
} // namespace cycode

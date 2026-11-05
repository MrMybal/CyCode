#include "cycode/NativeFileLease.h"
#include <cerrno>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace cycode {
struct NativeFileLease::State {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~State() {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
#else
    int handle = -1;
    ~State() {
        if (handle >= 0)
            ::close(handle);
    }
#endif
    bool held = false;
};
NativeFileLease::NativeFileLease(const std::filesystem::path &path, NativeFileLeaseMode mode)
    : state_(std::make_unique<State>()) {
    std::error_code error;
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path, error)))
        throw std::runtime_error("Lease path must not be a symbolic link");
#ifdef _WIN32
    state_->handle =
        CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (state_->handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open state lease file");
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(state_->handle, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        GetFileType(state_->handle) != FILE_TYPE_DISK)
        throw std::runtime_error("Invalid state lease file");
    OVERLAPPED overlap{};
    state_->held = bool(LockFileEx(state_->handle,
                                   (mode == NativeFileLeaseMode::exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) |
                                       LOCKFILE_FAIL_IMMEDIATELY,
                                   0, 1, 0, &overlap));
    if (!state_->held && GetLastError() != ERROR_LOCK_VIOLATION && GetLastError() != ERROR_IO_PENDING)
        throw std::runtime_error("Cannot acquire state lease");
#else
    state_->handle = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (state_->handle < 0)
        throw std::runtime_error("Cannot open state lease file");
    struct stat info {};
    if (::fstat(state_->handle, &info) != 0 || !S_ISREG(info.st_mode))
        throw std::runtime_error("Invalid state lease file");
    state_->held =
        ::flock(state_->handle, (mode == NativeFileLeaseMode::exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0;
    if (!state_->held && errno != EWOULDBLOCK && errno != EAGAIN)
        throw std::runtime_error("Cannot acquire state lease");
#endif
}
NativeFileLease::~NativeFileLease() = default;
bool NativeFileLease::acquired() const {
    return state_->held;
}
} // namespace cycode

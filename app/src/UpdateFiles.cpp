#include "cycode/update/detail/UpdateFiles.h"
#include "UpdateBudget.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#endif

namespace cycode::updates::detail {
void removeOwnedText(const std::filesystem::path &path, const std::string &baseline) {
    safe(path);
    if (baseline.empty() || baseline.size() > 8192)
        throw std::runtime_error("Invalid owned update marker baseline");
#ifdef _WIN32
    struct File {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~File() {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    } file;
    file.handle = CreateFileW(path.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot exclusively open the owned update marker");
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file.handle, &info) || GetFileType(file.handle) != FILE_TYPE_DISK ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
        info.nNumberOfLinks != 1 || info.nFileSizeHigh || info.nFileSizeLow != baseline.size())
        throw std::runtime_error("Owned update marker is changed or unsafe");
    std::string bytes(baseline.size(), '\0');
    DWORD readBytes = 0;
    if (!ReadFile(file.handle, bytes.data(), static_cast<DWORD>(bytes.size()), &readBytes, nullptr) ||
        readBytes != bytes.size() || bytes != baseline)
        throw std::runtime_error("Owned update marker changed; it was retained");
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(file.handle, FileDispositionInfo, &disposition, sizeof(disposition)))
        throw std::runtime_error("Cannot remove the verified owned update marker");
#else
    throw std::runtime_error("Owned application update marker removal currently requires Windows");
#endif
}
uint64_t allocationUnit(const std::filesystem::path &directory) {
    safe(directory);
    if (!directory.is_absolute() || !std::filesystem::is_directory(directory))
        throw std::runtime_error("Cannot inspect update volume geometry");
#ifdef _WIN32
    std::wstring root(32768, L'\0');
    if (!GetVolumePathNameW(directory.c_str(), root.data(), static_cast<DWORD>(root.size())))
        throw std::runtime_error("Cannot locate the update storage volume");
    root.resize(wcslen(root.c_str()));
    if (root.empty())
        throw std::runtime_error("Invalid update storage volume");
    if (root.back() != L'\\')
        root += L'\\';
    DWORD sectors = 0, bytes = 0, freeClusters = 0, clusters = 0;
    if (!GetDiskFreeSpaceW(root.c_str(), &sectors, &bytes, &freeClusters, &clusters))
        throw std::runtime_error("Cannot inspect update volume allocation size");
    const auto unit = uint64_t(sectors) * uint64_t(bytes);
    if (!allocationUnitValid(unit))
        throw std::runtime_error("Unsupported update volume allocation size");
    return unit;
#else
    throw std::runtime_error("Automatic installation geometry currently requires Windows");
#endif
}
uint64_t availableSpace(const std::filesystem::path &directory) {
    safe(directory);
    if (!std::filesystem::is_directory(directory))
        throw DownloadError(DownloadIssue::storage, "Update storage directory is unavailable");
#ifdef _WIN32
    auto name = directory.wstring();
    if (!name.empty() && name.back() != L'\\' && name.back() != L'/')
        name += L'\\';
    ULARGE_INTEGER available{};
    if (!GetDiskFreeSpaceExW(name.c_str(), &available, nullptr, nullptr))
        throw DownloadError(DownloadIssue::storage, "Cannot determine available update storage space");
    return available.QuadPart; // Caller-available bytes, including quota limits.
#else
    const auto info = std::filesystem::space(directory);
    if (info.available == static_cast<uintmax_t>(-1))
        throw DownloadError(DownloadIssue::storage, "Cannot determine available update storage space");
    return info.available;
#endif
}
std::unique_ptr<NativeFileLease> cacheLease(const std::filesystem::path &root, NativeFileLeaseMode mode) {
    const auto path = root / "data/updates/cache.lock";
    safe(path);
    auto lease = std::make_unique<NativeFileLease>(path, mode);
    if (!lease->acquired())
        throw DownloadError(DownloadIssue::busy,
                            "Update cache is in use; retry after the active operation finishes");
    return lease;
}
void moveFile(const std::filesystem::path &source, const std::filesystem::path &destination) {
    safe(source);
    safe(destination);
    std::filesystem::create_directories(destination.parent_path());
    safe(destination);
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot move update file without replacing an existing target");
#else
    throw std::runtime_error("Application update storage currently requires Windows x64");
#endif
}
bool workName(std::string_view name, std::string_view prefix) {
    return name.starts_with(prefix) && name.size() > prefix.size() && name.size() <= 128 &&
           std::all_of(name.begin() + prefix.size(), name.end(),
                       [](char c) { return (c >= '0' && c <= '9') || c == '-'; }) &&
           std::any_of(name.begin() + prefix.size(), name.end(), [](char c) { return c >= '0' && c <= '9'; });
}
bool digest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
void safe(const std::filesystem::path &supplied) {
    const auto path = std::filesystem::absolute(supplied).lexically_normal();
    auto current = path.root_path();
    for (const auto &part : path.relative_path()) {
        current /= part;
        std::error_code ec;
        const auto status = std::filesystem::symlink_status(current, ec);
        if (std::filesystem::is_symlink(status))
            throw std::runtime_error("Update path contains a symbolic link");
#ifdef _WIN32
        const auto attrs = GetFileAttributesW(current.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Update path contains a reparse point");
#endif
        if (ec && ec != std::errc::no_such_file_or_directory)
            throw std::runtime_error("Cannot inspect update path");
    }
}
std::string read(const std::filesystem::path &path, uint64_t maximum) {
    safe(path);
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Update input is not a regular file");
    const auto size = std::filesystem::file_size(path);
    if (size > maximum)
        throw std::runtime_error("Update input exceeds its limit");
    std::ifstream in(path, std::ios::binary);
    std::string result(static_cast<size_t>(size), 0);
    in.read(result.data(), result.size());
    if (!in || in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Update input changed while reading");
    return result;
}
std::filesystem::path work(const std::filesystem::path &root, const char *prefix) {
    safe(root / "data/updates");
    std::filesystem::create_directories(root / "data/updates");
    safe(root / "data/updates");
    std::random_device random;
    const auto path =
        root / "data/updates" /
        (std::string(prefix) + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(random()) + "-" + std::to_string(random()));
    if (!std::filesystem::create_directory(path))
        throw std::runtime_error("Cannot create update work directory");
    return path;
}
void jsonFile(const std::filesystem::path &path, const nlohmann::json &value) {
    safe(path);
    const auto bytes = value.dump(2) + "\n";
    if (bytes.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Update journal exceeds its size limit");
    const auto baseline = std::filesystem::exists(path)
                              ? std::optional<std::string>(read(path, 8 * 1024 * 1024))
                              : std::nullopt;
    writeTextFileAtomic(path, bytes, baseline);
}
} // namespace cycode::updates::detail
namespace cycode::updates {
std::string detail::hashFile(const std::filesystem::path &path, uint64_t maximum,
                             const std::atomic<bool> *cancel) {
#ifdef _WIN32
    if (cancel && cancel->load())
        throw detail::Cancellation("Update cancelled");
    detail::safe(path);
    if (!maximum || maximum > maximumPackage || std::filesystem::file_size(path) > maximum)
        throw std::runtime_error("Update hash input exceeds its size limit");
    struct State {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        ~State() {
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    } state;
    if (BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(state.algorithm, &state.hash, nullptr, 0, nullptr, 0, 0) < 0)
        throw std::runtime_error("Cannot initialize SHA-256");
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot read update file");
    std::array<char, 65536> buffer{};
    uint64_t total = 0;
    while (stream) {
        if (cancel && cancel->load())
            throw detail::Cancellation("Update cancelled");
        stream.read(buffer.data(), buffer.size());
        total += static_cast<uint64_t>(stream.gcount());
        if (total > maximum)
            throw std::runtime_error("Update hash input grew beyond its limit");
        if (stream.gcount() && BCryptHashData(state.hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                              static_cast<ULONG>(stream.gcount()), 0) < 0)
            throw std::runtime_error("Cannot hash update file");
    }
    if (!stream.eof())
        throw std::runtime_error("Cannot read update file");
    std::array<unsigned char, 32> hash{};
    if (BCryptFinishHash(state.hash, hash.data(), hash.size(), 0) < 0)
        throw std::runtime_error("Cannot finish SHA-256");
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (auto byte : hash) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
#else
    (void)path;
    (void)maximum;
    (void)cancel;
    throw std::runtime_error("Native application update installation currently requires Windows x64");
#endif
}
std::string sha256File(const std::filesystem::path &path, const std::atomic<bool> *cancel) {
    return detail::hashFile(path, maximumPackage, cancel);
}
} // namespace cycode::updates

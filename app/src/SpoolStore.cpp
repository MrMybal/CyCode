#include "cycode/SpoolStore.h"
#include "cycode/Error.h"
#include "cycode/StreamingData.h"
#include <array>
#include <cwchar>
#include <fstream>
#include <limits>
#include <monocypher.h>
#include <random>
namespace cycode {
namespace {
std::string hashBytes(std::string_view text) {
    std::array<uint8_t, 32> bytes;
    crypto_blake2b(bytes.data(), bytes.size(), reinterpret_cast<const uint8_t *>(text.data()), text.size());
    std::string out;
    for (auto b : bytes) {
        out += "0123456789abcdef"[b >> 4];
        out += "0123456789abcdef"[b & 15];
    }
    return out;
}
std::filesystem::path fixedDirectory(const std::filesystem::path &parent, const std::string &name) {
    const auto path = parent / name;
    const auto canonical = std::filesystem::weakly_canonical(path);
#ifdef _WIN32
    const bool same = _wcsicmp(path.c_str(), canonical.c_str()) == 0;
#else
    const bool same = path == canonical;
#endif
    if (!same || std::filesystem::is_symlink(path) ||
        (std::filesystem::exists(path) && !std::filesystem::is_directory(path)))
        throw NativeError(ErrorCode::FileSystemError, "Redirected private working-data directory refused");
    std::filesystem::create_directory(path);
    return path;
}
} // namespace
SpoolStore::SpoolStore(const std::filesystem::path &root) {
    const auto parent = fixedDirectory(fixedDirectory(root, ".cycode"), "working-data");
    std::random_device random;
    std::string id = "core-";
    for (size_t i = 0; i < 32; ++i)
        id += "0123456789abcdef"[random() & 15];
    directory_ = parent / id;
    if (!std::filesystem::create_directory(directory_))
        throw NativeError(ErrorCode::FileSystemError, "Cannot exclusively create private working directory");
}
SpoolStore::~SpoolStore() {
    // Exact owned files only. Preserve anything modified/replaced or redirected.
    for (const auto &item : owned_)
        try {
            if (!std::filesystem::is_symlink(directory_) &&
                std::filesystem::weakly_canonical(item.file) == item.file &&
                !std::filesystem::is_symlink(item.file) && std::filesystem::is_regular_file(item.file) &&
                std::filesystem::file_size(item.file) == item.bytes &&
                std::filesystem::last_write_time(item.file) == item.stamp)
                std::filesystem::remove(item.file);
        } catch (...) {
        }
    std::error_code ignored;
    std::filesystem::remove(directory_, ignored);
}
StoredBlob SpoolStore::create(uint64_t size, const AtomicByteProducer &produce) {
    if (size > uint64_t(std::numeric_limits<std::streamoff>::max()))
        throw NativeError(ErrorCode::CapacityExceeded, "Payload exceeds native stream offsets");
    const auto space = std::filesystem::space(directory_);
    if (size > space.available || space.available - size < 1024 * 1024)
        throw NativeError(ErrorCode::CapacityExceeded,
                          "Insufficient disk space for private transaction payload");
    const auto file = directory_ / ("blob-" + std::to_string(++sequence_) + ".bin");
    AtomicFileOptions options;
    options.access = AtomicAccess::PrivateUser;
    writeFileAtomicStream(file, produce, options);
    const auto bytes = std::filesystem::file_size(file);
    const auto stamp = std::filesystem::last_write_time(file);
    owned_.push_back({file, bytes, stamp});
    if (bytes != size)
        throw NativeError(ErrorCode::RevisionConflict, "Payload changed while importing");
    const auto digest = digestFileStreaming(file);
    if (auto previous = cached_[digest].lock())
        return previous;
    auto result = std::make_shared<SpoolBlob>(SpoolBlob{shared_from_this(), file, size, digest, stamp});
    cached_[digest] = result;
    return result;
}
StoredBlob SpoolStore::put(std::string_view bytes) {
    const auto digest = hashBytes(bytes);
    if (auto previous = cached_[digest].lock()) {
        previous->validate();
        return previous;
    }
    return create(bytes.size(), [&](const AtomicByteSink &sink) { sink(bytes); });
}
StoredBlob SpoolStore::importFile(const std::filesystem::path &path) {
    if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path))
        throw NativeError(ErrorCode::FileSystemError, "Large text requires a regular source file");
    const auto size = std::filesystem::file_size(path);
    const auto stamp = std::filesystem::last_write_time(path);
    return create(size, [&](const AtomicByteSink &sink) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw NativeError(ErrorCode::FileSystemError, "Cannot open source payload");
        std::array<char, 65536> bytes;
        uint64_t total = 0;
        while (input) {
            input.read(bytes.data(), bytes.size());
            const auto count = input.gcount();
            if (count) {
                sink({bytes.data(), size_t(count)});
                total += count;
            }
        }
        if (!input.eof() || total != size || std::filesystem::file_size(path) != size ||
            std::filesystem::last_write_time(path) != stamp)
            throw NativeError(ErrorCode::RevisionConflict, "Source changed while creating its baseline");
    });
}
void SpoolBlob::validate() const {
    if (std::filesystem::is_symlink(file) || !std::filesystem::is_regular_file(file) ||
        std::filesystem::file_size(file) != bytes || std::filesystem::last_write_time(file) != stamp)
        throw NativeError(ErrorCode::RevisionConflict, "Private working payload changed");
}
std::string SpoolBlob::read(uint64_t offset, size_t count) const {
    validate();
    if (offset > bytes || count > bytes - offset)
        throw NativeError(ErrorCode::InvalidParams, "Payload range exceeds its extent");
    std::ifstream input(file, std::ios::binary);
    input.seekg(std::streamoff(offset));
    std::string result(count, '\0');
    input.read(result.data(), std::streamsize(count));
    if (!input && count)
        throw NativeError(ErrorCode::FileSystemError, "Cannot read private payload range");
    validate();
    return result;
}
std::string SpoolBlob::load() const {
    validate();
    if (digestFileStreaming(file) != digest)
        throw NativeError(ErrorCode::RevisionConflict, "Private snapshot bytes changed");
    return read(0, size_t(bytes));
}
void SpoolBlob::stream(const AtomicByteSink &sink) const {
    validate();
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw NativeError(ErrorCode::FileSystemError, "Cannot open private payload");
    std::array<char, 65536> data;
    uint64_t total = 0;
    while (input) {
        input.read(data.data(), data.size());
        const auto count = input.gcount();
        if (count) {
            sink({data.data(), size_t(count)});
            total += count;
        }
    }
    if (!input.eof() || total != bytes)
        throw NativeError(ErrorCode::FileSystemError, "Incomplete private payload stream");
    validate();
}
} // namespace cycode

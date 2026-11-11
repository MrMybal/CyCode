#include "cycode/StreamingData.h"
#include "cycode/Error.h"
#include <array>
#include <cctype>
#include <fstream>
#include <monocypher.h>
namespace cycode {
namespace {
void checked(const std::atomic<bool> *cancel) {
    if (cancel && cancel->load())
        throw NativeError(ErrorCode::Cancelled, "Streaming data read cancelled");
}
} // namespace
std::string digestFileStreaming(const std::filesystem::path &path, const std::atomic<bool> *cancel) {
    if (std::filesystem::is_symlink(path) || !std::filesystem::is_regular_file(path))
        throw NativeError(ErrorCode::FileSystemError, "Streaming digest requires a regular, unlinked file");
    const auto size = std::filesystem::file_size(path);
    const auto stamp = std::filesystem::last_write_time(path);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw NativeError(ErrorCode::FileSystemError, "Cannot read streaming data");
    crypto_blake2b_ctx context;
    crypto_blake2b_init(&context, 32);
    std::array<char, 65536> bytes;
    uintmax_t total = 0;
    while (input) {
        checked(cancel);
        input.read(bytes.data(), bytes.size());
        const auto count = input.gcount();
        if (count) {
            crypto_blake2b_update(&context, reinterpret_cast<const uint8_t *>(bytes.data()), size_t(count));
            total += count;
        }
    }
    if (!input.eof() || total != size || std::filesystem::file_size(path) != size ||
        std::filesystem::last_write_time(path) != stamp)
        throw NativeError(ErrorCode::RevisionConflict, "Streaming data changed during read");
    std::array<uint8_t, 32> hash;
    crypto_blake2b_final(&context, hash.data());
    std::string result;
    for (auto value : hash) {
        result += "0123456789abcdef"[value >> 4];
        result += "0123456789abcdef"[value & 15];
    }
    return result;
}
void visitCompilationDatabase(const std::filesystem::path &path,
                              const std::function<void(const nlohmann::json &)> &visit,
                              const std::atomic<bool> *cancel) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw NativeError(ErrorCode::FileSystemError, "Cannot read compilation database");
    size_t consumed = 0;
    auto get = [&]() {
        if ((++consumed & 4095) == 0)
            checked(cancel);
        return input.get();
    };
    auto next = [&]() {
        int c;
        do {
            c = get();
        } while (c != EOF && (c == ' ' || c == '\t' || c == '\r' || c == '\n'));
        return c;
    };
    checked(cancel);
    if (input.peek() == 0xef) {
        if (get() != 0xef || get() != 0xbb || get() != 0xbf)
            throw NativeError(ErrorCode::InvalidParams, "Invalid compilation database UTF-8 BOM");
    }
    if (next() != '[')
        throw NativeError(ErrorCode::InvalidParams, "Compilation database must be an array");
    int first = next();
    if (first != ']')
        for (;;) {
            if (first != '{')
                throw NativeError(ErrorCode::InvalidParams, "Compilation command must be an object");
            std::string entry(1, '{');
            size_t depth = 1;
            bool quoted = false, escaped = false;
            while (depth) {
                const auto value = get();
                if (value == EOF)
                    throw NativeError(ErrorCode::InvalidParams, "Truncated compilation database");
                const char c = char(value);
                entry += c;
                if (entry.size() > 8 * 1024 * 1024)
                    throw NativeError(ErrorCode::CapacityExceeded,
                                      "A single compiler command exceeds the 8 MiB operation budget");
                if (quoted) {
                    if (escaped)
                        escaped = false;
                    else if (c == '\\')
                        escaped = true;
                    else if (c == '"')
                        quoted = false;
                } else if (c == '"')
                    quoted = true;
                else if (c == '{' || c == '[') {
                    if (++depth > 64)
                        throw NativeError(ErrorCode::InvalidParams,
                                          "Compiler command nesting exceeds its bound");
                } else if (c == '}' || c == ']')
                    --depth;
            }
            auto unit = nlohmann::json::parse(entry);
            visit(unit);
            checked(cancel);
            const auto delimiter = next();
            if (delimiter == ']')
                break;
            if (delimiter != ',')
                throw NativeError(ErrorCode::InvalidParams, "Invalid compiler command separator");
            first = next();
        }
    if (next() != EOF || !input.eof())
        throw NativeError(ErrorCode::InvalidParams, "Trailing compilation database data");
}
} // namespace cycode

#include "EndpointSecurity.h"
#include "cycode/AtomicFile.h"
#include "cycode/Error.h"
#include "cycode/TextFile.h"
#include <array>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif

namespace cycode::detail {
std::string nativeEndpointSecret() {
    std::array<unsigned char, 32> bytes{};
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("Cannot obtain native endpoint OS entropy");
#else
    if (getentropy(bytes.data(), bytes.size()) != 0)
        throw std::runtime_error("Cannot obtain native endpoint OS entropy");
#endif
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}

bool nativeEndpointSecretMatches(const std::string &candidate, const std::string &expected) noexcept {
    if (expected.size() != 64 || candidate.size() != expected.size())
        return false;
    volatile unsigned difference = 0;
    for (size_t i = 0; i < expected.size(); ++i)
        difference = difference | static_cast<unsigned>(static_cast<unsigned char>(candidate[i]) ^
                                                        static_cast<unsigned char>(expected[i]));
    return difference == 0;
}

void publishNativeDescriptor(const std::filesystem::path &path, const std::string &bytes,
                             const std::optional<std::string> &baseline) {
    if (bytes.size() > maxTextFileBytes)
        throw std::runtime_error("Native descriptor exceeds its byte limit");
    auto check = [&] {
        if (std::filesystem::is_symlink(path))
            throw std::runtime_error("Native descriptor must not be a symbolic link");
        if (baseline) {
            if (readTextFileBytes(path) != *baseline)
                throw NativeError(ErrorCode::DiskConflict, "Native descriptor changed before publication");
        } else if (std::filesystem::exists(path))
            throw NativeError(ErrorCode::DiskConflict, "Native descriptor already exists");
    };
    check();
    AtomicFileOptions options;
    options.publication = baseline ? AtomicPublication::Replace : AtomicPublication::CreateExclusive;
    options.access = AtomicAccess::PrivateUser;
    writeFileAtomic(path, bytes, options, check);
}
} // namespace cycode::detail

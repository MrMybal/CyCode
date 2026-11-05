#pragma once
#include "cycode/PluginPackages.h"
#include <optional>
namespace cycode::package_data {
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr uint64_t maxPayloadFile = 64 * 1024 * 1024, maxPayload = 256 * 1024 * 1024;
std::string sha512(const std::string &bytes);
bool hex(const std::string &, size_t bytes);
std::vector<uint8_t> unhex(const std::string &, size_t bytes);
bool portablePath(const std::string &);
bool packageId(const std::string &);
bool redirected(const fs::path &);
void directory(const fs::path &);
fs::path join(const fs::path &, const std::string &);
std::string read(const fs::path &, size_t maximum);
Json parse(const std::string &);
void fields(const Json &, std::initializer_list<const char *>);
void copyChecked(const fs::path &source, const fs::path *destination, const Json &file);
struct Package {
    Json document, signature, plugin;
    std::string bytes, signatureBytes, digest, publicKey;
    uint64_t totalBytes = 0;
};
Package package(const fs::path &, bool verifyPayload, bool installed = false);
Json summary(const Package &);
struct Store {
    fs::path root;
    std::unique_ptr<NativeFileLease> lease;
    std::optional<std::string> indexBytes, trustBytes;
    Json index, trust;
    explicit Store(const fs::path &installation, NativeFileLeaseMode mode = NativeFileLeaseMode::exclusive);
    std::string revision() const;
    std::string trustRevision() const;
    fs::path packagePath(const std::string &id, const std::string &digest) const;
    fs::path leasePath(const std::string &digest) const;
    bool trusted(const std::string &key) const;
    void saveIndex();
    void saveTrust();
};
} // namespace cycode::package_data

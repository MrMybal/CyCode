#pragma once
#include "cycode/NativeFileLease.h"
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
struct ManagedPluginCandidate {
    std::filesystem::path manifest;
    std::shared_ptr<NativeFileLease> lease;
    nlohmann::json metadata;
    nlohmann::json declaration;
};
// Installation-owned persistent state, independent of any workspace/job lifetime.
class PluginPackages {
  public:
    using Json = nlohmann::json;
    explicit PluginPackages(std::filesystem::path installation);
    Json catalog() const;
    Json inspect(const std::filesystem::path &source) const;
    Json install(const std::filesystem::path &source, const std::string &expectedRevision,
                 const std::string &expectedTrustRevision, const std::string &expectedPackage) const;
    Json remove(const std::string &id, const std::string &expectedRevision) const;
    Json rollback(const std::string &id, const std::string &digest, const std::string &expectedRevision,
                  const std::string &expectedTrustRevision) const;
    Json clean(const std::string &id, const std::string &digest, const std::string &expectedRevision) const;
    // Only the native desktop exposes publisher enrollment/revocation.
    Json trust(const std::string &publicKey, const std::string &label, bool enabled,
               const std::string &expectedTrustRevision) const;
    std::vector<ManagedPluginCandidate> load(Json &errors, bool verifyPayload = true) const;

  private:
    std::filesystem::path installation_;
};
} // namespace cycode

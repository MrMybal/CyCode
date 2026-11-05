#pragma once
#include "cycode/NativeFileLease.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cycode::updates {
inline constexpr auto repository = "MrMybal/CyCode";
inline constexpr uint64_t maximumPackage = 256ull * 1024 * 1024;
enum class DownloadMode { resume, fresh };
enum class DownloadIssue {
    none,
    access,
    network,
    response,
    storage,
    space,
    integrity,
    busy,
    installation,
    other
};
struct DownloadStatus;
class DownloadError : public std::runtime_error {
  public:
    DownloadError(DownloadIssue issue, const std::string &message)
        : std::runtime_error(message), issue_(issue) {}
    DownloadIssue issue() const noexcept {
        return issue_;
    }

  private:
    DownloadIssue issue_;
};
struct Asset {
    std::string name, url, digest;
    uint64_t size = 0;
};
struct Release {
    std::string version, notes, page;
    Asset asset;
};
struct Prepared {
    std::filesystem::path package, helper, installation;
    std::string version, digest;
};
struct Attempt {
    std::string version, state;
    std::filesystem::path directory;
    int64_t time = 0;    // Unix seconds, informational only.
    std::string failure; // Optional structured detail; legacy state remains "failed".
};
struct PartialDownload {
    std::filesystem::path directory;
    std::string version;
    Asset asset;
    uint64_t received = 0; // Observed bytes, not yet verified or installable.
};
struct Inventory {
    std::optional<Prepared> prepared;
    std::vector<PartialDownload> partials;
    std::vector<Attempt> attempts;
    uint32_t rejected = 0;
    bool limited = false;
};
std::array<uint32_t, 3> version(std::string_view);
bool newer(std::string_view candidate, std::string_view current);
std::string platform();
std::optional<Release> parseRelease(const nlohmann::json &, std::string_view current,
                                    std::string_view target);
bool packagePath(std::string_view);
bool portableInstallation(const std::filesystem::path &);
std::unique_ptr<NativeFileLease> installationUse(const std::filesystem::path &);
std::string sha256File(const std::filesystem::path &, const std::atomic<bool> *cancel = nullptr);
// Read-only, bounded local discovery; never downloads, installs or resumes consent.
Inventory inspect(const std::filesystem::path &, const std::atomic<bool> &);
void verifyPrepared(const Prepared &, const std::atomic<bool> &);
// Helper-owned outcome, separate from installation recovery journals.
void recordOutcome(const Prepared &, std::string_view state, std::string_view failure = {});
std::optional<Release> check(const std::atomic<bool> &cancel);
Prepared download(const Release &, const std::filesystem::path &, const std::atomic<bool> &,
                  std::atomic<uint64_t> &, DownloadMode = DownloadMode::resume,
                  const std::shared_ptr<DownloadStatus> & = {});
// An explicit desktop action, only after the ordinary save/discard close flow.
void launch(const Prepared &);
// Caller holds the exclusive installation lease. Validates the whole archive and
// old managed files first; restores originals on an ordinary filesystem failure.
void apply(const std::filesystem::path &archive, const std::filesystem::path &installation,
           const std::string &expectedVersion);
} // namespace cycode::updates

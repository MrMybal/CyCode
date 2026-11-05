#pragma once
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>

namespace cycode {
// Application-owned preferences, never a configuration loaded from a project.
class LanguagePreferences {
  public:
    LanguagePreferences(const std::filesystem::path &workspace, const std::filesystem::path &directory);
    nlohmann::json snapshot(bool refresh = true);
    nlohmann::json status();
    void update(const std::string &backend, const std::optional<nlohmann::json> &configuration,
                uint64_t expectedRevision);
    static void validateConfiguration(const nlohmann::json &configuration);
    static uint64_t revisionValue(const nlohmann::json &);

  private:
    void read();
    std::filesystem::path path_;
    std::string workspace_;
    std::optional<std::string> baseline_;
    nlohmann::json state_;
    std::mutex mutex_;
};
} // namespace cycode

#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode {
// Native project generation shared by desktop, plugin host and MCP. The owner
// grants a root; every destination must stay below it, in a new directory.
class ProjectTemplates {
  public:
    using Registry = std::function<nlohmann::json()>;
    using Guard =
        std::function<nlohmann::json(const std::string &, uint64_t, const std::function<nlohmann::json()> &)>;
    explicit ProjectTemplates(std::filesystem::path root, Registry = {}, Guard = {});
    static nlohmann::json catalogue();
    nlohmann::json list() const;
    nlohmann::json preview(const nlohmann::json &);
    nlohmann::json create(const std::string &id);

  private:
    struct Plan {
        nlohmann::json model;
        std::string parentIdentity;
        std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    };
    std::filesystem::path destination(const std::string &) const;
    nlohmann::json createLocked(const std::string &);
    Registry registry_;
    Guard guard_;
    std::filesystem::path root_;
    std::string rootIdentity_;
    std::mutex mutex_;
    std::map<std::string, Plan> plans_;
};
} // namespace cycode

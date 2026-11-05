#pragma once
#include "cycode/BuildSystem.h"
#include <map>
namespace cycode {
class UnrealProject {
  public:
    using Json = nlohmann::json;
    UnrealProject(std::filesystem::path root, std::filesystem::path entry, Json solution,
                  BuildSystem::Resolver);
    Json inspect(const Json &params = Json::object());
    Json run(const std::string &method, const Json &params, const std::atomic<bool> *cancellation = nullptr);
    Json prepare(const Json &, const std::atomic<bool> *cancellation = nullptr);
    Json automationPlan(const Json &);
    Json launchProfile(const Json &);
    Json artifacts(const Json &);
    void validateLaunchProfile(const Json &);
    Json automationRun(const Json &, const std::atomic<bool> *cancellation = nullptr);

  private:
    Json inspectLocked(const Json &);
    Json artifactsLocked(const Json &);
    Json runLocked(const std::string &, const Json &, const std::atomic<bool> *);
    Json automationContext(const Json &, const std::string &id);
    struct AutomationPlan {
        Json value;
        std::chrono::steady_clock::time_point created;
    };
    std::map<std::string, AutomationPlan> automationPlans_;
    uint64_t automationSequence_ = 0;
    std::filesystem::path root_, entry_;
    Json solution_;
    BuildSystem::Resolver resolve_;
    std::mutex mutex_;
};
} // namespace cycode

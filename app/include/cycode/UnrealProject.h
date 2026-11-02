#pragma once
#include "cycode/BuildSystem.h"
namespace cycode {
class UnrealProject {
  public:
    using Json = nlohmann::json;
    UnrealProject(std::filesystem::path root, std::filesystem::path entry, Json solution,
                  BuildSystem::Resolver);
    Json inspect(const Json &params = Json::object());
    Json run(const std::string &method, const Json &params);
    Json prepare(const Json &, const std::atomic<bool> *cancellation = nullptr);

  private:
    Json inspectLocked(const Json &);
    Json runLocked(const std::string &, const Json &, const std::atomic<bool> *);
    std::filesystem::path root_, entry_;
    Json solution_;
    BuildSystem::Resolver resolve_;
    std::mutex mutex_;
};
} // namespace cycode

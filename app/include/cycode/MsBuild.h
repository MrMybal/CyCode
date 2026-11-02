#pragma once
#include "cycode/BuildSystem.h"
namespace cycode {
// Read-only inspection never evaluates project imports. Evaluation/build are explicit.
class MsBuild {
  public:
    MsBuild(std::filesystem::path root, std::filesystem::path entry, BuildSystem::Resolver resolve);
    nlohmann::json inspect(const nlohmann::json &params = nlohmann::json::object());
    nlohmann::json tools(bool refresh = false);
    nlohmann::json run(const std::string &method, const nlohmann::json &params);
  private:
    nlohmann::json inspectLocked(const nlohmann::json &);
    nlohmann::json evaluate(const nlohmann::json &, const nlohmann::json &);
    std::vector<std::string> command(const nlohmann::json &);
    std::filesystem::path root_, entry_;
    BuildSystem::Resolver resolve_;
    std::recursive_mutex mutex_;
    nlohmann::json tools_;
};
} // namespace cycode

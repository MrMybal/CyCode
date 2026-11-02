#pragma once
#include "cycode/Process.h"
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode {
// CMake project services shared by the desktop, CLI and MCP.
class BuildSystem {
  public:
    using Json = nlohmann::json;
    using Resolver = std::function<std::filesystem::path(const std::string &)>;
    BuildSystem(std::filesystem::path root, Resolver resolve);
    Json detect(bool refresh = false);
    Json inspect(const Json &params = Json::object());
    Json run(const std::string &method, const Json &params);
    std::filesystem::path activeDatabaseDirectory() const;

  private:
    Json profile(const Json &) const;
    Json inspectLocked(const Json &, bool generateDatabase = false);
    Json readReply(const std::filesystem::path &, const std::string &) const;
    void saveProfile(const Json &);
    std::filesystem::path root_;
    Resolver resolve_;
    std::mutex operationMutex_, discoveryMutex_;
    Json detected_;
};
} // namespace cycode

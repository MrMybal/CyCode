#pragma once
#include "cycode/NativeFileLease.h"
#include <nlohmann/json.hpp>
#include <optional>
namespace cycode {
inline constexpr size_t sessionMaxFolds = 4096, sessionMaxTotalFolds = 16384;
nlohmann::json validateSessionEditor(const nlohmann::json &);
nlohmann::json validateSessionImage(const nlohmann::json &);
nlohmann::json validateSessionGit(const nlohmann::json &);
nlohmann::json validateWorkspaceSession(const nlohmann::json &, const std::string &workspace);
// Installation-owned GUI metadata only. Document text/recovery stays in Engine.
class WorkspaceSession {
  public:
    WorkspaceSession(std::filesystem::path directory, std::string workspace, std::string instance);
    nlohmann::json latest();
    void save(const nlohmann::json &, bool closed = false);
    std::filesystem::path layoutPath() const;

  private:
    std::filesystem::path directory_, file_;
    std::string workspace_, instance_;
    std::unique_ptr<NativeFileLease> lease_;
    std::optional<std::string> baseline_;
};
} // namespace cycode

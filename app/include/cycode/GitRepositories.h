#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
struct GitRepositorySelection {
    std::filesystem::path directory;
    std::string root, identity;
    bool linked = false, marker = false;
};
GitRepositorySelection gitRepositoryAt(const std::filesystem::path &workspace,
                                       const std::filesystem::path &directory, bool nearest = true);
nlohmann::json discoverGitRepositories(const std::filesystem::path &workspace);
std::string gitFileArgument(const std::filesystem::path &workspace, const GitRepositorySelection &,
                            const std::filesystem::path &file, bool allowDirectories = false);
nlohmann::json parseGitWorktrees(const std::string &porcelain);
nlohmann::json gitWorktreeMaintenanceRequest(const nlohmann::json &);
} // namespace cycode

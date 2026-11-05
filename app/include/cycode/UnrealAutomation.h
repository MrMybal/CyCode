#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
void validateUnrealMapPackages(const nlohmann::json &);
std::vector<std::string> unrealManagedTool(const std::filesystem::path &engineRoot, const std::string &tool);
nlohmann::json unrealAutomationRequest(const nlohmann::json &);
std::vector<std::string> unrealAutomationArguments(const nlohmann::json &,
                                                   const std::filesystem::path &project,
                                                   const std::filesystem::path &editor,
                                                   const std::filesystem::path &output);
} // namespace cycode

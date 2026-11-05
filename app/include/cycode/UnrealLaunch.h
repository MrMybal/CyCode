#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
std::filesystem::path unrealEditorExecutable(const std::filesystem::path &engine, int major,
                                             const std::string &platform, const std::string &configuration);
nlohmann::json unrealLaunchRequest(const nlohmann::json &);
nlohmann::json unrealBoundLaunchParameters(const nlohmann::json &profile);
void validateUnrealDebugDocuments(const nlohmann::json &before, const nlohmann::json &after);
nlohmann::json unrealDebugContinuation(const nlohmann::json &snapshot, const nlohmann::json &draft,
                                       const nlohmann::json &saved, const nlohmann::json &buildResult,
                                       const std::string &operationId);
nlohmann::json unrealLaunchSettings(const nlohmann::json &, const std::filesystem::path &project,
                                    const std::filesystem::path &editor, const std::string &workingDirectory);
} // namespace cycode

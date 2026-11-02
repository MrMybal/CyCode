#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
// Inspection only: opening a solution never evaluates MSBuild or generates project files.
std::filesystem::path workspaceRoot(const std::filesystem::path &entry);
nlohmann::json inspectSolution(const std::filesystem::path &entry);
} // namespace cycode

#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
// Inspection only: opening a solution never evaluates MSBuild or generates project files.
std::filesystem::path workspaceRoot(const std::filesystem::path &entry);
nlohmann::json inspectSolution(const std::filesystem::path &entry);
bool isSolutionPath(const std::filesystem::path &);
// Shared read-only reference classification for both solution formats.
nlohmann::json solutionReference(const std::filesystem::path &root, std::string value);
nlohmann::json inspectXmlSolution(const std::filesystem::path &, const std::string &text);
} // namespace cycode

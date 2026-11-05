#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
// Data-only profiles; process and preference ownership stay with existing services.
nlohmann::json normalizeRustAnalysisProfile(const nlohmann::json &);
nlohmann::json cargoRustAnalysisProfile(const nlohmann::json &project,
                                        const std::filesystem::path &workspace);
nlohmann::json rustAnalysisSettings(const nlohmann::json &settings, const nlohmann::json &profile,
                                    const std::filesystem::path &workspace);
std::vector<std::string> rustAnalysisArguments(const nlohmann::json &profile,
                                               const std::vector<std::string> &requested,
                                               const std::vector<std::string> &defaults,
                                               const std::string &rustup);
} // namespace cycode

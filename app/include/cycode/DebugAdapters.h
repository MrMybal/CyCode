#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
// Declarative, UI-free stdio-DAP adapters. Registration does not spawn a process.
void validateDebugAdapter(const nlohmann::json &);
void validateDebugAdapterOptions(const nlohmann::json &);
nlohmann::json expandDebugAdapter(nlohmann::json, const std::filesystem::path &pluginDirectory,
                                  const std::filesystem::path &workspace);
nlohmann::json debugAdapterSummary(const nlohmann::json &);
nlohmann::json debugAdapterRequest(const nlohmann::json &definition, const nlohmann::json &profile,
                                   const std::filesystem::path &workingDirectory, bool attach,
                                   const nlohmann::json &pid = nullptr);
} // namespace cycode

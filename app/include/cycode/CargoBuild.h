#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
using CargoResolver = std::function<std::filesystem::path(const std::string &)>;
// Pure native planning/model helpers. Cargo execution belongs to ProjectTasks.
void validateCargoOptions(const nlohmann::json &profile);
std::vector<std::string> cargoArguments(const nlohmann::json &profile, const std::string &operation,
                                        const std::filesystem::path &manifest);
nlohmann::json cargoWorkspaceModel(const nlohmann::json &metadata, const std::filesystem::path &workspace,
                                   const CargoResolver &resolve);
nlohmann::json cargoExecutable(const nlohmann::json &message, const nlohmann::json &model,
                               const nlohmann::json &profile, const std::filesystem::path &workspace,
                               const CargoResolver &resolve);
} // namespace cycode

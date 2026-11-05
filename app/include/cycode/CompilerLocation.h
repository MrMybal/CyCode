#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
// Parse one display line, then resolve its explicit path through the owner's
// workspace guard. Does not read source contents or execute a tool.
nlohmann::json compilerLocation(const nlohmann::json &parameters, const std::filesystem::path &root,
                                const std::function<std::filesystem::path(const std::string &)> &resolve);
} // namespace cycode

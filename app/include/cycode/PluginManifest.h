#pragma once
#include <array>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
bool pluginIdentifier(const std::string &);
void validatePluginManifest(const nlohmann::json &);
std::array<unsigned, 3> pluginVersion(const std::string &text);
nlohmann::json pluginHostInfo();
// Validate metadata without opening a native library. Malformed constraints throw;
// well-formed but incompatible constraints return compatible:false and a reason.
nlohmann::json pluginCompatibility(const nlohmann::json &manifest);
} // namespace cycode

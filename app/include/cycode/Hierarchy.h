#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// Validate protocol data independently of transport, UI and workspace permissions.
nlohmann::json hierarchyRange(const nlohmann::json &);
void validateHierarchyItem(const nlohmann::json &);
std::string hierarchyMethod(const std::string &kind, const std::string &direction = {});
} // namespace cycode

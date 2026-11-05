#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
bool debugMutationOperation(const std::string &method);
nlohmann::json debugMutationArguments(const std::string &method, const nlohmann::json &parameters,
                                      const nlohmann::json &capabilities, int frame);
nlohmann::json debugMutationResult(const std::string &method, const nlohmann::json &body,
                                   const nlohmann::json &parameters);
nlohmann::json debugHexBytes(const std::string &text);
// Follows DAP's evaluateName preference and readOnly presentation metadata.
std::string debugVariableEditMethod(const nlohmann::json &variable, const nlohmann::json &capabilities);
} // namespace cycode

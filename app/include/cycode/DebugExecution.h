#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
bool debugExecutionOperation(const std::string &method);
bool advancedDebugExecutionOperation(const std::string &method);
nlohmann::json debugExecutionArguments(const std::string &method, const nlohmann::json &parameters,
                                       const nlohmann::json &state);
nlohmann::json debugExecutionResult(const std::string &method, const nlohmann::json &body);
bool debugFrameRestartable(const nlohmann::json &state);
// Cache invalidation within one pause must not hide a newly stopped event.
bool debugExecutionNeedsResume(const std::string &phase, uint64_t dispatchEpoch, uint64_t currentEpoch);
} // namespace cycode

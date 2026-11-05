#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// UI-free, bounded DAP contracts shared by the owner, native tests and views.
bool stoppedDebugOperation(const std::string &method);
void validateDebugCapabilities(const nlohmann::json &capabilities);
nlohmann::json debugBreakpointVerification(const nlohmann::json &point, bool signedId = false);
nlohmann::json debugInspectionArguments(const std::string &method, const nlohmann::json &parameters,
                                        const nlohmann::json &capabilities);
nlohmann::json debugInspectionResult(const std::string &method, const nlohmann::json &body,
                                     const nlohmann::json &arguments);
void validateFunctionBreakpoints(const nlohmann::json &points);
nlohmann::json functionBreakpointArguments(const nlohmann::json &points, const nlohmann::json &capabilities);
void validateExceptionFilters(const nlohmann::json &filters);
nlohmann::json exceptionBreakpointArguments(const nlohmann::json &filters,
                                            const nlohmann::json &capabilities);
} // namespace cycode

#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace cycode {
inline bool supportsBuildTimeout(const std::string &method) {
    return method == "build.configure" || method == "build.run" || method == "build.presetRun" || method == "msbuild.build" ||
           method == "msbuild.rebuild" || method == "msbuild.clean" || method == "msbuild.restore" ||
           method == "unreal.generate" || method == "unreal.database" || method == "unreal.build" ||
           method == "unreal.prepare" || method == "unreal.automationRun";
}
// Process deadlines, independent from probing/evaluation limits and cancellation.
inline std::chrono::seconds buildTimeout(const nlohmann::json &parameters, int fallback = 180) {
    if (!parameters.contains("timeoutSeconds"))
        return std::chrono::seconds(fallback);
    const auto &value = parameters.at("timeoutSeconds");
    if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<uint64_t>() > 86400) ||
        (!value.is_number_unsigned() && (value.get<int64_t>() < 1 || value.get<int64_t>() > 86400)))
        throw std::runtime_error("Build timeout must be an integer from 1 to 86400 seconds");
    const auto count = value.get<int>();
    if (count < 1)
        throw std::runtime_error("Build timeout must be positive");
    return std::chrono::seconds(count);
}
} // namespace cycode

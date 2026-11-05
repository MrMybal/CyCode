#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
// Host-owned launch options for Microsoft's JDT LS Java debug add-on.
void validateJavaDebugOptions(const nlohmann::json &, bool requireMain = false);
// Java DAP uses strings on the wire, unlike VS Code's array-valued client settings.
std::string javaDebugArguments(const nlohmann::json &arguments, bool windows);
nlohmann::json javaDebugRequest(const nlohmann::json &profile, const std::filesystem::path &cwd);
nlohmann::json javaDebugTarget(const nlohmann::json &artifact, const std::filesystem::path &workspace,
                               const std::string &javaExecutable);
bool javaDebugAvailable(const nlohmann::json &capabilities);
nlohmann::json javaDebugAttachDefaults();
void validateJavaDebugAttach(const nlohmann::json &);
nlohmann::json javaDebugAttachRequest(const nlohmann::json &profile, const std::filesystem::path &cwd);
// Validate the actual JDT build ordinal and java-debug's custom DAP reply/event.
void validateJavaBuildResult(const nlohmann::json &);
nlohmann::json javaIncrementalBuildParams();
nlohmann::json javaHotReloadResult(const nlohmann::json &);
nlohmann::json javaHotReloadEvent(const nlohmann::json &);
} // namespace cycode

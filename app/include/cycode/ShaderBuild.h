#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
struct ShaderBuildPlan {
    std::string tool, language, stage, format, extension;
    std::vector<std::string> arguments;
};
bool shaderSourceFile(const std::filesystem::path &path);
ShaderBuildPlan shaderBuildPlan(const nlohmann::json &profile, const std::filesystem::path &source,
                                const std::filesystem::path &output,
                                const std::function<std::filesystem::path(const std::string &)> &resolve);
nlohmann::json
shaderBuildDiagnostics(const std::string &text, const std::filesystem::path &source,
                       const std::filesystem::path &workspace, const std::filesystem::path &directory,
                       const std::function<std::filesystem::path(const std::string &)> &resolve);
void validateShaderArtifact(const std::filesystem::path &path, const std::string &format);
} // namespace cycode

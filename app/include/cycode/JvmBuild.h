#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <vector>
namespace cycode {
using JvmResolver = std::function<std::filesystem::path(const std::string &)>;
std::vector<std::string> jvmBuildLauncher(const std::filesystem::path &workspace,
                                          const std::filesystem::path &directory, const std::string &system,
                                          const std::string &java, const JvmResolver &resolve);
std::string gradleModelScript();
nlohmann::json mavenRuntimeModel(const std::string &xml, const std::filesystem::path &directory,
                                 const JvmResolver &resolve);
nlohmann::json gradleRuntimeModels(const std::string &json, const JvmResolver &resolve);
std::string jvmRuntimeClasspath(const nlohmann::json &entries);
std::string jvmArgumentFile(const std::vector<std::string> &arguments);
} // namespace cycode

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
void validateRustToolchain(const std::string &name);
// Plan only: no discovery subprocess, installation, shell or process ownership.
std::vector<std::string> rustToolchainCommand(const std::string &name, const std::string &rustup,
                                              const std::vector<std::string> &command);
nlohmann::json rustToolchainEnvironment(const std::string &name);
} // namespace cycode

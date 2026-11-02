#pragma once
#include <filesystem>
#include <string>
namespace cycode {
std::filesystem::path installationRoot();
// Resolve managed/system tools independently of the project and launching shell.
std::string toolPath(const std::string &name);
} // namespace cycode

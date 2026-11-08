#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace cycode {
std::filesystem::path installationRoot();
// Search absolute PATH entries, then explicit full-path hints, retaining aliases.
// Relative, missing and non-executable (POSIX) candidates are ignored. This does
// not apply managed-tool/environment overrides used by toolPath below.
std::filesystem::path findExecutableOnPath(
    const std::string &name, const std::vector<std::filesystem::path> &hints = {});
// Compare the invocation name and resolved parent directories, not the linked
// binary alone: clang and clang++ can be one file but are different drivers.
bool sameExecutableInvocation(const std::filesystem::path &a, const std::filesystem::path &b);
// Resolve managed/system tools independently of the project and launching shell.
// Preserve executable aliases (clang++, g++, environment interpreters) in argv[0].
std::string toolPath(const std::string &name);
} // namespace cycode

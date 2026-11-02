#pragma once
#include "cycode/Process.h"
#include <nlohmann/json.hpp>
namespace cycode {
// PE/CLI metadata also identifies portable managed assemblies on Linux/macOS.
bool isManagedProgram(const std::filesystem::path &);
bool isRunnableArtifact(const std::filesystem::path &);
std::string debugRuntime(const std::filesystem::path &, const std::string &requested);
struct DebugProgram { std::string runtime; std::vector<std::string> argv; };
DebugProgram debugProgram(const std::filesystem::path &root,const nlohmann::json &profile);
}

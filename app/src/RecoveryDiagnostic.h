#pragma once
#include <filesystem>
#include <string_view>
namespace cycode {
// Explicit, synthetic owned-fixture diagnostic; never an application/MCP operation.
int runOwnedRecoveryDiagnostic(const std::filesystem::path &root, bool filesOnly);
int runOwnedFileOperationDiagnostic(const std::filesystem::path &root, std::string_view operation);
}

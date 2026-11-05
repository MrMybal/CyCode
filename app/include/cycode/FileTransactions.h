#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace cycode {
// Same-volume rename with an exclusive destination. Never falls back to overwrite.
void moveExclusive(const std::filesystem::path &from, const std::filesystem::path &to);
std::string directoryIdentity(const std::filesystem::path &);
// Binary-safe, exclusive creation. A failed write retains its own partial file.
void writeFileExclusive(const std::filesystem::path &, std::string_view);
} // namespace cycode

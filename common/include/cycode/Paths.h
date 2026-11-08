#pragma once
#include <filesystem>
#include <string>
namespace cycode {
std::string pathUtf8(const std::filesystem::path &path);
std::filesystem::path utf8Path(const std::string &text);
}

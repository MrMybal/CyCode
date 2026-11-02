#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
namespace cycode {
struct ProcessResult {
    int exitCode = -1;
    std::string output;
    bool timedOut = false;
    bool truncated = false;
    bool cancelled = false;
};
ProcessResult runProcess(const std::vector<std::string> &argv, const std::filesystem::path &cwd,
                         std::chrono::milliseconds timeout = std::chrono::seconds(15),
                         std::size_t maxOutput = 4 * 1024 * 1024,
                         const std::atomic<bool> *cancellation = nullptr);
std::string pathUtf8(const std::filesystem::path &path);
std::filesystem::path utf8Path(const std::string &text);
} // namespace cycode

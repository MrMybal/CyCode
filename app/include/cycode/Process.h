#pragma once
#include "cycode/Paths.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
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
                         const std::atomic<bool> *cancellation = nullptr,
                         const std::map<std::string,std::string> &environment = {});
} // namespace cycode

#pragma once
#include "cycode/Paths.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace cycode {
using ProcessOutputObserver = std::function<void(std::string_view)>;
// Internal synchronous observation, scoped to one thread and cancellation owner.
// Adapters opt in only for user-facing execution, never structured discovery.
class ProcessOutputScope {
  public:
    ProcessOutputScope(const std::atomic<bool> *, ProcessOutputObserver);
    ~ProcessOutputScope();
    ProcessOutputScope(const ProcessOutputScope &) = delete;
    ProcessOutputScope &operator=(const ProcessOutputScope &) = delete;

  private:
    friend ProcessOutputObserver processOutputObserver(const std::atomic<bool> *);
    const std::atomic<bool> *owner_;
    ProcessOutputObserver observer_;
    ProcessOutputScope *previous_;
};
ProcessOutputObserver processOutputObserver(const std::atomic<bool> *);
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
                         const std::map<std::string, std::string> &environment = {},
                         const ProcessOutputObserver &observer = {});
// Owned execution with explicit removal of ambient variables, for isolated providers.
ProcessResult runProcessFiltered(const std::vector<std::string> &argv, const std::filesystem::path &cwd,
    std::chrono::milliseconds timeout, std::size_t maxOutput, const std::atomic<bool> *cancellation,
    const std::map<std::string,std::string> &environment,
    const std::vector<std::string> &removeEnvironment, const ProcessOutputObserver &observer = {});
} // namespace cycode

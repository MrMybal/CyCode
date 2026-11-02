#pragma once
#include "cycode/Engine.h"
#include <atomic>
#include <filesystem>
#include <thread>
namespace cycode {
// Private local IPC endpoint. CyToolsCore owns MCP parsing, jobs and identities.
class Bridge {
  public:
    Bridge(Engine &, const std::filesystem::path &descriptor);
    ~Bridge();
    Bridge(const Bridge &) = delete;
    Bridge &operator=(const Bridge &) = delete;
    const std::filesystem::path &descriptor() const {
        return descriptor_;
    }

  private:
    void serve();
    Engine &engine_;
    std::filesystem::path descriptor_;
    std::string token_;
    std::intptr_t listener_ = -1;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};
} // namespace cycode

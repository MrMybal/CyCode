#pragma once
#include "cycode/Engine.h"
#include <atomic>
#include <chrono>

namespace cycode {
// A transport-neutral host of ONE existing Engine, not another document model.
// The owner must keep Engine alive until all calls have joined.
class CoreService {
  public:
    static constexpr const char *protocol = "cycode-core/1";
    static constexpr size_t maximumRequestBytes = 8 * 1024 * 1024;
    explicit CoreService(Engine &);
    Json status() const;
    Json respond(const Json &request) noexcept;
    static Json error(const std::string &code, const std::string &message, const Json &id = nullptr);
    void beginShutdown() noexcept;
    bool draining() const { return draining_.load(); }
  private:
    Engine &engine_;
    const std::string context_;
    const std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::atomic<bool> draining_{false};
};
} // namespace cycode

#pragma once
#include "cycode/Process.h"
#include "cycode/TaskOutput.h"
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

namespace cycode {
inline void checkBuildCancellation(const std::atomic<bool> *cancellation) {
    if (cancellation && cancellation->load())
        throw std::runtime_error("BuildCancelled: build operation cancelled");
}
template <class Mutex>
std::unique_lock<Mutex> cancellableBuildLock(Mutex &mutex, const std::atomic<bool> *cancellation) {
    std::unique_lock<Mutex> lock(mutex, std::defer_lock);
    while (!lock.try_lock()) {
        checkBuildCancellation(cancellation);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    checkBuildCancellation(cancellation);
    return lock;
}

// A single native build reservation per Engine, retained until all owned native
// work has returned. Cancellation/status never acquire an adapter's work lock.
class BuildOperations {
  public:
    using Json = nlohmann::json;
    using Work = std::function<Json(const std::atomic<bool> *)>;
    static bool supports(const std::string &method);
    Json execute(const std::string &method, const Json &parameters, const Work &work);
    Json cancel(const Json &parameters = Json::object());
    Json status() const;
    Json output(const Json &parameters = Json::object()) const;

  private:
    bool finish();
    mutable std::mutex stateMutex_;
    std::mutex operationMutex_;
    std::atomic<bool> cancellation_{false};
    std::string operationId_, method_;
    TaskOutput output_;
};
} // namespace cycode

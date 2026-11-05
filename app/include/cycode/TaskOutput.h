#pragma once
#include "cycode/Process.h"
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode {
// Observes a synchronous owner's lifetime; never owns or cancels its process.
class TaskOutput {
  public:
    using Json = nlohmann::json;
    std::string begin(const std::string &method, const std::string &operationId,
                      const Json &context = Json::object());
    void context(const std::string &stream, const Json &);
    void cancelling();
    void finish();
    ProcessOutputObserver observer(const std::string &stream);
    Json read(const Json &parameters = Json::object()) const;

  private:
    void append(const std::string &, std::string_view) noexcept;
    mutable std::mutex mutex_;
    std::string stream_, operationId_, method_;
    Json context_ = Json::object();
    std::deque<std::string> chunks_;
    size_t retained_ = 0;
    uint64_t bytes_ = 0, dropped_ = 0, revision_ = 0;
    bool running_ = false, cancelling_ = false, incomplete_ = false;
};
} // namespace cycode

#pragma once
#include "cycode/Process.h"
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode {
class TestRunner {
  public:
    using Json = nlohmann::json;
    using Resolver = std::function<std::filesystem::path(const std::string &)>;
    TestRunner(std::filesystem::path root, Resolver resolve)
        : root_(std::move(root)), resolve_(std::move(resolve)) {}
    Json discover(const Json &);
    Json run(const Json &);
    Json cancel(const Json &params = Json::object());

  private:
    Json catalog(const std::filesystem::path &, const std::string &, const std::atomic<bool> *);
    std::filesystem::path root_;
    Resolver resolve_;
    std::mutex operation_, state_;
    std::shared_ptr<std::atomic<bool>> cancellation_;
    std::string operationId_;
    Json issued_;
    uint64_t next_ = 1;
};
} // namespace cycode

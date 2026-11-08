#pragma once
#include "cycode/ServiceProtocol.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
namespace cycode {
// Owned loopback JSONL transport. The caller owns models, protocol dispatch,
// permissions and job scheduling; stop() joins handlers before it returns.
class LocalEndpoint {
  public:
    using Json=nlohmann::json;
    using Handler=std::function<Json(const Json &)>;
    struct Options {
        Json descriptor=Json::object();
        Handler respond;
        std::function<bool(const std::string &)> control;
        std::function<Json(const std::string &)> error;
    };
    LocalEndpoint(const std::filesystem::path &descriptor,Options);
    ~LocalEndpoint();
    void stop() noexcept;
    LocalEndpoint(const LocalEndpoint &)=delete;
    LocalEndpoint &operator=(const LocalEndpoint &)=delete;
    static constexpr size_t maximumRequestBytes=maximumNativeRequestBytes;
    static constexpr size_t maximumResponseBytes=maximumNativeResponseBytes;
    static constexpr size_t maximumClients=8;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cycode

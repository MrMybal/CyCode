#pragma once
#include "cycode/Terminal.h"
#include <future>
#include <functional>
#include <atomic>
namespace cycode {
class Debugger {
  public:
    using Json = nlohmann::json;
    using Resolver = std::function<std::filesystem::path(const std::string &)>;
    using JavaStart = std::function<std::future<Json>()>;
    using JavaEpoch = std::function<std::uint64_t()>;
    using JavaBuild = std::function<std::future<Json>(std::shared_ptr<std::atomic<bool>>)>;
    using JavaDocuments = std::function<Json()>;
    using SourceCheck = std::function<void(const Json &)>;
    using ProfileCheck = std::function<void(const Json &, bool launching)>;
    Debugger(std::filesystem::path root, Resolver, Terminals &, JavaStart = {}, JavaEpoch = {}, JavaBuild = {}, JavaDocuments = {}, SourceCheck = {}, ProfileCheck = {});
    ~Debugger();
    Json status() const;
    Json profile() const;
    Json execute(const std::string &, const Json &);
    Json adapters() const;
    void replaceExtensions(const Json &);
    void requestShutdown();
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cycode

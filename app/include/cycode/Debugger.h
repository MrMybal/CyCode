#pragma once
#include "cycode/Terminal.h"
#include <future>
#include <functional>
namespace cycode {
class Debugger {
  public:
    using Json = nlohmann::json;
    using Resolver = std::function<std::filesystem::path(const std::string &)>;
    Debugger(std::filesystem::path root, Resolver, Terminals &);
    ~Debugger();
    Json status() const;
    Json profile() const;
    Json execute(const std::string &, const Json &);
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cycode

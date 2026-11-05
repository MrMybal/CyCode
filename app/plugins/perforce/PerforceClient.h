#pragma once
#include "cycode/Process.h"
#include <functional>
#include <nlohmann/json.hpp>

namespace cycode::perforce {
using Json = nlohmann::json;
using Runner =
    std::function<ProcessResult(const std::vector<std::string> &, const std::filesystem::path &, int)>;
class Client {
  public:
    Client(std::string executable, Runner runner);
    Json run(const std::string &action, const Json &parameters) const;

  private:
    std::string executable_;
    Runner runner_;
};
Json descriptor();
} // namespace cycode::perforce

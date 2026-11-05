#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
nlohmann::json gitReplayRequest(const nlohmann::json &);
nlohmann::json gitReplayState(const nlohmann::json &, const std::string &operationCommit);
} // namespace cycode

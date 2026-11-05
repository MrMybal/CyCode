#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// Complete ls-files --stage -z records, including stage zero, never line-split paths.
nlohmann::json parseGitConflictIndex(const std::string &);
nlohmann::json gitConflictChoices(const nlohmann::json &);
nlohmann::json gitConflictIndexPlan(const nlohmann::json &entries, const nlohmann::json &choices);
} // namespace cycode

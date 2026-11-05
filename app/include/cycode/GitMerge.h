#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
nlohmann::json parseGitMergeHeads(const std::string &);
nlohmann::json parseGitMergeCommitParents(const std::string &);
nlohmann::json parseGitMergeChanges(const std::string &);
nlohmann::json parseGitMergeTree(const std::string &);
nlohmann::json gitMergeRequest(const nlohmann::json &);
} // namespace cycode

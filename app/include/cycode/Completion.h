#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
namespace cycode {
// Plain-text preview of the shared bounded snippet expansion.
std::optional<std::string> plainSnippet(const std::string &source);
// Materialize supported CompletionList defaults without interpreting server commands.
nlohmann::json completionItems(const nlohmann::json &result,bool *truncated=nullptr);
bool completionCommandAllowed(const nlohmann::json &item,const std::string &backend);
struct CompletionEdit { nlohmann::json edits,cursor;std::string after;nlohmann::json snippet; };
CompletionEdit completionEdit(const std::string &before,const nlohmann::json &item,
                              const nlohmann::json &position,bool replace=true);
}

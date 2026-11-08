#pragma once
#include "cycode/CompletionSelection.h"
#include <optional>
#include <filesystem>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
namespace cycode {
// Context is supplied by the owning document, never by a language server item.
// text/byteOffset are borrowed for the duration of a preview call only. Optional
// selection stores ordered UTF-8 byte offsets captured by the native issuer.
struct CompletionSnippetContext {
    std::filesystem::path file, workspace;
    std::string_view text;
    size_t byteOffset = 0;
    std::optional<CompletionSelection> selection;
};
// Context variables are substituted before shared numbered-field expansion.
// A non-null deferredTransforms skips regex evaluation for UI suggestions;
// when set, the returned text is provisional and the UI must display the label.
// selectedTextUsed reports actual variable resolution, excluding unused defaults,
// escaped text and literal choices. Both output flags are reset on entry.
std::optional<std::string> plainSnippet(const std::string &source,
                                      const CompletionSnippetContext &context = {},
                                      bool *deferredTransforms = nullptr,
                                      bool *selectedTextUsed = nullptr);
// Materialize supported CompletionList defaults without interpreting server commands.
nlohmann::json completionItems(const nlohmann::json &result,bool *truncated=nullptr);
bool completionCommandAllowed(const nlohmann::json &item,const std::string &backend);
struct CompletionEdit { nlohmann::json edits,cursor;std::string after;nlohmann::json snippet; };
CompletionEdit completionEdit(const std::string &before,const nlohmann::json &item,
                              const nlohmann::json &position,bool replace=true,
                              const CompletionSnippetContext &context = {});
}

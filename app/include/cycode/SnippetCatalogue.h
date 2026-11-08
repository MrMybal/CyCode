#pragma once
#include <filesystem>
#include <string_view>
#include <nlohmann/json.hpp>
#include "cycode/CompletionVariables.h"

namespace cycode {
inline constexpr std::string_view snippetCataloguePath = ".cycode/snippets.code-snippets";
struct SnippetCatalogueLimits {
    static constexpr size_t bytes = 256 * 1024, definitions = 256, items = 512;
    static constexpr size_t body = 8192, prefixes = 16, prefix = 128, lines = 256;
    static constexpr size_t nodes = 16384, depth = 8;
};
// Capture literal source through the same escaping owner as context variables.
// Caller validates UTF-8. The body includes one final caret and must fit 8 KiB.
constexpr std::optional<completion_detail::PreparedSnippet> captureSnippetBody(std::string_view text) {
    if (text.empty() || text.size()>SnippetCatalogueLimits::body || text.find('\0')!=text.npos) return {};
    completion_detail::PreparedSnippet body;
    if (!body.literal(text) || !body.append("$0",true)) return {};
    return body;
}
struct SnippetRegistryLimits {
    static constexpr size_t catalogues = 32, bytes = 1024 * 1024;
};
constexpr bool snippetRegistryFits(size_t count, size_t bytes) {
    return count <= SnippetRegistryLimits::catalogues && bytes <= SnippetRegistryLimits::bytes;
}
// Only a matching word prefix BEFORE the caret is replaced; a suffix is retained.
constexpr bool snippetWord(unsigned char c) {
    return c >= 128 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '$';
}
constexpr size_t snippetPrefixBytes(std::string_view text, size_t at, std::string_view prefix) {
    if (at > text.size() || prefix.empty() || prefix.size() > SnippetCatalogueLimits::prefix) return 0;
    auto start = at;
    while (start && at - start <= prefix.size() && snippetWord(static_cast<unsigned char>(text[start - 1]))) --start;
    const auto typed = text.substr(start, at - start);
    return !typed.empty() && prefix.starts_with(typed) ? typed.size() : 0;
}
constexpr bool snippetScopeMatches(std::string_view scope, std::string_view language) {
    if (scope.empty()) return true;
    size_t start = 0;
    while (start <= scope.size()) {
        const auto comma = scope.find(',', start);
        auto part = scope.substr(start, comma == scope.npos ? scope.size() - start : comma - start);
        while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.remove_prefix(1);
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.remove_suffix(1);
        if (!part.empty() && part == language) return true;
        if (comma == scope.npos) break;
        start = comma + 1;
    }
    return false;
}
std::string readSnippetCatalogue(const std::filesystem::path &workspaceContainedFile);
std::string snippetLanguage(const std::string &path);
nlohmann::json snippetCatalogueItems(std::string_view source, const std::string &language);
// Declarative ABI2 descriptor, validated with the same grammar as project JSON.
void validateSnippetContribution(const nlohmann::json &);
// Project, personal, then the host's stable active-plugin snapshot. No callbacks.
nlohmann::json mergedSnippetItems(std::string_view workspace, const nlohmann::json &catalogues,
                                 const std::string &language, std::string_view user = "{}");
} // namespace cycode

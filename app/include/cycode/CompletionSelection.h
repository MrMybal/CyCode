#pragma once
#include <cstddef>
#include <optional>
#include <string_view>

namespace cycode {
struct CompletionSelection { std::size_t start=0,end=0; };
inline constexpr std::size_t maxCompletionSelectionBytes=8192;
constexpr bool snippetByteBoundary(std::string_view text,std::size_t at) {
    return at<=text.size() && (at==text.size() || (static_cast<unsigned char>(text[at])&0xc0)!=0x80);
}
// The caller owns the document snapshot. A selection must end at the original
// caret, in either direction, without splitting UTF-8 codepoints.
constexpr std::optional<std::string_view> selectedSnippetText(std::string_view text,std::size_t caret,
                                                            std::optional<CompletionSelection> selection) {
    if (!snippetByteBoundary(text,caret)) return {};
    if (!selection) return std::string_view{};
    const auto [start,end]=*selection;
    if (start>end || end>text.size() || end-start>maxCompletionSelectionBytes ||
        (caret!=start && caret!=end) || !snippetByteBoundary(text,start) || !snippetByteBoundary(text,end)) return {};
    return text.substr(start,end-start);
}
} // namespace cycode

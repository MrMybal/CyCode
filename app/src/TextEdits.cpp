#include "cycode/TextEdits.h"
#include <algorithm>
#include <limits>
namespace cycode {
namespace {
std::pair<std::int64_t, std::int64_t> lineAndCharacter(const Json &position) {
    if (!position.is_object() || !position.contains("line") || !position.contains("character"))
        throw std::runtime_error("Position requires integer line and character fields");
    for (const auto *key : {"line","character"}) {
        const auto &value=position.at(key);
        if (!value.is_number_integer() || (value.is_number_unsigned() &&
            value.get<uint64_t>()>static_cast<uint64_t>(std::numeric_limits<int64_t>::max())))
            throw std::runtime_error("Position requires integer line and character fields");
    }
    const auto line = position.at("line").get<std::int64_t>();
    const auto character = position.at("character").get<std::int64_t>();
    if (line < 0 || character < 0) throw std::runtime_error("Position must be zero-based UTF-16");
    return {line, character};
}
// Advances from a line start by a UTF-16 code-unit count, refusing to cross the
// line end or to stop inside a surrogate pair.
size_t advanceUtf16(const std::string &text, size_t at, std::int64_t character) {
    size_t units = 0;
    while (units < static_cast<size_t>(character)) {
        if (at >= text.size() || text[at] == '\n') throw std::runtime_error("Position is outside line");
        auto c = static_cast<unsigned char>(text[at]);
        units += c >= 0xf0 ? 2 : 1;
        at += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    }
    if (units != static_cast<size_t>(character)) throw std::runtime_error("Position splits a UTF-16 surrogate pair");
    return at;
}
} // namespace
size_t utf16Offset(const std::string &text, const Json &position) {
    const auto [line, character] = lineAndCharacter(position);
    size_t at = 0;
    for (std::int64_t i = 0; i < line; ++i) {
        auto next = text.find('\n', at);
        if (next == std::string::npos) throw std::runtime_error("Position is outside document");
        at = next + 1;
    }
    return advanceUtf16(text, at, character);
}
TextLineIndex::TextLineIndex(const std::string &text) : text_(text) {
    starts_.push_back(0);
    for (size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at + 1))
        starts_.push_back(at + 1);
}
size_t TextLineIndex::offset(const Json &position) const {
    const auto [line, character] = lineAndCharacter(position);
    if (static_cast<std::uint64_t>(line) >= starts_.size()) throw std::runtime_error("Position is outside document");
    return advanceUtf16(text_, starts_[static_cast<size_t>(line)], character);
}
EditedText applyTextEdits(const std::string &text, const Json &edits, const std::string &expectedOldName) {
    if (!edits.is_array() || edits.size() > 50000) throw std::runtime_error("Invalid or excessive text edits");
    struct Edit { size_t start, end; std::string text; Json range, annotation; };
    std::vector<Edit> sorted;
    sorted.reserve(edits.size());
    // One line table for every endpoint: rescanning from the start of the text for
    // each edit made large formatting results O(edits x size).
    const TextLineIndex lines(text);
    for (const auto &edit : edits) {
        const auto &range = edit.at("range");
        auto a = lines.offset(range.at("start")), b = lines.offset(range.at("end"));
        if (b < a) throw std::runtime_error("Text edit has a reversed range");
        const auto replacement = edit.at("newText").get<std::string>();
        if (replacement.find('\0') != std::string::npos) throw std::runtime_error("Binary replacement text");
        if (!expectedOldName.empty() && text.compare(a, b-a, expectedOldName) != 0) throw std::runtime_error("RevisionConflict: rename range no longer names the expected symbol");
        auto annotation=edit.value("annotationId",Json());
        if (edit.contains("annotationId") && (!annotation.is_string() || annotation.get_ref<const std::string &>().size()>512))
            throw std::runtime_error("Invalid text edit annotation ID");
        if (text.compare(a, b-a, replacement) != 0) sorted.push_back({a, b, replacement, range, std::move(annotation)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.start == b.start ? a.end < b.end : a.start < b.start; });
    for (size_t i = 1; i < sorted.size(); ++i)
        if (sorted[i].start < sorted[i-1].end || sorted[i].start == sorted[i-1].start) throw std::runtime_error("Overlapping or ambiguous text edits");
    constexpr size_t limit = 4 * 1024 * 1024;
    EditedText result{std::string{}, Json::array()};
    // Assemble the edited text in one forward pass instead of replacing in place,
    // which moved the whole tail of the text once per edit.
    size_t size = text.size();
    for (const auto &edit : sorted) size = size - (edit.end - edit.start) + edit.text.size();
    if (size > limit) throw std::runtime_error("Edited document exceeds 4 MiB");
    result.text.reserve(size);
    size_t copied = 0;
    for (const auto &edit : sorted) {
        Json value={{"range", edit.range}, {"oldText", text.substr(edit.start, edit.end-edit.start)}, {"newText", edit.text}};
        if (!edit.annotation.is_null()) value["annotationId"]=edit.annotation;
        result.edits.push_back(std::move(value));
        result.text.append(text, copied, edit.start - copied);
        result.text += edit.text;
        copied = edit.end;
    }
    result.text.append(text, copied, std::string::npos);
    (void)Json(result.text).dump();
    return result;
}
bool containsPath(const std::filesystem::path &root, const std::filesystem::path &path) {
    auto a = root.begin(), b = path.begin();
    for (; a != root.end(); ++a, ++b) {
        if (b == path.end()) return false;
#ifdef _WIN32
        // path equality treats / and \ as the same Windows root separator.
        // Keep case-insensitive component comparison for ordinary names.
        if (*a != *b && _wcsicmp(a->c_str(), b->c_str())) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return true;
}
} // namespace cycode

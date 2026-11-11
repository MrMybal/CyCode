#include "cycode/TextEdits.h"
#include <algorithm>
#include <limits>
namespace cycode {
size_t utf16Offset(const std::string &text, const Json &position) {
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
    size_t at = 0;
    for (std::int64_t i = 0; i < line; ++i) {
        auto next = text.find('\n', at);
        if (next == std::string::npos) throw std::runtime_error("Position is outside document");
        at = next + 1;
    }
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
EditedText applyTextEdits(const std::string &text, const Json &edits, const std::string &expectedOldName) {
    if (!edits.is_array() || edits.size() > 50000) throw std::runtime_error("Invalid or excessive text edits");
    struct Edit { size_t start, end; std::string text; Json range, annotation; };
    std::vector<Edit> sorted;
    for (const auto &edit : edits) {
        const auto &range = edit.at("range");
        auto a = utf16Offset(text, range.at("start")), b = utf16Offset(text, range.at("end"));
        if (b < a) throw std::runtime_error("Text edit has a reversed range");
        const auto replacement = edit.at("newText").get<std::string>();
        if (replacement.find('\0') != std::string::npos) throw std::runtime_error("Binary replacement text");
        if (!expectedOldName.empty() && text.substr(a, b-a) != expectedOldName) throw std::runtime_error("RevisionConflict: rename range no longer names the expected symbol");
        auto annotation=edit.value("annotationId",Json());
        if (edit.contains("annotationId") && (!annotation.is_string() || annotation.get_ref<const std::string &>().size()>512))
            throw std::runtime_error("Invalid text edit annotation ID");
        if (text.substr(a, b-a) != replacement) sorted.push_back({a, b, replacement, range, std::move(annotation)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.start == b.start ? a.end < b.end : a.start < b.start; });
    for (size_t i = 1; i < sorted.size(); ++i)
        if (sorted[i].start < sorted[i-1].end || sorted[i].start == sorted[i-1].start) throw std::runtime_error("Overlapping or ambiguous text edits");
    EditedText result{text, Json::array()};
    for (const auto &edit : sorted) {
        Json value={{"range", edit.range}, {"oldText", text.substr(edit.start, edit.end-edit.start)}, {"newText", edit.text}};
        if (!edit.annotation.is_null()) value["annotationId"]=edit.annotation;
        result.edits.push_back(std::move(value));
    }
    for (auto i = sorted.rbegin(); i != sorted.rend(); ++i) {
        result.text.replace(i->start, i->end-i->start, i->text);
        if (result.text.size() > 4 * 1024 * 1024) throw std::runtime_error("Edited document exceeds 4 MiB");
    }
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

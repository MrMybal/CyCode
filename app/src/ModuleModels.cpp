#include "cycode/ModuleModels.h"
#include <stdexcept>

namespace cycode {
cyui::ItemId uiId(const std::string &key) {
    uint64_t id = 14695981039346656037ull;
    for (unsigned char c : key) { id ^= c; id *= 1099511628211ull; }
    return id ? id : 1;
}
void cancelPropertyDrafts(cyui::InspectorState &state) {
    for (auto &[id, draft] : state.drafts) if (draft.active) {
        draft.value = draft.before; draft.active = false;
        draft.cancelled = true; draft.error.clear();
    }
}
void ProblemModel::add(const std::string &key, cyui::LogLevel level,
    const std::string &source, const std::string &message, ProblemLocation location) {
    auto id = uiId(key);
    // Duplicate backend reports are separate rows; changing list order does not
    // change the identity of unrelated diagnostics.
    for (size_t duplicate = 1; locations.contains(id); ++duplicate)
        id = uiId(key + "/duplicate/" + std::to_string(duplicate));
    rows.push_back({id, level, source, message, location.line, location.column});
    locations.emplace(id, std::move(location));
    ++revision_;
}
const std::vector<cyui::Diagnostic> &ProblemModel::matching(const cyui::TextFilter &filter) const {
    if (!filter.active()) return rows;
    if (filteredRevision_ == revision_ && filteredQuery_ == filter.query && filteredLocale_ == cyui::locale()) return filteredRows_;
    filteredRows_.clear();
    for (const auto &row : rows)
        if (filter.matches(row.source, row.source + " " + row.message + " " +
                           std::to_string(row.line) + " " + std::to_string(row.column))) filteredRows_.push_back(row);
    filteredRevision_ = revision_; filteredQuery_ = filter.query; filteredLocale_ = cyui::locale();
    return filteredRows_;
}
void OutputLog::synchronize(std::string &text) {
    constexpr size_t maximum = 8 * 1024 * 1024;
    if (text.size() > maximum) {
        auto cut = text.find('\n', text.size() - maximum);
        if (cut == std::string::npos) {
            cut = text.size() - maximum;
            while (cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xc0) == 0x80) ++cut;
        } else ++cut;
        text.erase(0, cut);
    }
    if (text == seen_) return;
    size_t at = 0;
    if (!seen_.empty() && seen_.back() == '\n' && text.starts_with(seen_)) at = seen_.size();
    else buffer.clear();
    while (at < text.size()) {
        const auto end = text.find('\n', at);
        auto line = text.substr(at, end == std::string::npos ? end : end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto level = cyui::LogLevel::info;
        for (const auto &span : cyui::highlightCode(line, cyui::CodeLanguage::log)) {
            if (span.role == cyui::TokenRole::error) { level = cyui::LogLevel::error; break; }
            if (span.role == cyui::TokenRole::warning) level = cyui::LogLevel::warning;
            if (span.role == cyui::TokenRole::comment) level = cyui::LogLevel::debug;
        }
        buffer.append(level, line);
        if (end == std::string::npos) break;
        at = end + 1;
    }
    seen_ = text;
}
} // namespace cycode

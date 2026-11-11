#include "cycode/CompletionTransforms.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <utf8proc.h>
#include <memory>

namespace cycode::completion_detail {
namespace {
bool appendCodepoint(PreparedSnippet &output, utf8proc_int32_t point) {
    utf8proc_uint8_t bytes[4];
    const auto size = utf8proc_encode_char(point, bytes);
    return size > 0 && output.append({reinterpret_cast<const char *>(bytes), size_t(size)}, true);
}
bool upper(utf8proc_int32_t point) {
    const auto kind = utf8proc_category(point);
    return kind == UTF8PROC_CATEGORY_LU || kind == UTF8PROC_CATEGORY_LT;
}
bool lower(utf8proc_int32_t point) { return utf8proc_category(point) == UTF8PROC_CATEGORY_LL; }
bool word(utf8proc_int32_t point) {
    const auto kind = utf8proc_category(point);
    return (kind >= UTF8PROC_CATEGORY_LU && kind <= UTF8PROC_CATEGORY_LO) ||
           (kind >= UTF8PROC_CATEGORY_MN && kind <= UTF8PROC_CATEGORY_ME) ||
           (kind >= UTF8PROC_CATEGORY_ND && kind <= UTF8PROC_CATEGORY_NO);
}
bool readPoint(std::string_view value, size_t &at, utf8proc_int32_t &point) {
    if (at >= value.size()) return false;
    const auto read = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(value.data() + at),
                                     utf8proc_ssize_t(value.size() - at), &point);
    if (read <= 0) return false;
    at += size_t(read); return true;
}
int callout(pcre2_callout_block *, void *data) {
    auto &budget = *static_cast<TransformBudget *>(data);
    if (++budget.callouts > TransformLimits::callouts) return PCRE2_ERROR_CALLOUT;
    if ((budget.callouts & 127) == 1 && std::chrono::steady_clock::now() >= *budget.deadline)
        return PCRE2_ERROR_CALLOUT;
    return 0;
}
bool beforeDeadline(const TransformBudget &budget) {
    return !budget.deadline || std::chrono::steady_clock::now() < *budget.deadline;
}
}
std::optional<PreparedSnippet> transformSnippetCase(std::string_view value, std::string_view method) {
    if (!transformCase(method) || value.size() > 8192) return {};
    PreparedSnippet output;
    const bool separated = method == "snakecase" || method == "kebabcase";
    const bool words = separated || method == "camelcase" || method == "pascalcase";
    size_t at = 0; bool first = true, previousWord = false;
    utf8proc_int32_t previous = 0;
    while (at < value.size()) {
        utf8proc_int32_t point;
        if (!readPoint(value, at, point)) return {};
        if (!words) {
            const auto mapped = method == "upcase" ? utf8proc_toupper(point)
                              : method == "downcase" ? utf8proc_tolower(point)
                              : first ? utf8proc_totitle(point) : point;
            if (!appendCodepoint(output, mapped)) return {};
            first = false; continue;
        }
        if (!word(point)) { previousWord = false; continue; }
        bool boundary = !previousWord;
        if (previousWord && upper(point)) {
            auto nextAt = at; utf8proc_int32_t next = 0;
            bool nextLower = false;
            while (nextAt < value.size()) {
                if (!readPoint(value, nextAt, next)) return {};
                const auto category = utf8proc_category(next);
                if (category >= UTF8PROC_CATEGORY_MN && category <= UTF8PROC_CATEGORY_ME) continue;
                nextLower = lower(next); break;
            }
            const auto previousCategory = utf8proc_category(previous);
            const bool previousNumber = previousCategory >= UTF8PROC_CATEGORY_ND && previousCategory <= UTF8PROC_CATEGORY_NO;
            boundary = lower(previous) || previousNumber || (upper(previous) && nextLower);
        }
        if (separated && !first && boundary && !output.append(method == "snakecase" ? "_" : "-", true)) return {};
        const auto mapped = separated ? utf8proc_tolower(point)
                          : boundary && (!first || method == "pascalcase") ? utf8proc_totitle(point)
                          : utf8proc_tolower(point);
        if (!appendCodepoint(output, mapped)) return {};
        const auto category = utf8proc_category(point);
        if (category < UTF8PROC_CATEGORY_MN || category > UTF8PROC_CATEGORY_ME) previous = point;
        previousWord = true; first = false;
    }
    return output;
}
std::optional<PreparedSnippet> transformSnippetVariable(std::string_view value, std::string_view pattern,
        std::string_view format, std::string_view options, TransformBudget &budget) {
    if (value.size() > 8192 || pattern.size() > TransformLimits::patternBytes ||
        !transformOptions(options) || !validTransformFormat(format) ||
        ++budget.transformations > TransformLimits::transformations || !beforeDeadline(budget)) return {};
    if (!budget.deadline) budget.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    if (value.empty()) value = "";
    if (pattern.empty()) pattern = "";
    using Compile = std::unique_ptr<pcre2_compile_context, decltype(&pcre2_compile_context_free)>;
    using Pattern = std::unique_ptr<pcre2_code, decltype(&pcre2_code_free)>;
    using Context = std::unique_ptr<pcre2_match_context, decltype(&pcre2_match_context_free)>;
    using Match = std::unique_ptr<pcre2_match_data, decltype(&pcre2_match_data_free)>;
    Compile compile(pcre2_compile_context_create(nullptr), pcre2_compile_context_free);
    if (!compile || pcre2_set_max_pattern_length(compile.get(), TransformLimits::patternBytes) ||
        pcre2_set_max_pattern_compiled_length(compile.get(), 16 * 1024) ||
        pcre2_set_parens_nest_limit(compile.get(), 32)) return {};
    uint32_t flags = PCRE2_UTF | PCRE2_UCP | PCRE2_AUTO_CALLOUT | PCRE2_NEVER_BACKSLASH_C |
                     PCRE2_ALT_BSUX | PCRE2_MATCH_UNSET_BACKREF;
    if (options.find('i') != options.npos) flags |= PCRE2_CASELESS;
    if (options.find('m') != options.npos) flags |= PCRE2_MULTILINE;
    if (options.find('s') != options.npos) flags |= PCRE2_DOTALL;
    int error = 0; PCRE2_SIZE errorAt = 0;
    Pattern code(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), flags,
                              &error, &errorAt, compile.get()), pcre2_code_free);
    uint32_t count = 0;
    if (!code || pcre2_pattern_info(code.get(), PCRE2_INFO_CAPTURECOUNT, &count) ||
        count > TransformLimits::captures || !beforeDeadline(budget)) return {};
    Context context(pcre2_match_context_create(nullptr), pcre2_match_context_free);
    Match match(pcre2_match_data_create_from_pattern(code.get(), nullptr), pcre2_match_data_free);
    if (!context || !match || pcre2_set_match_limit(context.get(), TransformLimits::matchSteps) ||
        pcre2_set_depth_limit(context.get(), TransformLimits::depth) ||
        pcre2_set_heap_limit(context.get(), TransformLimits::heapKiB) ||
        pcre2_set_callout(context.get(), callout, &budget)) return {};
    PreparedSnippet output;
    const bool global = options.find('g') != options.npos;
    size_t offset = 0, copied = 0;
    for (;;) {
        if (++budget.matches > TransformLimits::matchAttempts || !beforeDeadline(budget)) return {};
        const auto result = pcre2_match(code.get(), reinterpret_cast<PCRE2_SPTR>(value.data()),
            value.size(), offset, 0,
            match.get(), context.get());
        if (!beforeDeadline(budget)) return {};
        if (result == PCRE2_ERROR_NOMATCH) break;
        if (result <= 0) return {};
        const auto *positions = pcre2_get_ovector_pointer(match.get());
        const auto start = positions[0], end = positions[1];
        if (start < copied || start < offset || end < start || end > value.size()) return {};
        TransformCaptures captures{};
        for (uint32_t i = 0; i <= count; ++i) {
            const auto a = positions[2 * i], b = positions[2 * i + 1];
            if (a == PCRE2_UNSET || b == PCRE2_UNSET) continue;
            if (a > b || b > value.size()) return {};
            captures[i] = value.substr(a, b - a);
        }
        const auto replacement = formatTransform(format, captures, transformSnippetCase);
        if (!replacement || !output.append(value.substr(copied, start - copied), true) ||
            !output.append(replacement->text(), true)) return {};
        copied = end;
        if (!global) break;
        offset = end;
        if (start == end) {
            if (offset == value.size()) break;
            // JavaScript-style global replacement advances after an empty
            // match; preserve that unmatched codepoint for the next copy.
            utf8proc_int32_t point;
            if (!readPoint(value, offset, point)) return {};
        }
    }
    if (!output.append(value.substr(copied), true)) return {};
    return output;
}
} // namespace cycode::completion_detail

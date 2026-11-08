#pragma once
#include "cycode/CompletionVariables.h"
#include <cstdint>

namespace cycode::completion_detail {
using std::size_t;
struct TransformLimits {
    static constexpr size_t patternBytes = 256, captures = 16, transformations = 8;
    static constexpr size_t matchAttempts = 64, callouts = 100000;
    static constexpr std::uint32_t matchSteps = 20000, depth = 128, heapKiB = 1024;
};
constexpr bool transformCase(std::string_view name) {
    return name == "upcase" || name == "downcase" || name == "capitalize" ||
           name == "camelcase" || name == "pascalcase" || name == "snakecase" || name == "kebabcase";
}
constexpr bool transformOptions(std::string_view options) {
    unsigned seen = 0;
    for (char c : options) {
        const unsigned bit = c == 'g' ? 1 : c == 'i' ? 2 : c == 'm' ? 4 : c == 's' ? 8 : c == 'u' ? 16 : 0;
        if (!bit || (seen & bit)) return false;
        seen |= bit;
    }
    return true;
}
using TransformCaptures = std::array<std::optional<std::string_view>, TransformLimits::captures + 1>;

template<class CaseMapper> struct TransformFormatter {
    std::string_view source;
    const TransformCaptures &captures;
    CaseMapper &mapCase;
    size_t at = 0, nodes = 0;
    PreparedSnippet output;

    constexpr char fragment(unsigned depth, bool nested, bool colon, bool emit) {
        if (depth > 16) return '!';
        while (at < source.size()) {
            const auto start = at;
            const char c = source[at++];
            if (c == '}' && nested) return '}';
            if (c == ':' && colon) return ':';
            if (c == '\\' && at < source.size()) {
                const char next = source[at];
                if (next == '$' || next == '}' || next == '\\' || next == '/' || next == ':') {
                    ++at;
                    if (!output.append(source.substr(at - 1, 1), emit)) return '!';
                    continue;
                }
            }
            if (c != '$') {
                if (!output.append(source.substr(start, 1), emit)) return '!';
                continue;
            }
            if (++nodes > 2048) return '!';
            const bool braced = at < source.size() && source[at] == '{';
            if (braced) ++at;
            if (at == source.size() || !digit(source[at])) return '!';
            size_t index = 0;
            while (at < source.size() && digit(source[at])) {
                index = index * 10 + source[at++] - '0';
                if (index > TransformLimits::captures) return '!';
            }
            const auto value = captures[index].value_or("");
            const bool present = !value.empty();
            if (!braced) {
                if (!output.append(value, emit)) return '!';
                continue;
            }
            if (at == source.size()) return '!';
            const auto kind = source[at++];
            if (kind == '}') {
                if (!output.append(value, emit)) return '!';
            } else if (kind == ':') {
                if (at == source.size()) return '!';
                if (source[at] == '/') {
                    const auto methodStart = ++at;
                    while (at < source.size() && source[at] != '}') ++at;
                    const auto method = source.substr(methodStart, at - methodStart);
                    if (at == source.size() || !transformCase(method)) return '!';
                    ++at;
                    if (emit) {
                        const auto mapped = mapCase(value, method);
                        if (!mapped || !output.append(mapped->text(), true)) return '!';
                    }
                } else if (source[at] == '?') {
                    ++at;
                    if (fragment(depth + 1, true, true, emit && present) != ':' ||
                        fragment(depth + 1, true, false, emit && !present) != '}') return '!';
                } else {
                    const auto mode = source[at];
                    if (mode == '+' || mode == '-') ++at;
                    if (mode != '+' && present && !output.append(value, emit)) return '!';
                    if (fragment(depth + 1, true, false, emit && (mode == '+' ? present : !present)) != '}') return '!';
                }
            } else return '!';
        }
        return nested ? '!' : '\0';
    }
};
template<class CaseMapper>
constexpr std::optional<PreparedSnippet> formatTransform(std::string_view source,
        const TransformCaptures &captures, CaseMapper mapper, bool emit = true) {
    if (source.size() > 8192) return {};
    TransformFormatter<CaseMapper> parser{source, captures, mapper};
    if (parser.fragment(0, false, false, emit) != '\0') return {};
    return parser.output;
}
constexpr bool validTransformFormat(std::string_view source) {
    TransformCaptures captures{};
    return bool(formatTransform(source, captures,
        [](std::string_view, std::string_view) -> std::optional<PreparedSnippet> { return {}; }, false));
}
} // namespace cycode::completion_detail

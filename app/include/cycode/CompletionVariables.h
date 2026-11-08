#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace cycode::completion_detail {
// Application-context substitution only. Numbered fields are passed to the
// shared Snippet library, which still owns their expansion and editor metadata.
struct PreparedSnippet {
    std::array<char, 8192> bytes{};
    std::size_t size = 0;
    constexpr std::string_view text() const { return {bytes.data(), size}; }
    constexpr bool append(std::string_view value, bool emit) {
        if (!emit) return true;
        if (value.size() > bytes.size() - size) return false;
        for (char c : value) bytes[size++] = c;
        return true;
    }
    constexpr bool literal(std::string_view value) {
        auto required = value.size();
        const auto remaining = bytes.size() - size;
        if (required > remaining) return false;
        for (char c : value)
            if ((c == '$' || c == '\\' || c == '}') && ++required > remaining) return false;
        for (char c : value) {
            if ((c == '$' || c == '\\' || c == '}') && !append("\\", true)) return false;
            if (!append({&c, 1}, true)) return false;
        }
        return true;
    }
};
constexpr bool variableStart(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }

struct RejectSnippetTransform {
    constexpr std::optional<PreparedSnippet> operator()(std::string_view, std::string_view,
            std::string_view, std::string_view, bool) const { return {}; }
};
template <class Resolver, class Transformer = RejectSnippetTransform> struct VariableSubstitution {
    std::string_view source;
    Resolver &resolve;
    Transformer transform{};
    std::size_t at = 0, nodes = 0;
    PreparedSnippet output;

    constexpr bool variableTransform(std::string_view name, bool emit) {
        const auto patternStart = at;
        while (at < source.size() && source[at] != '/') {
            if (source[at++] == '\\' && at < source.size()) ++at;
        }
        if (at == source.size()) return false;
        const auto pattern = source.substr(patternStart, at++ - patternStart);
        const auto formatStart = at;
        unsigned nesting = 0;
        while (at < source.size()) {
            const auto c = source[at];
            if (c == '/' && !nesting) break;
            ++at;
            if (c == '\\' && at < source.size()) ++at;
            else if (c == '$' && at < source.size() && source[at] == '{') {
                ++at; if (++nesting > 16) return false;
            } else if (c == '}' && nesting) --nesting;
        }
        if (at == source.size() || nesting) return false;
        const auto format = source.substr(formatStart, at++ - formatStart);
        const auto optionsStart = at;
        while (at < source.size() && source[at] != '}') ++at;
        if (at == source.size()) return false;
        const auto options = source.substr(optionsStart, at++ - optionsStart);
        const auto value = emit ? resolve(name) : std::optional<std::string_view>{};
        const auto changed = transform(value.value_or(""), pattern, format, options, emit);
        return changed && (!emit || output.literal(changed->text()));
    }

    constexpr bool fragment(unsigned depth, bool nested, bool emit) {
        if (depth > 16) return false;
        while (at < source.size()) {
            const auto start = at;
            const char c = source[at++];
            if (c == '}' && nested) return true;
            if (c == '\\') {
                if (at < source.size()) ++at;
                if (!output.append(source.substr(start, at - start), emit)) return false;
                continue;
            }
            if (c != '$') {
                if (!output.append(source.substr(start, 1), emit)) return false;
                continue;
            }
            if (++nodes > 2048) return false;
            const bool braced = at < source.size() && source[at] == '{';
            if (braced) ++at;
            const auto nameStart = at;
            if (at < source.size() && digit(source[at])) {
                while (at < source.size() && digit(source[at])) ++at;
                if (!braced) {
                    if (!output.append(source.substr(start, at - start), emit)) return false;
                    continue;
                }
                if (at == source.size()) return false;
                const auto kind = source[at++];
                if (kind == ':') {
                    if (!output.append(source.substr(start, at - start), emit) ||
                        !fragment(depth + 1, true, emit) || !output.append("}", emit)) return false;
                } else if (kind == '|') {
                    bool closed = false;
                    while (at < source.size()) {
                        const char ch = source[at++];
                        if (ch == '\\' && at < source.size()) ++at;
                        else if (ch == '|' && at < source.size() && source[at] == '}') {
                            ++at; closed = true; break;
                        }
                    }
                    if (!closed || !output.append(source.substr(start, at - start), emit)) return false;
                } else if (kind != '}' || !output.append(source.substr(start, at - start), emit)) return false;
                continue;
            }
            if (at == source.size() || !variableStart(source[at])) return false;
            while (at < source.size() && (variableStart(source[at]) || digit(source[at]))) ++at;
            const auto name = source.substr(nameStart, at - nameStart);
            char kind = 0;
            if (braced) {
                if (at == source.size()) return false;
                kind = source[at++];
                if (kind == '/') {
                    if (!variableTransform(name, emit)) return false;
                    continue;
                }
                if (kind != ':' && kind != '}') return false;
            }
            // Resolve only the selected branch. Values are inserted literally;
            // document text cannot introduce another field or variable.
            const auto value = emit ? resolve(name) : std::optional<std::string_view>{};
            if (emit && value && !value->empty() && !output.literal(*value)) return false;
            if (kind == ':') {
                if (!fragment(depth + 1, true, emit && (!value || value->empty()))) return false;
            } else if (emit && !value) return false;
        }
        return !nested;
    }
};
template <class Resolver, class Transformer = RejectSnippetTransform>
constexpr std::optional<PreparedSnippet> prepareSnippetVariables(std::string_view source, Resolver resolve,
                                                               Transformer transform = {}) {
    if (source.size() > 8192) return {};
    VariableSubstitution<Resolver, Transformer> parser{source, resolve, transform};
    if (!parser.fragment(0, false, true)) return {};
    return parser.output;
}
} // namespace cycode::completion_detail

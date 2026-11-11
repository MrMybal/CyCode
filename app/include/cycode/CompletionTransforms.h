#pragma once
#include "cycode/CompletionTransformFormat.h"
#include <chrono>
namespace cycode::completion_detail {
struct TransformBudget {
    size_t transformations = 0, matches = 0, callouts = 0;
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
// Bounded native PCRE2 matching and Unicode simple casing. No process or JIT.
std::optional<PreparedSnippet> transformSnippetVariable(std::string_view value,
    std::string_view pattern, std::string_view format, std::string_view options, TransformBudget &);
std::optional<PreparedSnippet> transformSnippetCase(std::string_view value, std::string_view method);
}

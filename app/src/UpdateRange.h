#pragma once
#include "cycode/Updates.h"

namespace cycode::updates::detail {
enum class DownloadResponse { reject, receive, restart };
constexpr std::optional<uint64_t> responseNumber(std::string_view text, bool canonical = false) {
    if (text.empty() || (canonical && text.size() > 1 && text.front() == '0'))
        return std::nullopt;
    uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9' || value > (UINT64_MAX - uint64_t(c - '0')) / 10)
            return std::nullopt;
        value = value * 10 + uint64_t(c - '0');
    }
    return value;
}
// This is the production response policy. It is also evaluated by static_assert
// in source-only regression checks; no native test executable is needed.
constexpr DownloadResponse downloadResponsePolicy(unsigned status, std::string_view range,
                                                  std::string_view encoding, std::string_view length,
                                                  uint64_t offset, uint64_t size) {
    if (!size || size > maximumPackage || offset >= size || (!encoding.empty() && encoding != "identity"))
        return DownloadResponse::reject;
    const auto count = responseNumber(length);
    if (!length.empty() && !count)
        return DownloadResponse::reject;
    if (status == 200) {
        if (!range.empty() || (count && *count != size))
            return DownloadResponse::reject;
        return offset ? DownloadResponse::restart : DownloadResponse::receive;
    }
    if (status != 206 || !range.starts_with("bytes "))
        return DownloadResponse::reject;
    range.remove_prefix(6);
    const auto dash = range.find('-'), slash = range.find('/');
    if (dash == range.npos || slash == range.npos || slash <= dash)
        return DownloadResponse::reject;
    const auto first = responseNumber(range.substr(0, dash), true);
    const auto last = responseNumber(range.substr(dash + 1, slash - dash - 1), true);
    const auto total = responseNumber(range.substr(slash + 1), true);
    return first && last && total && *first == offset && *last == size - 1 && *total == size &&
                   (!count || *count == size - offset)
               ? DownloadResponse::receive
               : DownloadResponse::reject;
}
} // namespace cycode::updates::detail

#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace cycode {
namespace detail {
// libc++ can represent filesystem ticks with a signed 128-bit integer. Preserve
// every tick, including the minimum negative value, without a narrowing cast.
template <class Rep> constexpr std::string exactFileTicks(Rep value) {
    const bool negative = value < 0;
    std::string result;
    do {
        auto digit = value % 10;
        result.push_back(char('0' + (digit < 0 ? -digit : digit)));
        value /= 10;
    } while (value != 0);
    if (negative)
        result.push_back('-');
    for (size_t a = 0, b = result.size() - 1; a < b; ++a, --b) {
        const auto item = result[a];
        result[a] = result[b];
        result[b] = item;
    }
    return result;
}
static_assert(exactFileTicks(0) == "0");
static_assert(exactFileTicks(1234567890LL) == "1234567890");
static_assert(exactFileTicks(-1234567890LL) == "-1234567890");
static_assert(exactFileTicks(INT64_MAX) == "9223372036854775807");
static_assert(exactFileTicks(INT64_MIN) == "-9223372036854775808");
static_assert(exactFileTicks(UINT64_MAX) == "18446744073709551615");
#if defined(__SIZEOF_INT128__)
static_assert(exactFileTicks(__int128_t(INT64_MAX) + 1) == "9223372036854775808");
static_assert(exactFileTicks(__int128_t(INT64_MIN) - 1) == "-9223372036854775809");
static_assert(exactFileTicks(__int128_t(1) << 100) == "1267650600228229401496703205376");
static_assert(exactFileTicks(-(__int128_t(1) << 100)) == "-1267650600228229401496703205376");
#endif
} // namespace detail
inline std::string fileTimeToken(std::filesystem::file_time_type value) {
    return detail::exactFileTicks(value.time_since_epoch().count());
}
} // namespace cycode

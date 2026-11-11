#pragma once
#include "cycode/Error.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
namespace cycode {
// JSON Schema integers can be represented as floating numbers. Check their
// range BEFORE conversion; an enormous valid JSON number must never wrap to an
// unrelated byte position. Native file streams use signed 64-bit offsets.
inline uint64_t byteOffset(const nlohmann::json &params, const char *key, uint64_t fallback = 0) {
    if (!params.contains(key))
        return fallback;
    const auto &value = params.at(key);
    if (value.is_number_unsigned()) {
        const auto result = value.get<uint64_t>();
        if (result <= uint64_t(std::numeric_limits<int64_t>::max()))
            return result;
    } else if (value.is_number_integer()) {
        const auto result = value.get<int64_t>();
        if (result >= 0)
            return uint64_t(result);
    } else if (value.is_number_float()) {
        const auto result = value.get<double>();
        if (std::isfinite(result) && result >= 0 && result < std::ldexp(1.0, 63) &&
            std::floor(result) == result)
            return uint64_t(result);
    }
    throw NativeError(ErrorCode::InvalidParams,
                      "Byte offset/length must fit a nonnegative native stream integer");
}
} // namespace cycode

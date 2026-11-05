#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
// Null for methods outside the read-only source-navigation family.
const char *locationRequest(const std::string &method);
using LocationResolver =
    std::function<nlohmann::json(const std::string &, const nlohmann::json &, const nlohmann::json &)>;
// Validated Location/LocationLink responses share one bounded, UI-free catalogue.
nlohmann::json collectLocations(const nlohmann::json &response, size_t limit, const std::string &sourceText,
                                const LocationResolver &);
} // namespace cycode

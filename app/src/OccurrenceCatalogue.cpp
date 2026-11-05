#include "cycode/OccurrenceCatalogue.h"
#include "cycode/Hierarchy.h"
#include "cycode/SymbolCatalogue.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace cycode {
namespace {
using Json = nlohmann::json;
auto point(const Json &value) {
    return std::pair{value.at("line").get<int64_t>(), value.at("character").get<int64_t>()};
}
bool included(const Json &row, unsigned kinds) {
    return (kinds & (1u << (row.at("kind").get<unsigned>() - 1))) != 0;
}
} // namespace
Json collectOccurrences(const Json &response, size_t limit, const std::string &text) {
    if (!response.is_null() && !response.is_array())
        throw std::runtime_error("Invalid document-highlight response");
    limit = std::clamp<size_t>(limit, 1, 4096);
    SymbolTextBounds bounds(text);
    Json rows = Json::array();
    std::map<std::string, size_t> unique;
    size_t invalid = 0, visited = 0, bytes = 2;
    bool truncated = false;
    for (const auto &raw : response) {
        if (++visited > 8192) {
            truncated = true;
            break;
        }
        try {
            const auto range = hierarchyRange(raw.at("range"));
            const auto kind = raw.value("kind", Json(1));
            if (!kind.is_number_integer() || kind < 1 || kind > 3 || !bounds.accepts(range))
                throw std::runtime_error("Invalid highlight range/kind");
            const auto key = range.dump();
            if (const auto found = unique.find(key); found != unique.end()) {
                // Stronger server classification wins for an identical span.
                if (kind > rows.at(found->second).at("kind"))
                    rows.at(found->second)["kind"] = kind;
                continue;
            }
            if (rows.size() == limit) {
                truncated = true;
                break;
            }
            Json row = {{"range", range}, {"kind", kind}};
            const auto size = row.dump().size() + 1;
            if (size > 4 * 1024 * 1024 - bytes) {
                truncated = true;
                break;
            }
            bytes += size;
            unique.emplace(key, rows.size());
            rows.push_back(std::move(row));
        } catch (const std::exception &) {
            ++invalid;
        }
    }
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        const auto &x = a.at("range"), &y = b.at("range");
        return point(x.at("start")) != point(y.at("start")) ? point(x.at("start")) < point(y.at("start"))
                                                            : point(x.at("end")) < point(y.at("end"));
    });
    return {{"occurrences", std::move(rows)}, {"invalid", invalid}, {"truncated", truncated}};
}
int adjacentOccurrence(const Json &rows, const Json &position, bool previous, unsigned kinds) {
    const auto probe = hierarchyRange({{"start", position}, {"end", position}});
    const auto cursor = point(probe.at("start"));
    int first = -1, last = -1, before = -1, after = -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (included(rows.at(i), kinds)) {
            if (first < 0)
                first = static_cast<int>(i);
            last = static_cast<int>(i);
            const auto start = point(rows.at(i).at("range").at("start"));
            if (start < cursor)
                before = static_cast<int>(i);
            if (cursor < start && after < 0)
                after = static_cast<int>(i);
        }
    return previous ? (before >= 0 ? before : last) : (after >= 0 ? after : first);
}
Json occurrenceSelections(const Json &rows, unsigned kinds) {
    Json ranges = Json::array();
    for (const auto &row : rows)
        if (included(row, kinds)) {
            const auto range = hierarchyRange(row.at("range"));
            if (point(range.at("start")) == point(range.at("end")) ||
                (!ranges.empty() && point(range.at("start")) < point(ranges.back().at("end"))))
                throw std::runtime_error("Occurrence selection requires nonempty, disjoint ranges");
            if (ranges.size() == 128)
                throw std::runtime_error("Editor selection limit: 128 occurrences");
            ranges.push_back(range);
        }
    if (ranges.empty())
        throw std::runtime_error("No matching occurrences to select");
    return ranges;
}
} // namespace cycode

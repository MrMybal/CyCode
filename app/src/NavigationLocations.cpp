#include "cycode/NavigationLocations.h"
#include "cycode/Hierarchy.h"
#include "cycode/SymbolCatalogue.h"
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace cycode {
namespace {
using Json = nlohmann::json;
auto point(const Json &p) {
    return std::pair{p.at("line").get<int64_t>(), p.at("character").get<int64_t>()};
}
bool contains(const Json &outer, const Json &inner) {
    return point(outer.at("start")) <= point(inner.at("start")) &&
           point(inner.at("end")) <= point(outer.at("end"));
}
} // namespace
const char *locationRequest(const std::string &method) {
    if (method == "language.definition")
        return "textDocument/definition";
    if (method == "language.declaration")
        return "textDocument/declaration";
    if (method == "language.typeDefinition")
        return "textDocument/typeDefinition";
    if (method == "language.implementation")
        return "textDocument/implementation";
    if (method == "language.references")
        return "textDocument/references";
    return nullptr;
}
Json collectLocations(const Json &response, size_t limit, const std::string &sourceText,
                      const LocationResolver &resolve) {
    if (!response.is_null() && !response.is_object() && !response.is_array())
        throw std::runtime_error("Language server returned an invalid location response");
    const auto singleton = response.is_object() ? Json::array({response}) : Json::array();
    const auto &list = response.is_array() ? response : singleton;
    limit = std::clamp<size_t>(limit, 1, 5000);
    Json rows = Json::array();
    size_t invalid = 0, outside = 0, visited = 0, bytes = 2;
    bool truncated = false;
    std::set<std::string> unique;
    std::optional<SymbolTextBounds> sourceBounds;
    for (const auto &raw : list) {
        if (++visited > 16384 || rows.size() >= limit) {
            truncated = true;
            break;
        }
        try {
            if (!raw.is_object())
                throw std::runtime_error("Invalid location row");
            const bool link = raw.contains("targetUri");
            const auto &uri = raw.at(link ? "targetUri" : "uri").get_ref<const std::string &>();
            if (uri.empty() || uri.size() > 8192 || uri.find('\0') != uri.npos)
                throw std::runtime_error("Invalid location URI");
            const auto selection = hierarchyRange(raw.at(link ? "targetSelectionRange" : "range"));
            const auto extent = link ? hierarchyRange(raw.at("targetRange")) : selection;
            if (!contains(extent, selection))
                throw std::runtime_error("Selection is outside target range");
            Json origin;
            if (link && raw.contains("originSelectionRange")) {
                origin = hierarchyRange(raw.at("originSelectionRange"));
                if (!sourceBounds)
                    sourceBounds.emplace(sourceText);
                if (!sourceBounds->accepts(origin))
                    throw std::runtime_error("Origin is outside source buffer");
            }
            auto row = resolve(uri, selection, extent);
            if (row.is_null()) {
                ++outside;
                continue;
            }
            // The same destination can be reported through multiple links.
            const auto identity = Json::array({row.at("path"), selection}).dump();
            if (unique.contains(identity))
                continue;
            row["fullRange"] = extent;
            if (!origin.is_null())
                row["originRange"] = std::move(origin);
            const auto size = row.dump().size() + 1;
            if (size > 4 * 1024 * 1024 - bytes) {
                truncated = true;
                break;
            }
            bytes += size;
            unique.insert(identity);
            rows.push_back(std::move(row));
        } catch (const std::exception &) {
            ++invalid;
        }
    }
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
        if (a.at("path") != b.at("path"))
            return a.at("path") < b.at("path");
        const auto &x = a.at("range"), &y = b.at("range");
        if (point(x.at("start")) != point(y.at("start")))
            return point(x.at("start")) < point(y.at("start"));
        return point(x.at("end")) < point(y.at("end"));
    });
    return {{"locations", std::move(rows)},
            {"invalid", invalid},
            {"outsideWorkspace", outside},
            {"truncated", truncated}};
}
} // namespace cycode

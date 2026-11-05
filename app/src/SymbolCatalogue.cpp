#include "cycode/SymbolCatalogue.h"
#include "cycode/Hierarchy.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace cycode {
namespace {
using Json = nlohmann::json;
std::pair<int64_t, int64_t> point(const Json &value) {
    return {value.at("line").get<int64_t>(), value.at("character").get<int64_t>()};
}
std::string text(const Json &item, const char *key, size_t maximum, const std::string &fallback = {}) {
    const auto found = item.find(key);
    if (found == item.end())
        return fallback;
    if (!found->is_string())
        throw std::runtime_error("Invalid symbol text");
    const auto &value = found->get_ref<const std::string &>();
    if (value.size() > maximum || value.find('\0') != value.npos)
        throw std::runtime_error("Invalid or oversized symbol text");
    return value;
}
bool type(int kind) {
    return kind == 5 || kind == 10 || kind == 11 || kind == 23 || kind == 26;
}
} // namespace
SymbolTextBounds::SymbolTextBounds(const std::string &text) : lineUnits_(1, 0) {
    for (size_t at = 0; at < text.size();) {
        const auto c = static_cast<unsigned char>(text[at]);
        if (c == '\n') {
            lineUnits_.push_back(0);
            ++at;
        } else if (c >= 0xf0) {
            surrogateSplits_.push_back((uint64_t(lineUnits_.size() - 1) << 32) | (lineUnits_.back() + 1));
            lineUnits_.back() += 2;
            at += 4;
        } else {
            ++lineUnits_.back();
            at += c < 0x80 ? 1 : c < 0xe0 ? 2 : 3;
        }
    }
}
bool SymbolTextBounds::accepts(const Json &range) const {
    const auto normalized = hierarchyRange(range);
    for (const auto *key : {"start", "end"}) {
        const auto [line, character] = point(normalized.at(key));
        if (static_cast<size_t>(line) >= lineUnits_.size() || character > lineUnits_[line] ||
            std::binary_search(surrogateSplits_.begin(), surrogateSplits_.end(),
                               (uint64_t(line) << 32) | uint64_t(character)))
            return false;
    }
    return true;
}
Json collectSymbols(const Json &response, const std::string &documentUri, size_t limit, bool typesOnly,
                    const SymbolResolver &resolve, const PendingSymbolResolver &pending) {
    if (!response.is_array() && !response.is_null())
        throw std::runtime_error("Language server returned an invalid symbol list");
    limit = std::clamp<size_t>(limit, 1, 1000);
    Json symbols = Json::array();
    size_t visited = 0, invalid = 0, outside = 0, bytes = 2;
    bool truncated = false;
    std::map<std::string, uint64_t> unique;
    std::function<void(const Json &, const std::string &, uint64_t, size_t, size_t)> collect;
    collect = [&](const Json &list, const std::string &container, uint64_t parent, size_t depth,
                  size_t nesting) {
        if (!list.is_array()) {
            if (!list.is_null())
                ++invalid;
            return;
        }
        if (nesting > 64) {
            truncated = true;
            return;
        }
        for (const auto &item : list) {
            if (++visited > 8192 || symbols.size() >= limit || bytes >= 4 * 1024 * 1024) {
                truncated = true;
                return;
            }
            std::string name, parentName = container;
            uint64_t node = parent;
            size_t childDepth = depth;
            try {
                if (!item.is_object())
                    throw std::runtime_error("Invalid symbol");
                name = text(item, "name", 1024);
                parentName = text(item, "containerName", 4096, container);
                const auto detail = text(item, "detail", 4096);
                if (name.empty() || !item.at("kind").is_number_integer())
                    throw std::runtime_error("Invalid symbol kind/name");
                const auto kind = item.at("kind").get<int64_t>();
                if (kind < 1 || kind > 26)
                    throw std::runtime_error("Unknown symbol kind");
                const bool hierarchical = item.contains("selectionRange");
                const auto location = item.value("location", Json::object());
                const bool unresolved = !hierarchical && documentUri.empty() && location.is_object() &&
                                        !location.contains("range") && bool(pending);
                Json selection, extent;
                if (!unresolved) {
                    selection =
                        hierarchyRange(hierarchical ? item.at("selectionRange") : location.at("range"));
                    extent = hierarchyRange(hierarchical ? item.at("range") : location.at("range"));
                    if (point(selection.at("start")) < point(extent.at("start")) ||
                        point(extent.at("end")) < point(selection.at("end")))
                        throw std::runtime_error("Symbol selection is outside its range");
                }
                const auto uri = text(location, "uri", 8192, documentUri);
                bool deprecated = item.value("deprecated", false);
                if (item.contains("tags")) {
                    if (!item.at("tags").is_array() || item.at("tags").size() > 32)
                        throw std::runtime_error("Invalid symbol tags");
                    for (const auto &tag : item.at("tags"))
                        if (tag == 1)
                            deprecated = true;
                }
                if (!typesOnly || type(static_cast<int>(kind))) {
                    const auto deferredIdentity = unresolved ? item.dump() : std::string{};
                    const auto duplicate = unresolved ? unique.find(deferredIdentity) : unique.end();
                    if (duplicate != unique.end()) {
                        node = duplicate->second;
                        childDepth = depth + 1;
                    } else {
                        auto target = unresolved ? pending(uri, item) : resolve(uri, selection, extent);
                        if (target.is_null())
                            ++outside;
                        else {
                            target.update({{"name", name},
                                           {"kind", kind},
                                           {"container", parentName},
                                           {"detail", detail},
                                           {"deprecated", deprecated}});
                            if (!unresolved)
                                target["fullRange"] = extent;
                            const auto identity = unresolved ? deferredIdentity : target.dump();
                            const auto found = unique.find(identity);
                            if (found != unique.end())
                                node = found->second;
                            else {
                                node = symbols.size() + 1;
                                target.update({{"node", node}, {"parent", parent}, {"depth", depth}});
                                const auto size = target.dump().size() + 1;
                                if (size > 4 * 1024 * 1024 - bytes) {
                                    truncated = true;
                                    return;
                                }
                                bytes += size;
                                unique.emplace(identity, node);
                                symbols.push_back(std::move(target));
                            }
                            childDepth = depth + 1;
                        }
                    }
                }
            } catch (const std::exception &) {
                ++invalid;
                // Valid descendants can still be represented as roots when a
                // parent row is malformed. Never invent its source location.
            }
            if (item.is_object() && item.contains("children")) {
                const auto next = name.empty()         ? parentName
                                  : parentName.empty() ? name
                                                       : parentName + "::" + name;
                if (next.size() > 4096) {
                    truncated = true;
                    continue;
                }
                collect(item.at("children"), next, node, childDepth, nesting + 1);
            }
            if (visited > 8192 || bytes >= 4 * 1024 * 1024)
                return;
        }
    };
    collect(response, "", 0, 0, 0);
    return {{"symbols", std::move(symbols)},
            {"outsideWorkspace", outside},
            {"invalid", invalid},
            {"truncated", truncated},
            {"positionEncoding", "utf-16"}};
}
uint64_t enclosingSymbol(const Json &symbols, const std::string &path, const Json &position) {
    const auto probe = hierarchyRange({{"start", position}, {"end", position}});
    const auto cursor = point(probe.at("start"));
    uint64_t selected = 0;
    int64_t depth = -1;
    std::pair<int64_t, int64_t> start{}, end{};
    for (const auto &symbol : symbols) {
        if (symbol.at("path") != path)
            continue;
        const auto &range = symbol.at("fullRange");
        const auto a = point(range.at("start")), b = point(range.at("end"));
        const auto candidateDepth = symbol.at("depth").get<int64_t>();
        if (cursor < a || (b <= cursor && !(a == b && cursor == a)))
            continue;
        if (!selected || candidateDepth > depth ||
            (candidateDepth == depth && (a > start || (a == start && b < end)))) {
            selected = symbol.at("node");
            depth = candidateDepth;
            start = a;
            end = b;
        }
    }
    return selected;
}
const char *symbolKindName(int kind) {
    static const char *names[] = {
        "Symbol",   "File",        "Module",      "Namespace", "Package",   "Class",         "Method",
        "Property", "Field",       "Constructor", "Enum",      "Interface", "Function",      "Variable",
        "Constant", "String",      "Number",      "Boolean",   "Array",     "Object",        "Key",
        "Null",     "Enum member", "Struct",      "Event",     "Operator",  "Type parameter"};
    return names[kind >= 1 && kind <= 26 ? kind : 0];
}
} // namespace cycode

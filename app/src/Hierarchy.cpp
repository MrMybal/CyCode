#include "cycode/Hierarchy.h"
#include <cstdint>
#include <stdexcept>
#include <utility>
namespace cycode {
using Json = nlohmann::json;
namespace {
std::pair<int64_t, int64_t> position(const Json &value) {
    const auto line = value.at("line"), character = value.at("character");
    if (!line.is_number_integer() || !character.is_number_integer() || line.get<int64_t>() < 0 ||
        line.get<int64_t>() > INT32_MAX || character.get<int64_t>() < 0 ||
        character.get<int64_t>() > INT32_MAX)
        throw std::runtime_error("Invalid hierarchy UTF-16 position");
    return {line.get<int64_t>(), character.get<int64_t>()};
}
} // namespace
Json hierarchyRange(const Json &value) {
    const auto start = position(value.at("start")), end = position(value.at("end"));
    if (end < start)
        throw std::runtime_error("Reversed hierarchy range");
    return {{"start", {{"line", start.first}, {"character", start.second}}},
            {"end", {{"line", end.first}, {"character", end.second}}}};
}
void validateHierarchyItem(const Json &item) {
    if (!item.is_object() || item.dump().size() > 64 * 1024 || !item.at("name").is_string() ||
        item.at("name").get_ref<const std::string &>().size() > 4096 || !item.at("uri").is_string() ||
        item.at("uri").get_ref<const std::string &>().size() > 8192 ||
        item.at("uri").get_ref<const std::string &>().find('\0') != std::string::npos ||
        !item.at("kind").is_number_integer() || item.at("kind").get<int64_t>() < 1 ||
        item.at("kind").get<int64_t>() > 26)
        throw std::runtime_error("Invalid or oversized hierarchy item");
    if (item.contains("detail") &&
        (!item.at("detail").is_string() || item.at("detail").get_ref<const std::string &>().size() > 16384))
        throw std::runtime_error("Invalid hierarchy detail");
    const auto outer = hierarchyRange(item.at("range")), inner = hierarchyRange(item.at("selectionRange"));
    if (position(inner.at("start")) < position(outer.at("start")) ||
        position(outer.at("end")) < position(inner.at("end")))
        throw std::runtime_error("Hierarchy selection is outside its symbol");
}
std::string hierarchyMethod(const std::string &kind, const std::string &direction) {
    if (kind == "call") {
        if (direction.empty())
            return "textDocument/prepareCallHierarchy";
        if (direction == "incoming")
            return "callHierarchy/incomingCalls";
        if (direction == "outgoing")
            return "callHierarchy/outgoingCalls";
    } else if (kind == "type") {
        if (direction.empty())
            return "textDocument/prepareTypeHierarchy";
        if (direction == "supertypes")
            return "typeHierarchy/supertypes";
        if (direction == "subtypes")
            return "typeHierarchy/subtypes";
    }
    throw std::runtime_error("Unsupported hierarchy kind/direction");
}
} // namespace cycode

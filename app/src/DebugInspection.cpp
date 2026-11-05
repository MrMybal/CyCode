#include "cycode/DebugInspection.h"
#include <set>
#include <stdexcept>
#include <vector>
namespace cycode {
namespace {
using Json = nlohmann::json;
std::string text(const Json &object, const char *key, size_t limit, bool required = false) {
    const auto value = object.value(key, std::string{});
    if ((required && value.empty()) || value.size() > limit || value.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("Invalid debugger ") + key);
    return value;
}
int integer(const Json &object, const char *key, int fallback, int low, int high) {
    const auto value = object.value(key, Json(fallback));
    if (!value.is_number_integer() || value < low || value > high)
        throw std::runtime_error(std::string("Invalid debugger ") + key);
    return value.get<int>();
}
void capability(const Json &caps, const char *key) {
    if (!caps.value(key, false))
        throw std::runtime_error(std::string("Debug adapter does not support ") + key);
}
Json decode(const std::string &encoded, int count) {
    static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (encoded.size() > size_t((count + 2) / 3 * 4) || encoded.size() % 4)
        throw std::runtime_error("Invalid debugger memory base64 length");
    Json bytes = Json::array();
    for (size_t at = 0; at < encoded.size(); at += 4) {
        unsigned value = 0;
        int padding = 0;
        for (int i = 0; i < 4; ++i) {
            const auto c = encoded[at + i];
            if (c == '=') {
                if (i < 2 || at + 4 != encoded.size())
                    throw std::runtime_error("Invalid memory base64 padding");
                ++padding;
                value <<= 6;
            } else {
                const auto index = alphabet.find(c);
                if (padding || index == std::string::npos)
                    throw std::runtime_error("Invalid memory base64 character");
                value = (value << 6) | unsigned(index);
            }
        }
        if ((padding == 1 && (value & 255)) || (padding == 2 && (value & 65535)))
            throw std::runtime_error("Noncanonical memory base64 padding");
        bytes.push_back((value >> 16) & 255);
        if (padding < 2)
            bytes.push_back((value >> 8) & 255);
        if (!padding)
            bytes.push_back(value & 255);
    }
    if (bytes.size() > size_t(count))
        throw std::runtime_error("Debugger memory response exceeds the requested range");
    return bytes;
}
} // namespace
bool stoppedDebugOperation(const std::string &method) {
    static const std::set<std::string> methods = {
        "debug.evaluate", "debug.variables", "debug.source",      "debug.frame",
        "debug.thread",   "debug.continue",  "debug.next",        "debug.stepIn",
        "debug.stepOut",  "debug.memory",    "debug.disassemble", "debug.exceptionInfo",
        "debug.dataBreakpointInfo", "debug.dataBreakpoints", "debug.setVariable", "debug.setExpression", "debug.writeMemory",
        "debug.stepBack", "debug.reverseContinue", "debug.restartFrame", "debug.instructionBreakpoints",
        "debug.stepInTargets", "debug.stepIntoTarget", "debug.gotoTargets", "debug.gotoTarget",
        "debug.runToCursor", "debug.restoreRunToCursor"};
    return methods.contains(method);
}
nlohmann::json debugBreakpointVerification(const Json &point, bool signedId) {
    if (!point.is_object() || !point.contains("verified") || !point.at("verified").is_boolean())
        throw std::runtime_error("Invalid debugger breakpoint verification");
    Json result = {{"verified",point.at("verified")},{"message",text(point,"message",8192)}};
    if (point.contains("id")) result["id"] = integer(point,"id",0,signedId ? (-2147483647-1) : 0,2147483647);
    return result;
}
nlohmann::json debugInspectionArguments(const std::string &method, const Json &p, const Json &caps) {
    if (method == "debug.memory") {
        capability(caps, "supportsReadMemoryRequest");
        return {{"memoryReference", text(p, "memoryReference", 8192, true)},
                {"offset", integer(p, "offset", 0, -1048576, 1048576)},
                {"count", integer(p, "count", 256, 1, 4096)}};
    }
    if (method == "debug.disassemble") {
        capability(caps, "supportsDisassembleRequest");
        return {{"memoryReference", text(p, "memoryReference", 8192, true)},
                {"offset", integer(p, "offset", 0, -1048576, 1048576)},
                {"instructionOffset", integer(p, "instructionOffset", 0, -4096, 4096)},
                {"instructionCount", integer(p, "instructionCount", 64, 1, 200)},
                {"resolveSymbols", true}};
    }
    if (method == "debug.exceptionInfo") {
        capability(caps, "supportsExceptionInfoRequest");
        return {{"threadId", integer(p, "threadId", 0, 1, 2147483647)}};
    }
    if (method == "debug.variables") {
        Json result = {{"variablesReference", integer(p, "variablesReference", 0, 1, 2147483647)},
                       {"start", integer(p, "start", 0, 0, 10000000)},
                       {"count", integer(p, "count", 100, 1, 200)}};
        const auto filter = text(p, "filter", 16);
        if (!filter.empty()) {
            if (filter != "named" && filter != "indexed")
                throw std::runtime_error("Invalid variable page filter");
            result["filter"] = filter;
        }
        return result;
    }
    throw std::runtime_error("Unknown debugger inspection operation");
}
nlohmann::json debugInspectionResult(const std::string &method, const Json &body, const Json &arguments) {
    if (!body.is_object() || body.dump().size() > 1024 * 1024)
        throw std::runtime_error("Invalid or excessive debugger response");
    auto result = body;
    if (method == "debug.memory") {
        result["address"] = text(body, "address", 8192, true);
        result["data"] = text(body, "data", 5500);
        result["bytes"] = decode(result["data"], arguments.at("count"));
        result["unreadableBytes"] = integer(body, "unreadableBytes", 0, 0, arguments.at("count"));
        if (result.at("bytes").size() + result.at("unreadableBytes").get<size_t>() >
            arguments.at("count").get<size_t>())
            throw std::runtime_error("Debugger memory byte counts exceed the requested range");
    } else if (method == "debug.disassemble") {
        if (!body.at("instructions").is_array() ||
            body.at("instructions").size() > arguments.at("instructionCount").get<size_t>())
            throw std::runtime_error("Invalid disassembly instruction count");
        for (const auto &line : body.at("instructions")) {
            (void)text(line, "address", 8192, true);
            (void)text(line, "instruction", 16384, true);
            (void)text(line, "instructionBytes", 4096);
            (void)text(line, "symbol", 8192);
            if (line.contains("presentationHint") && line.at("presentationHint") != "normal" && line.at("presentationHint") != "invalid")
                throw std::runtime_error("Invalid disassembly presentation hint");
            if (line.contains("location") && !line.at("location").is_object())
                throw std::runtime_error("Invalid disassembly source");
        }
    } else if (method == "debug.variables") {
        if (!body.at("variables").is_array() || body.at("variables").size() > 200)
            throw std::runtime_error("Debugger variable page exceeds 200 entries");
        for (const auto &item : body.at("variables")) {
            (void)text(item, "name", 8192, true);
            (void)text(item, "value", 65536);
            (void)integer(item, "variablesReference", 0, 0, 2147483647);
        }
    } else if (method == "debug.exceptionInfo") {
        (void)text(body, "exceptionId", 8192, true);
        (void)text(body, "description", 65536);
        const auto mode = text(body, "breakMode", 32, true);
        if (mode != "never" && mode != "always" && mode != "unhandled" && mode != "userUnhandled")
            throw std::runtime_error("Invalid debugger exception break mode");
    }
    return result;
}
void validateFunctionBreakpoints(const Json &points) {
    if (!points.is_array() || points.size() > 128)
        throw std::runtime_error("Function breakpoint limit is 128");
    std::set<std::string> names;
    for (const auto &point : points) {
        const auto name = text(point, "name", 4096, true);
        if (!names.insert(name).second)
            throw std::runtime_error("Duplicate function breakpoint name");
        if (point.contains("enabled") && !point.at("enabled").is_boolean())
            throw std::runtime_error("Invalid function breakpoint enabled flag");
        (void)text(point, "condition", 4096);
        (void)text(point, "hitCondition", 4096);
    }
}
nlohmann::json functionBreakpointArguments(const Json &points, const Json &caps) {
    validateFunctionBreakpoints(points);
    capability(caps, "supportsFunctionBreakpoints");
    Json enabled = Json::array();
    for (const auto &point : points)
        if (point.value("enabled", true)) {
            Json item = {{"name", point.at("name")}};
            for (const auto &[key, feature] : std::vector<std::pair<std::string, std::string>>{
                     {"condition", "supportsConditionalBreakpoints"},
                     {"hitCondition", "supportsHitConditionalBreakpoints"}}) {
                const auto value = text(point, key.c_str(), 4096);
                if (!value.empty()) {
                    capability(caps, feature.c_str());
                    item[key] = value;
                }
            }
            enabled.push_back(std::move(item));
        }
    return {{"breakpoints", enabled}};
}
void validateExceptionFilters(const Json &filters) {
    if (filters.is_null())
        return;
    if (!filters.is_array() || filters.size() > 64)
        throw std::runtime_error("Exception filter limit is 64");
    std::set<std::string> seen;
    for (const auto &filter : filters) {
        if (!filter.is_string() || filter.get_ref<const std::string &>().empty() ||
            filter.get_ref<const std::string &>().size() > 128 ||
            filter.get_ref<const std::string &>().find('\0') != std::string::npos ||
            !seen.insert(filter.get<std::string>()).second)
            throw std::runtime_error("Invalid or duplicate exception filter");
    }
}
nlohmann::json exceptionBreakpointArguments(const Json &selected, const Json &caps) {
    validateExceptionFilters(selected);
    const auto catalogue = caps.value("exceptionBreakpointFilters", Json::array());
    if (!catalogue.is_array() || catalogue.size() > 64)
        throw std::runtime_error("Invalid adapter exception filter catalogue");
    std::set<std::string> known;
    Json enabled = Json::array();
    for (const auto &filter : catalogue) {
        const auto id = text(filter, "filter", 128, true);
        (void)text(filter, "label", 4096);
        (void)text(filter, "description", 16384);
        if (filter.contains("default") && !filter.at("default").is_boolean())
            throw std::runtime_error("Invalid adapter exception default flag");
        if (!known.insert(id).second)
            throw std::runtime_error("Duplicate adapter exception filter");
        if (selected.is_null() && filter.value("default", false))
            enabled.push_back(id);
    }
    if (!selected.is_null())
        for (const auto &filter : selected) {
            if (!known.contains(filter.get<std::string>()))
                throw std::runtime_error("Selected exception filter is unsupported by this adapter");
            enabled.push_back(filter);
        }
    return {{"filters", enabled}};
}
void validateDebugCapabilities(const Json &caps) {
    if (!caps.is_object() || caps.size() > 256 || caps.dump().size() > 256 * 1024)
        throw std::runtime_error("Invalid or excessive debug adapter capabilities");
    for (const auto &[key, value] : caps.items())
        if (key.starts_with("supports") && !value.is_boolean())
            throw std::runtime_error("Invalid debug adapter capability flag: " + key);
    (void)exceptionBreakpointArguments(nullptr, caps);
}
} // namespace cycode

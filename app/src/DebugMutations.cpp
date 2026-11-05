#include "cycode/DebugMutations.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
std::string text(const Json &value, size_t limit, bool nonempty = false) {
    if (!value.is_string() || (nonempty && value.get_ref<const std::string &>().empty()) ||
        value.get_ref<const std::string &>().size() > limit ||
        value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid debugger mutation text");
    return value.get<std::string>();
}
int integer(const Json &value, int low, int high) {
    if (!value.is_number_integer() || value < low || value > high)
        throw std::runtime_error("Invalid debugger mutation reference or offset");
    return value.get<int>();
}
void capability(const Json &caps, const char *key) {
    if (!caps.value(key, false))
        throw std::runtime_error("Debug adapter does not support " + std::string(key));
}
void bytes(const Json &value) {
    if (!value.is_array() || value.empty() || value.size() > 4096)
        throw std::runtime_error("Debugger memory writes require 1 to 4096 bytes");
    for (const auto &item : value)
        (void)integer(item, 0, 255);
}
std::string base64(const Json &value) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t index = 0; index < value.size(); index += 3) {
        unsigned word = value[index].get<unsigned>() << 16;
        if (index + 1 < value.size())
            word |= value[index + 1].get<unsigned>() << 8;
        if (index + 2 < value.size())
            word |= value[index + 2].get<unsigned>();
        result += alphabet[(word >> 18) & 63];
        result += alphabet[(word >> 12) & 63];
        result += index + 1 < value.size() ? alphabet[(word >> 6) & 63] : '=';
        result += index + 2 < value.size() ? alphabet[word & 63] : '=';
    }
    return result;
}
} // namespace
bool debugMutationOperation(const std::string &method) {
    return method == "debug.setVariable" || method == "debug.setExpression" || method == "debug.writeMemory";
}
Json debugMutationArguments(const std::string &method, const Json &p, const Json &caps, int frame) {
    if (!p.is_object() || !debugMutationOperation(method))
        throw std::runtime_error("Invalid debugger mutation operation");
    for (const auto *key : {"sessionGeneration", "stopGeneration"})
        if (!p.contains(key) || !p.at(key).is_number_integer() ||
            p.at(key) < (std::string(key) == "sessionGeneration" ? 1 : 0))
            throw std::runtime_error(
                "Debugger mutations require observed sessionGeneration and stopGeneration");
    std::set<std::string> fields = {"sessionGeneration", "stopGeneration", "value"};
    if (method == "debug.setVariable") {
        fields.insert("variablesReference");
        fields.insert("name");
    } else if (method == "debug.setExpression") {
        fields.insert("expression");
        fields.insert("frameId");
    } else {
        fields = {"sessionGeneration", "stopGeneration", "memoryReference", "offset", "bytes",
                  "expectedBytes"};
    }
    for (const auto &[key, value] : p.items())
        if (!fields.contains(key))
            throw std::runtime_error("Unknown debugger mutation field: " + key);
    if (method == "debug.writeMemory") {
        capability(caps, "supportsWriteMemoryRequest");
        capability(caps, "supportsReadMemoryRequest");
        bytes(p.at("bytes"));
        bytes(p.at("expectedBytes"));
        if (p.at("bytes").size() != p.at("expectedBytes").size())
            throw std::runtime_error("Expected memory bytes must match the replacement length");
        return {{"memoryReference", text(p.at("memoryReference"), 8192, true)},
                {"offset", integer(p.value("offset", Json(0)), -1048576, 1048576)},
                {"allowPartial", false},
                {"data", base64(p.at("bytes"))}};
    }
    const auto value = text(p.at("value"), 8192);
    if (method == "debug.setVariable") {
        capability(caps, "supportsSetVariable");
        return {{"variablesReference", integer(p.at("variablesReference"), 1, 2147483647)},
                {"name", text(p.at("name"), 4096, true)},
                {"value", value}};
    }
    capability(caps, "supportsSetExpression");
    if (integer(p.at("frameId"), 0, 2147483647) != frame)
        throw std::runtime_error(
            "StaleDebugReference: selected frame changed before assigning the expression");
    return {{"expression", text(p.at("expression"), 4096, true)}, {"value", value}, {"frameId", frame}};
}
Json debugMutationResult(const std::string &method, const Json &body, const Json &p) {
    if (!body.is_object() || body.dump().size() > 128 * 1024)
        throw std::runtime_error("Invalid debugger mutation response");
    if (method == "debug.writeMemory") {
        Json result = Json::object();
        if (body.contains("bytesWritten"))
            result["bytesWritten"] = integer(body.at("bytesWritten"), 0, int(p.at("bytes").size()));
        if (body.contains("offset")) {
            const auto offset = integer(body.at("offset"), -1048576, 1048576);
            if (offset != p.value("offset", 0))
                throw std::runtime_error("Debug adapter reported a different memory write offset");
            result["offset"] = offset;
        }
        return result;
    }
    if (method != "debug.setVariable" && method != "debug.setExpression")
        throw std::runtime_error("Unknown debugger mutation response");
    Json result = {{"value", text(body.at("value"), 65536)}};
    if (body.contains("type"))
        result["type"] = text(body.at("type"), 8192);
    for (const auto *key : {"variablesReference", "namedVariables", "indexedVariables"})
        if (body.contains(key))
            (void)integer(body.at(key), 0, 2147483647);
    if (body.contains("memoryReference"))
        (void)text(body.at("memoryReference"), 8192);
    // References are deliberately omitted: the owner refreshes inspection after
    // every attempted mutation, including failures with potentially partial effects.
    return result;
}
Json debugHexBytes(const std::string &value) {
    if (value.size() > 16384)
        throw std::runtime_error("Memory byte text exceeds 16 KiB");
    Json result = Json::array();
    int high = -1;
    for (const unsigned char character : value) {
        if (character == ' ' || character == '\r' || character == '\n' || character == '\t')
            continue;
        int nibble = character >= '0' && character <= '9'   ? character - '0'
                     : character >= 'a' && character <= 'f' ? character - 'a' + 10
                     : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                                            : -1;
        if (nibble < 0)
            throw std::runtime_error("Enter hexadecimal byte pairs, for example 00 FF 2A");
        if (high < 0)
            high = nibble;
        else {
            result.push_back((high << 4) | nibble);
            high = -1;
        }
        if (result.size() > 4096)
            throw std::runtime_error("Memory write limit is 4096 bytes");
    }
    if (high >= 0)
        throw std::runtime_error("Memory bytes require complete hexadecimal pairs");
    bytes(result);
    return result;
}
std::string debugVariableEditMethod(const Json &variable, const Json &caps) {
    if (!variable.is_object())
        return {};
    const auto hint = variable.value("presentationHint", Json::object());
    if (!hint.is_object())
        return {};
    const auto attributes = hint.value("attributes", Json::array());
    if (!attributes.is_array() ||
        std::find(attributes.begin(), attributes.end(), "readOnly") != attributes.end())
        return {};
    if (caps.value("supportsSetExpression", false) && variable.contains("evaluateName") &&
        variable.at("evaluateName").is_string() &&
        !variable.at("evaluateName").get_ref<const std::string &>().empty())
        return "debug.setExpression";
    if (caps.value("supportsSetVariable", false) && variable.contains("name") &&
        variable.at("name").is_string() && !variable.at("name").get_ref<const std::string &>().empty())
        return "debug.setVariable";
    return {};
}
} // namespace cycode

#include "cycode/DebugAdapters.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
const std::set<std::string> owned = {"request",   "type",        "cwd",         "program",       "args",
                                     "env",       "stopOnEntry", "pid",         "processId",     "noDebug",
                                     "adapterId", "adapter",     "adapterArgs", "adapterOptions", "javaAttach"};
bool text(const Json &value, size_t limit, bool empty = true) {
    return value.is_string() && (empty || !value.get_ref<const std::string &>().empty()) &&
           value.get_ref<const std::string &>().size() <= limit &&
           value.get_ref<const std::string &>().find('\0') == std::string::npos;
}
void bounded(const Json &value, size_t depth = 0) {
    if (depth > 16)
        throw std::runtime_error("Debug adapter JSON exceeds 16 levels");
    if (value.is_string() && !text(value, 8192))
        throw std::runtime_error("Invalid debug adapter string");
    if ((value.is_object() || value.is_array()) && value.size() > 128)
        throw std::runtime_error("Debug adapter collection exceeds 128 entries");
    if (value.is_object())
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key().empty() || it.key().size() > 128 || it.key().find('\0') != std::string::npos)
                throw std::runtime_error("Invalid debug adapter key");
            bounded(it.value(), depth + 1);
        }
    else if (value.is_array())
        for (const auto &entry : value)
            bounded(entry, depth + 1);
}
std::string expand(const std::string &input, const std::string &directory, const std::string &workspace) {
    std::string result;
    for (size_t at = 0; at < input.size();) {
        if (input.compare(at, 12, "${pluginDir}") == 0) {
            result += directory;
            at += 12;
        } else if (input.compare(at, 12, "${workspace}") == 0) {
            result += workspace;
            at += 12;
        } else if (input.compare(at, 2, "${") == 0)
            throw std::runtime_error("Unknown debug adapter argument variable");
        else
            result += input[at++];
        if (result.size() > 8192)
            throw std::runtime_error("Expanded debug adapter argument exceeds 8 KiB");
    }
    return result;
}
} // namespace
void validateDebugAdapterOptions(const Json &value) {
    if (!value.is_object())
        throw std::runtime_error("Debug adapter options must be an object");
    bounded(value);
    if (value.dump().size() > 16384)
        throw std::runtime_error("Debug adapter options exceed 16 KiB");
    for (const auto &key : owned)
        if (value.contains(key))
            throw std::runtime_error("Use the common debug profile field instead of adapter option: " + key);
}
void validateDebugAdapter(const Json &value) {
    if (!value.is_object())
        throw std::runtime_error("Debug adapter descriptor must be an object");
    const std::set<std::string> keys = {"id",       "title",  "type",   "arguments", "environment",
                                        "requests", "launch", "attach", "plugin",    "pidField"};
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!keys.contains(it.key()))
            throw std::runtime_error("Unknown debug adapter descriptor field: " + it.key());
    for (const auto *key : {"id", "title", "type"})
        if (!value.contains(key) || !text(value.at(key), 160, false))
            throw std::runtime_error("Invalid debug adapter text field");
    if (value.contains("plugin") && !text(value.at("plugin"), 160, false))
        throw std::runtime_error("Invalid debug adapter owner");
    const auto pidField = value.value("pidField", std::string("processId"));
    if (pidField != "pid" && pidField != "processId")
        throw std::runtime_error("Debug adapter pidField must be pid or processId");
    const auto &type = value.at("type").get_ref<const std::string &>();
    if (type.size() > 128 ||
        type.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") !=
            std::string::npos)
        throw std::runtime_error("Invalid DAP adapter type");
    const auto &arguments = value.at("arguments");
    if (!arguments.is_array() || arguments.empty() || arguments.size() > 128)
        throw std::runtime_error("Debug adapter requires 1–128 argv elements");
    for (const auto &arg : arguments)
        if (!text(arg, 8192))
            throw std::runtime_error("Invalid debug adapter argument");
    if (!text(arguments[0], 8192, false))
        throw std::runtime_error("Empty debug adapter executable");
    const auto requests = value.value("requests", Json::array({"launch"}));
    if (!requests.is_array() || requests.empty() || requests.size() > 2)
        throw std::runtime_error("Invalid debug adapter request catalogue");
    std::set<std::string> seen;
    for (const auto &request : requests) {
        if ((request != "launch" && request != "attach") || !seen.insert(request.get<std::string>()).second)
            throw std::runtime_error("Invalid or duplicate debug adapter request");
    }
    for (const auto *key : {"launch", "attach"}) {
        const auto options = value.value(key, Json::object());
        validateDebugAdapterOptions(options);
        if (value.contains(key) && !seen.contains(key))
            throw std::runtime_error("Defaults supplied for an unsupported debug adapter request");
    }
    const auto environment = value.value("environment", Json::object());
    if (!environment.is_object() || environment.size() > 64)
        throw std::runtime_error("Invalid debug adapter environment");
    for (auto it = environment.begin(); it != environment.end(); ++it)
        if (it.key().empty() || it.key().size() > 256 ||
            it.key().find_first_of("=\0", 0, 2) != std::string::npos || (!it->is_null() && !text(*it, 8192)))
            throw std::runtime_error("Invalid debug adapter environment entry");
    if (environment.dump().size() > 16384 || value.dump().size() > 65536)
        throw std::runtime_error("Debug adapter environment/descriptor exceeds size limits");
}
Json expandDebugAdapter(Json value, const std::filesystem::path &directory,
                        const std::filesystem::path &workspace) {
    validateDebugAdapter(value);
    for (auto &arg : value["arguments"])
        arg = expand(arg.get<std::string>(), pathUtf8(directory), pathUtf8(workspace));
    if (value.contains("environment"))
        for (auto &entry : value["environment"])
            if (entry.is_string())
                entry = expand(entry.get<std::string>(), pathUtf8(directory), pathUtf8(workspace));
    validateDebugAdapter(value);
    return value;
}
Json debugAdapterSummary(const Json &value) {
    return {{"id", value.at("id")},
            {"title", value.at("title")},
            {"type", value.at("type")},
            {"plugin", value.value("plugin", std::string{})},
            {"requests", value.value("requests", Json::array({"launch"}))}};
}
Json debugAdapterRequest(const Json &definition, const Json &profile, const std::filesystem::path &cwd,
                         bool attach, const Json &pid) {
    const std::string request = attach ? "attach" : "launch";
    const auto supported = definition.value("requests", Json::array({"launch"}));
    if (std::find(supported.begin(), supported.end(), request) == supported.end())
        throw std::runtime_error("Selected debug adapter does not support " + request);
    const auto options = profile.value("adapterOptions", Json::object());
    validateDebugAdapterOptions(options);
    auto result = definition.value(request, Json::object());
    result.update(options);
    result["type"] = definition.at("type");
    result["request"] = request;
    result["cwd"] = pathUtf8(cwd);
    result["noDebug"] = false;
    result["stopOnEntry"] = profile.at("stopOnEntry");
    if (attach) {
        if (!pid.is_number_integer() || pid < 1 || pid > 2147483647)
            throw std::runtime_error("Attach requires a positive process ID");
        result[definition.value("pidField", std::string("processId"))] = pid;
    } else {
        const auto program = profile.value("program", std::string{});
        if (!program.empty()) {
            auto path = utf8Path(program);
            if (!path.is_absolute())
                path = cwd / path;
            if (!std::filesystem::is_regular_file(path))
                throw std::runtime_error("Debug program or script does not exist");
            result["program"] = pathUtf8(std::filesystem::weakly_canonical(path));
        }
        result["args"] = profile.at("args");
        result["env"] = profile.at("env");
    }
    if (!result.contains("terminal"))
        result["terminal"] = profile.at("terminal");
    if (result.dump().size() > 65536)
        throw std::runtime_error("Debug adapter launch request exceeds 64 KiB");
    return result;
}
} // namespace cycode

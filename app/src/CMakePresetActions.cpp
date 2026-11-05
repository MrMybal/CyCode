#include "cycode/CMakePresets.h"
#include "cycode/Recovery.h"
#include <stdexcept>
namespace cycode {
CMakePresets::Resolved CMakePresets::actionContext(const std::string &kind, const std::string &id) {
    const auto family = kind + "Presets";
    if (kind != "build" && kind != "test" && kind != "package")
        throw std::runtime_error("Unsupported CMake action preset family");
    std::set<std::string> chain;
    auto context = resolve(family, id, chain);
    if (context.value.value("hidden", false))
        throw std::runtime_error("Hidden preset cannot be executed");
    const auto configure = context.value.at("configurePreset").get<std::string>();
    if (!presets_["configurePresets"].contains(configure) ||
        !reachable_.at(pathUtf8(context.file))
             .contains(pathUtf8(presets_["configurePresets"].at(configure).file)))
        throw std::runtime_error("Action preset cannot reference this configure preset");
    const auto configured = resolve("configurePresets", configure, chain);
    if (context.value.value("inheritConfigureEnvironment", true)) {
        auto env = context.value.value("environment", Json::object());
        const auto inherited = configured.value.value("environment", Json::object());
        const auto own = presets_[family].at(id).value.value("environment", Json::object());
        if (!env.is_object() || !inherited.is_object() || !own.is_object())
            throw std::runtime_error("Invalid CMake preset environment");
        for (const auto &[key, value] : inherited.items()) {
            env[key] = value;
            context.origins["environment:" + key] = configured.origins.contains("environment:" + key)
                                                        ? configured.origins.at("environment:" + key)
                                                        : configured.file;
        }
        for (const auto &[key, value] : own.items()) {
            env[key] = value;
            context.origins["environment:" + key] = context.file;
        }
        context.value["environment"] = std::move(env);
    }
    context.value["generator"] = configured.value.value("generator", std::string{});
    context.origins["generator"] =
        configured.origins.contains("generator") ? configured.origins.at("generator") : configured.file;
    return context;
}
std::string CMakePresets::revision(const Json &selected) const {
    return RecoveryStore::fingerprint(
        Json{{"files", files_}, {"presence", rootPresence_}, {"selection", selected}}.dump());
}
CMakePresets::Json CMakePresets::action(const std::string &kind, const std::string &id) {
    if (id.empty() || id.size() > 160 || id.find_first_of("\r\n") != std::string::npos ||
        id.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid CMake action preset name");
    if (kind == "workflow") {
        const auto found = presets_["workflowPresets"].find(id);
        if (found == presets_["workflowPresets"].end())
            throw std::runtime_error("Unknown workflow preset");
        const auto &workflow = found->second;
        if (workflow.value.contains("inherits") || workflow.value.contains("hidden"))
            throw std::runtime_error("Workflow presets do not support inheritance or hidden");
        const auto steps = workflow.value.at("steps");
        if (!steps.is_array() || steps.empty() || steps.size() > 64)
            throw std::runtime_error("Workflow needs 1 to 64 explicit steps");
        Json result;
        Json flattened = Json::array();
        std::string configure;
        for (size_t i = 0; i < steps.size(); ++i) {
            const auto type = steps.at(i).at("type").get<std::string>();
            const auto name = steps.at(i).at("name").get<std::string>();
            if ((i == 0 && type != "configure") ||
                (i && type != "build" && type != "test" && type != "package"))
                throw std::runtime_error("A workflow starts with configure, then build/test/package steps");
            const auto family = type + "Presets";
            if (!presets_[family].contains(name) ||
                !reachable_.at(pathUtf8(workflow.file)).contains(pathUtf8(presets_[family].at(name).file)))
                throw std::runtime_error("Workflow references an unavailable preset");
            Json entry;
            if (!i) {
                entry = selection(name, "");
                configure = name;
                result = entry;
                entry["outputs"] = Json::array();
                entry["configurations"] = entry.at("configuration") == ""
                                              ? Json::array()
                                              : Json::array({entry.at("configuration")});
            } else
                entry = action(type, name);
            if (entry.at("configurePreset") != configure)
                throw std::runtime_error("All workflow steps must use the initial configure preset");
            if (type == "build" && entry.at("configuration") != "")
                result["configuration"] = entry.at("configuration");
            flattened.push_back({{"type", type},
                                 {"name", name},
                                 {"configurePreset", configure},
                                 {"buildDirectory", entry.at("buildDirectory")},
                                 {"configuration", entry.at("configuration")},
                                 {"configurations", entry.at("configurations")},
                                 {"outputs", entry.at("outputs")}});
        }
        result.erase("buildPreset");
        result["kind"] = kind;
        result["preset"] = id;
        result["steps"] = std::move(flattened);
        result["outputs"] = Json::array();
        result["program"] = "cmake";
        result["arguments"] = Json::array({"--workflow", "--preset", id});
        result["minimumCMake"] = "3.25";
        result["configurations"] =
            result.at("configuration") == "" ? Json::array() : Json::array({result.at("configuration")});
        return result;
    }
    auto context = actionContext(kind, id);
    const auto configure = context.value.at("configurePreset").get<std::string>();
    auto result = selection(configure, kind == "build" ? id : "");
    std::set<std::string> chain;
    if (context.value.contains("configuration"))
        result["configuration"] = expand(
            context.value.at("configuration"), context,
            context.origins.contains("configuration") ? context.origins.at("configuration") : context.file,
            chain);
    const auto build = resolver_(result.at("buildDirectory"));
    Json configurations = Json::array();
    if (kind == "package" && context.value.contains("configurations")) {
        const auto &values = context.value.at("configurations");
        if (!values.is_array() || values.size() > 64)
            throw std::runtime_error("Too many CPack configurations");
        for (const auto &value : values)
            configurations.push_back(expand(value.get<std::string>(), context,
                                            context.origins.contains("configurations")
                                                ? context.origins.at("configurations")
                                                : context.file,
                                            chain));
    } else if (result.at("configuration") != "")
        configurations.push_back(result.at("configuration"));
    Json outputs = Json::array();
    auto checkedPath = [&](const Json &raw, const std::string &originKey) {
        auto path =
            expand(raw.get<std::string>(), context,
                   context.origins.contains(originKey) ? context.origins.at(originKey) : context.file, chain);
        if (path.empty())
            throw std::runtime_error("Empty CMake preset output/config path");
        return pathUtf8(resolver_(pathUtf8(build / utf8Path(path))));
    };
    if (kind == "test" && context.value.contains("output"))
        for (const auto *field : {"outputLogFile", "outputJUnitFile"})
            if (context.value.at("output").contains(field))
                outputs.push_back(
                    checkedPath(context.value.at("output").at(field), std::string("output:") + field));
    if (kind == "package") {
        if (context.value.contains("configFile"))
            (void)checkedPath(context.value.at("configFile"), "configFile");
        if (context.value.contains("packageDirectory"))
            outputs.push_back(checkedPath(context.value.at("packageDirectory"), "packageDirectory"));
        const auto variables = context.value.value("variables", Json::object());
        if (!variables.is_object())
            throw std::runtime_error("Invalid CPack preset variables");
        if (variables.contains("CPACK_PACKAGE_DIRECTORY"))
            outputs.push_back(
                checkedPath(variables.at("CPACK_PACKAGE_DIRECTORY"), "variables:CPACK_PACKAGE_DIRECTORY"));
    }
    result.erase("buildPreset");
    result["kind"] = kind;
    result["preset"] = id;
    result["outputs"] = outputs;
    result["configurations"] = configurations;
    result["steps"] = Json::array({{{"type", kind},
                                    {"name", id},
                                    {"configurePreset", configure},
                                    {"buildDirectory", result.at("buildDirectory")},
                                    {"configuration", result.at("configuration")},
                                    {"configurations", configurations},
                                    {"outputs", outputs}}});
    result["program"] = kind == "test" ? "ctest" : kind == "package" ? "cpack" : "cmake";
    result["arguments"] =
        kind == "build" ? Json::array({"--build", "--preset", id}) : Json::array({"--preset", id});
    result["minimumCMake"] = kind == "package" ? "3.25" : "3.20";
    return result;
}
} // namespace cycode

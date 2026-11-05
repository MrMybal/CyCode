#include "cycode/UnrealLaunch.h"
#include "cycode/Process.h"
#include "cycode/UnrealAutomation.h"
#include "cycode/UnrealProject.h"
#include <algorithm>
#include <map>
#include <set>
namespace cycode {
using Json = nlohmann::json;
namespace fs = std::filesystem;
fs::path unrealEditorExecutable(const fs::path &engine, int major, const std::string &platform,
                                const std::string &configuration) {
    if ((major != 4 && major != 5) || (configuration != "Development" && configuration != "DebugGame"))
        throw std::runtime_error("Choose UE4/UE5 with Development or DebugGame editor configuration");
    const std::string base = major == 4 ? "UE4Editor" : "UnrealEditor";
    // DebugGame's launch module selects its game manifest at compile time. A
    // Development editor plus a flag is not an equivalent executable.
    const auto name = configuration == "Development" ? base : base + "-" + platform + "-DebugGame";
    if (platform == "Win64")
        return engine / "Engine/Binaries/Win64" / (name + ".exe");
    if (platform == "Mac") {
        const auto primary = engine / "Engine/Binaries/Mac" / (name + ".app") / "Contents/MacOS" / name;
        const auto sharedBundle = engine / "Engine/Binaries/Mac" / (base + ".app") / "Contents/MacOS" / name;
        if (!fs::is_regular_file(primary) && fs::is_regular_file(sharedBundle))
            return sharedBundle;
        return primary;
    }
    if (platform == "Linux" || platform == "LinuxArm64")
        return engine / "Engine/Binaries/Linux" / name;
    throw std::runtime_error("Unsupported Unreal editor host platform");
}
Json unrealLaunchRequest(const Json &params) {
    const auto mode = params.value("mode", std::string("editor"));
    if (mode != "editor" && mode != "game" && mode != "server")
        throw std::runtime_error("Choose Unreal editor, standalone game or uncooked server");
    const auto map = params.value("map", std::string{});
    if (!map.empty())
        validateUnrealMapPackages(Json::array({map}));
    const bool listen = params.value("listen", false);
    if (listen && (mode != "game" || map.empty()))
        throw std::runtime_error("Listen server requires game mode and an explicit /Game map");
    auto integer = [&](const char *key, int fallback, int minimum, int maximum) {
        if (params.contains(key) &&
            (!params.at(key).is_number_integer() || params.at(key) < minimum || params.at(key) > maximum))
            throw std::runtime_error("Invalid Unreal launch integer: " + std::string(key));
        return params.value(key, fallback);
    };
    const auto width = integer("width", 1280, 320, 16384), height = integer("height", 720, 200, 16384);
    const auto port = integer("port", 7777, 1, 65535);
    const bool windowed = params.value("windowed", true);
    return {{"mode", mode},
            {"map", map},
            {"listen", listen},
            {"log", params.value("log", false)},
            {"windowed", mode == "game" && windowed},
            {"width", width},
            {"height", height},
            {"port", port},
            {"stopOnEntry", params.value("stopOnEntry", false)},
            {"buildBeforeLaunch", params.value("buildBeforeLaunch", true)}};
}
Json unrealBoundLaunchParameters(const Json &profile) {
    const auto binding = profile.value("unrealLaunch", Json());
    if (binding.is_null())
        return Json();
    if (!binding.is_object() || binding.size() != 2 || !binding.contains("profile") ||
        !binding.contains("request"))
        throw std::runtime_error("Invalid Unreal launch binding");
    const auto &build = binding.at("profile");
    if (!build.is_object() || build.size() != 5)
        throw std::runtime_error("Invalid Unreal build profile");
    for (const auto *key : {"project", "engineRoot", "target", "platform", "configuration"}) {
        const auto text = build.at(key).get<std::string>();
        if (text.size() > 4096 || (text.empty() && std::string(key) != "target") ||
            std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("Invalid bound Unreal build field");
    }
    if (!std::set<std::string>{"Win64", "Linux", "LinuxArm64", "Mac"}.contains(build.at("platform")) ||
        !std::set<std::string>{"Development", "DebugGame"}.contains(build.at("configuration")))
        throw std::runtime_error("Unsupported bound Unreal host/configuration");
    const auto normalized = unrealLaunchRequest(binding.at("request"));
    if (normalized != binding.at("request"))
        throw std::runtime_error("Unreal launch binding must contain normalized options");
    auto params = build;
    params.update(normalized);
    params["stopOnEntry"] = profile.at("stopOnEntry");
    params["buildBeforeLaunch"] = profile.at("buildBeforeLaunch");
    return params;
}
void validateUnrealDebugDocuments(const Json &before, const Json &after) {
    std::map<std::string, const Json *> current;
    for (const auto &doc : after) {
        if (doc.value("dirty", false) || !doc.value("diskConflict", std::string{}).empty())
            throw std::runtime_error("Save or reconcile workspace documents before Unreal debugging");
        if (!current.emplace(doc.at("path").get<std::string>(), &doc).second)
            throw std::runtime_error("Duplicate document reference in Unreal debug preparation");
    }
    for (const auto &doc : before) {
        const auto found = current.find(doc.at("path").get<std::string>());
        if (found == current.end() || found->second->at("id") != doc.at("id") ||
            found->second->at("revision") != doc.at("revision"))
            throw std::runtime_error("Documents changed during the Unreal build; start debugging again");
    }
}
Json unrealDebugContinuation(const Json &snapshot, const Json &draft, const Json &saved, const Json &result,
                             const std::string &operationId) {
    if (snapshot.at("operationId") != operationId || operationId.empty() || snapshot.at("draft") != draft ||
        snapshot.at("expectedProfile") != saved)
        throw std::runtime_error(
            "DebugProfileConflict: launch settings changed during the Unreal build; start debugging again");
    if (!result.at("exitCode").is_number_integer() || result.at("exitCode") != 0 ||
        result.at("cancelled").get<bool>() || result.at("timedOut").get<bool>() ||
        result.at("truncated").get<bool>() || result.contains("inspectionError") ||
        result.contains("receiptError"))
        throw std::runtime_error(
            "Unreal build did not complete with a confirmed successful result; debugger was not started");
    const auto &parameters = snapshot.at("parameters");
    const auto context = unrealBoundLaunchParameters(parameters);
    if (context.is_null() || !result.contains("project") || !result.at("project").value("available", false) ||
        result.at("project").value("profile", Json::object()) != parameters.at("unrealLaunch").at("profile"))
        throw std::runtime_error("Unreal build result does not match the reviewed launch profile");
    if (!result.contains("artifacts") || !result.at("artifacts").value("ready",false) ||
        !result.at("artifacts").value("complete",false) ||
        result.at("artifacts").value("profile",Json::object()) != parameters.at("unrealLaunch").at("profile"))
        throw std::runtime_error("Unreal Editor artifacts are missing, mismatched or unverified; inspect build outputs before debugging");
    auto launch = parameters;
    launch["expectedProfile"] = snapshot.at("expectedProfile");
    return launch;
}
Json unrealLaunchSettings(const Json &params, const fs::path &project, const fs::path &editor,
                          const std::string &workingDirectory) {
    const auto request = unrealLaunchRequest(params);
    for (const auto &text : {pathUtf8(project), pathUtf8(editor), workingDirectory})
        if (text.empty() || text.size() > 4096 ||
            std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("Unreal launch path exceeds the debug profile bound");
    const auto configuration = params.value("configuration", std::string("Development"));
    if (configuration != "Development" && configuration != "DebugGame")
        throw std::runtime_error(
            "Editor-based Unreal launch requires Development or DebugGame configuration");
    Json args = Json::array({pathUtf8(project)});
    auto map = request.at("map").get<std::string>();
    if (request.at("listen").get<bool>())
        map += "?listen";
    if (!map.empty())
        args.push_back(map);
    const auto mode = request.at("mode").get<std::string>();
    if (mode == "game") {
        args.push_back("-game");
        args.push_back(request.at("windowed").get<bool>() ? "-windowed" : "-fullscreen");
        args.push_back("-ResX=" + std::to_string(request.at("width").get<int>()));
        args.push_back("-ResY=" + std::to_string(request.at("height").get<int>()));
    } else if (mode == "server")
        args.push_back("-server");
    if (mode == "server" || request.at("listen").get<bool>())
        args.push_back("-port=" + std::to_string(request.at("port").get<int>()));
    if (request.at("log").get<bool>())
        args.push_back("-log");
    return {{"name", "Unreal / " + mode},
            {"program", pathUtf8(editor)},
            {"cwd", workingDirectory},
            {"args", args},
            {"runtime", "native"},
            {"adapterId", "builtin"},
            {"adapter", "automatic"},
            {"adapterArgs", Json::array()},
            {"adapterOptions", Json::object()},
            {"stopOnEntry", request.at("stopOnEntry")},
            {"buildBeforeLaunch", request.at("buildBeforeLaunch")}};
}
UnrealProject::Json UnrealProject::launchProfile(const Json &params) {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        throw std::runtime_error("Wait for the current Unreal operation before preparing a launch profile");
    const auto model = inspectLocked(params);
    if (!model.value("available", false))
        throw std::runtime_error("Open an Unreal project before preparing a launch profile");
    const auto profile = model.at("profile");
    const auto major = model.at("engineVersion").value("MajorVersion", 0);
    if (major != 4 && major != 5)
        throw std::runtime_error("Choose an installed Unreal Engine 4 or 5 editor");
    const auto engine = utf8Path(profile.at("engineRoot"));
#ifdef _WIN32
    const char *host = "Win64";
#elif defined(__APPLE__)
    const char *host = "Mac";
#else
#if defined(__aarch64__)
    const char *host = "LinuxArm64";
#else
    const char *host = "Linux";
#endif
#endif
    if (profile.at("platform") != host)
        throw std::runtime_error("Unreal editor launch must use the current host platform");
    const auto editor = unrealEditorExecutable(engine, major, host, profile.at("configuration"));
    if (!fs::is_regular_file(editor))
        throw std::runtime_error("The selected Unreal editor executable/configuration is missing: " +
                                 pathUtf8(editor));
    auto input = params;
    input["configuration"] = profile.at("configuration");
    const auto request = unrealLaunchRequest(input);
    if (request.at("buildBeforeLaunch").get<bool>()) {
        bool known = false;
        for (const auto &target : model.at("targets"))
            known |= target.at("name") == profile.at("target") &&
                     target.at("name").get<std::string>().ends_with("Editor");
        if (!known)
            throw std::runtime_error("Build before launch requires a discovered Editor target; disable it "
                                     "for content-only projects");
    }
    const auto project = resolve_(profile.at("project"));
    const auto map = request.at("map").get<std::string>();
    if (!map.empty()) {
        const auto path =
            resolve_(pathUtf8(project.parent_path() / "Content" / utf8Path(map.substr(6) + ".umap")));
        if (!fs::is_regular_file(path))
            throw std::runtime_error("The explicit Unreal map is missing from project Content");
    }
    auto settings = unrealLaunchSettings(input, project, fs::canonical(editor),
                                         pathUtf8(project.parent_path().lexically_relative(root_)));
    settings["unrealLaunch"] = {{"profile", profile}, {"request", request}};
    return {{"request", request},          {"settings", settings},   {"unrealProfile", profile},
            {"usesUncookedContent", true}, {"startsProcess", false}, {"savesProfile", false}};
}
void UnrealProject::validateLaunchProfile(const Json &profile) {
    const auto params = unrealBoundLaunchParameters(profile);
    if (params.is_null())
        return;
    const auto expected = launchProfile(params).at("settings");
    for (const auto *key : {"program", "cwd", "args"})
        if (profile.at(key) != expected.at(key))
            throw std::runtime_error("UnrealLaunchConflict: regenerate the Unreal profile or explicitly "
                                     "detach its settings before editing program, cwd or arguments");
    if (profile.at("runtime") != "native" || profile.at("adapterId") == "builtin.java")
        throw std::runtime_error("A bound Unreal launch requires a native debug adapter/runtime");
}
} // namespace cycode

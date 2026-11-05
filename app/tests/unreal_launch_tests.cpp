#include "cycode/Engine.h"
#include "cycode/UnrealLaunch.h"
#include "cycode/UnrealProject.h"
#include <algorithm>
#include <fstream>
namespace cycode {
using Json = nlohmann::json;
void unrealLaunchContracts(const std::filesystem::path &root) {
    namespace fs = std::filesystem;
    auto require = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto rejects = [&](auto call) {
        bool caught = false;
        try {
            call();
        } catch (const std::exception &) {
            caught = true;
        }
        require(caught, "Expected Unreal launch rejection");
    };
    auto settings = unrealLaunchSettings(Json::object(), root / "Game.uproject", root / "Editor", ".");
    require(settings.at("args").size() == 1 && settings.at("adapterId") == "builtin" &&
                settings.at("runtime") == "native",
            "Editor profile uses the shared native debugger");
    Json request = {
        {"mode", "game"}, {"map", "/Game/Maps/One"}, {"listen", true}, {"port", 7788},
        {"log", true},    {"width", 1920},           {"height", 1080}, {"configuration", "DebugGame"}};
    settings =
        unrealLaunchSettings(request, root / "project with spaces/Game.uproject", root / "Editor", ".");
    auto has = [&](const char *text) {
        return std::find(settings.at("args").begin(), settings.at("args").end(), Json(text)) !=
               settings.at("args").end();
    };
    require(settings.at("args").at(0) == pathUtf8(root / "project with spaces/Game.uproject") &&
                has("/Game/Maps/One?listen") && has("-game") && has("-windowed") && has("-port=7788") &&
                has("-ResX=1920") && has("-ResY=1080") && has("-log") && !has("-debug") && !has("-server"),
            "Game settings retain literal argv, typed listen/resolution and DebugGame");
    request["mode"] = "server";
    request["listen"] = false;
    settings = unrealLaunchSettings(request, root / "Game.uproject", root / "Editor", ".");
    require(has("-server") && !has("-game") && !has("-windowed") && !has("-ResX=1920") && has("-port=7788"),
            "Server uses uncooked editor mode without game viewport arguments");
    for (const auto &patch : Json::array(
             {Json{{"mode", "PIE"}}, Json{{"mode", "server"}, {"listen", true}},
              Json{{"mode", "game"}, {"listen", true}, {"map", ""}}, Json{{"map", "/Game/Map?listen"}},
              Json{{"map", "/Game/../outside"}}, Json{{"port", true}}, Json{{"port", 65536}},
              Json{{"width", 0}}, Json{{"height", 16385}}}))
        rejects([&] {
            auto input = request;
            input.update(patch);
            unrealLaunchRequest(input);
        });
    rejects([&] {
        unrealLaunchSettings({{"configuration", "Shipping"}}, root / "Game.uproject", root / "Editor", ".");
    });
    rejects([&] {
        unrealLaunchSettings(Json::object(), root / "Game.uproject", root / "Editor", std::string(4097, 'a'));
    });

    const auto workspace = root / "launch-project", engine = root / "launch-engine";
    auto write = [](const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    };
    write(workspace / "Game.uproject", "{}");
    write(workspace / "Source/GameEditor.Target.cs", "// fixture editor rule\n");
    write(workspace / "Content/Maps/One.umap", "fixture map");
    write(engine / "Engine/Build/Build.version", "{\"MajorVersion\":5}");
#ifdef _WIN32
    const auto editor = engine / "Engine/Binaries/Win64/UnrealEditor.exe";
    const char *host = "Win64";
#elif defined(__APPLE__)
    const auto editor = engine / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor";
    const char *host = "Mac";
#else
    const auto editor = engine / "Engine/Binaries/Linux/UnrealEditor";
#if defined(__aarch64__)
    const char *host = "LinuxArm64";
#else
    const char *host = "Linux";
#endif
#endif
    // Empty metadata fixture only; this source never dispatches a process.
    write(editor, "");
    UnrealProject unreal(workspace, workspace / "Game.uproject", Json::object(),
                         [&](const std::string &path) {
                             const auto p = utf8Path(path);
                             return p.is_absolute() ? p : workspace / p;
                         });
    Json input = {{"engineRoot", pathUtf8(engine)},
                  {"project", "Game.uproject"},
                  {"target", "GameEditor"},
                  {"platform", host},
                  {"map", "/Game/Maps/One"}};
    const auto preview = unreal.launchProfile(input);
    require(preview.at("startsProcess") == false && preview.at("savesProfile") == false &&
                !fs::exists(workspace / ".cycode"),
            "Unreal launch preview reads only and creates no profile/cache");
    auto bound = preview.at("settings");
    require(bound.at("unrealLaunch").at("profile") == preview.at("unrealProfile"),
            "Launch saves its exact build context");
    unreal.validateLaunchProfile(bound);
    auto changed = bound;
    changed["args"].push_back("-other");
    rejects([&] { unreal.validateLaunchProfile(changed); });
    changed = bound;
    changed["runtime"] = "dotnet";
    rejects([&] { unreal.validateLaunchProfile(changed); });
    changed = bound;
    changed["unrealLaunch"]["request"]["extra"] = true;
    rejects([&] { unrealBoundLaunchParameters(changed); });
    changed = bound;
    changed["unrealLaunch"] = nullptr;
    require(unrealBoundLaunchParameters(changed).is_null(), "Explicit null detaches the Unreal binding");
    Json persisted;
    {
        Engine first(workspace);
        persisted = first.dispatch("debug.configure", bound);
        auto invalid = bound;
        invalid["program"] = "wrong-editor";
        rejects([&] { first.dispatch("debug.configure", invalid); });
        require(first.dispatch("debug.profile") == persisted,
                "A mismatched bound program cannot overwrite the profile");
    }
    {
        Engine reopened(workspace);
        require(reopened.dispatch("debug.profile").at("unrealLaunch") == bound.at("unrealLaunch"),
                "Unreal binding survives a native profile reload");
        auto detached =
            reopened.dispatch("debug.configure", {{"unrealLaunch", nullptr}, {"program", "manual-program"}});
        require(detached.at("unrealLaunch").is_null(),
                "Detached profiles retain manual configuration support");
    }
    Json snapshot = {{"operationId", "build-reviewed"},
                     {"draft", bound},
                     {"parameters", bound},
                     {"expectedProfile", persisted}};
    Json completed = {{"exitCode", 0},
                      {"cancelled", false},
                      {"timedOut", false},
                      {"truncated", false},
                      {"project", {{"available", true}, {"profile", preview.at("unrealProfile")}}}};
    completed["artifacts"] = {{"ready", true}, {"complete", true}, {"profile", preview.at("unrealProfile")}};
    auto launch = unrealDebugContinuation(snapshot, bound, persisted, completed, "build-reviewed");
    require(launch.at("program") == bound.at("program") && launch.at("expectedProfile") == persisted,
            "Confirmed continuation retains the captured launch parameters and save guard");
    for (const auto *flag : {"cancelled", "timedOut", "truncated"}) {
        auto failed = completed;
        failed[flag] = true;
        rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, failed, "build-reviewed"); });
    }
    auto failed = completed;
    failed.erase("artifacts");
    rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, failed, "build-reviewed"); });
    failed = completed;
    failed["artifacts"]["ready"] = false;
    rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, failed, "build-reviewed"); });
    failed = completed;
    failed["project"]["profile"]["configuration"] = "DebugGame";
    rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, failed, "build-reviewed"); });
    failed = completed;
    failed["inspectionError"] = "unavailable";
    rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, failed, "build-reviewed"); });
    changed = bound;
    changed["env"] = {{"CHANGED", "yes"}};
    rejects([&] { unrealDebugContinuation(snapshot, changed, persisted, completed, "build-reviewed"); });
    changed = persisted;
    changed["name"] = "Changed through SDK";
    rejects([&] { unrealDebugContinuation(snapshot, bound, changed, completed, "build-reviewed"); });
    rejects([&] { unrealDebugContinuation(snapshot, bound, persisted, completed, "another-build"); });
    Json before = Json::array(
        {{{"path", "Source/One.h"}, {"id", 1}, {"revision", 3}, {"dirty", false}, {"diskConflict", ""}}});
    auto after = before;
    validateUnrealDebugDocuments(before, after);
    after.push_back(
        {{"path", "Source/Other.h"}, {"id", 2}, {"revision", 4}, {"dirty", false}, {"diskConflict", ""}});
    validateUnrealDebugDocuments(before, after); // Opening an unrelated clean document is allowed.
    after[0]["revision"] = 4;
    rejects([&] { validateUnrealDebugDocuments(before, after); }); // Saved edits still change revision.
    rejects([&] { validateUnrealDebugDocuments(before, Json::array()); });
    after = before;
    after[0]["dirty"] = true;
    rejects([&] { validateUnrealDebugDocuments(before, after); });
    after = before;
    after[0]["diskConflict"] = "modified";
    rejects([&] { validateUnrealDebugDocuments(before, after); });
    input["map"] = "/Game/Maps/Missing";
    rejects([&] { unreal.launchProfile(input); });
    input["map"] = "";
    input["target"] = "Game";
    rejects([&] { unreal.launchProfile(input); });
    input["buildBeforeLaunch"] = false;
    unreal.launchProfile(input);
    input["platform"] = "Android";
    rejects([&] { unreal.launchProfile(input); });
    input["platform"] = host;
    input["configuration"] = "DebugGame";
    rejects([&] { unreal.launchProfile(input); });
    const auto debugEditor = unrealEditorExecutable(engine, 5, host, "DebugGame");
    write(debugEditor, "");
    require(unreal.launchProfile(input).at("settings").at("program") == pathUtf8(fs::canonical(debugEditor)),
            "DebugGame selects its installed executable rather than a Development fallback");
    require(unrealEditorExecutable(engine, 4, "Win64", "DebugGame").filename() ==
                "UE4Editor-Win64-DebugGame.exe",
            "UE4 configuration-specific editor name");
    input["configuration"] = "Development";
    fs::remove(editor);
    rejects([&] { unreal.launchProfile(input); });

    // Configuration assertions do not launch a debug session. Opt-in native suite only.
    const auto configRoot = root / "launch-profile-guard";
    fs::create_directories(configRoot);
    Engine core(configRoot);
    const auto previous = core.dispatch("debug.profile");
    auto configured =
        core.dispatch("debug.configure", {{"expectedProfile", previous}, {"name", "Reviewed Unreal"}});
    require(configured.at("name") == "Reviewed Unreal", "A matching profile is persisted");
    rejects([&] {
        core.dispatch("debug.configure", {{"expectedProfile", previous}, {"name", "Stale overwrite"}});
    });
    require(core.dispatch("debug.profile") == configured,
            "A stale configuration cannot replace the current profile");
}
} // namespace cycode

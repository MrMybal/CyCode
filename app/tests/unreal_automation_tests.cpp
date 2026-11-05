#include "cycode/BuildOperations.h"
#include "cycode/UnrealAutomation.h"
#include "cycode/UnrealProject.h"
#include <algorithm>
#include <fstream>
namespace cycode {
using Json = nlohmann::json;
void unrealAutomationContracts(const std::filesystem::path &root) {
    namespace fs = std::filesystem;
    auto require = [](bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    };
    auto rejects = [&](const std::function<void()> &call) {
        bool caught = false;
        try {
            call();
        } catch (const std::exception &) {
            caught = true;
        }
        require(caught, "Expected Unreal automation guard rejection");
    };
    Json request = {{"project", "Game.uproject"},
                    {"engineRoot", "installed-engine"},
                    {"target", "Game"},
                    {"platform", "Win64"},
                    {"configuration", "Development"},
                    {"action", "package"},
                    {"maps", Json::array({"/Game/Maps/One", "/Game/Maps/Two"})},
                    {"ioStore", true},
                    {"pak", true},
                    {"compressed", true},
                    {"timeoutSeconds", 3600}};
    auto value = unrealAutomationRequest(request);
    const auto args =
        unrealAutomationArguments(value, root / "Game.uproject", root / "engine/Editor", root / "output");
    auto has = [&](const char *arg) { return std::find(args.begin(), args.end(), arg) != args.end(); };
    require(has("BuildCookRun") && has("-cook") && has("-stage") && has("-package") && has("-archive") &&
                has("-iostore") && has("-compressed") && has("-map=/Game/Maps/One+/Game/Maps/Two") &&
                !has("-run") && !has("-deploy") && !has("-clean"),
            "Typed package arguments preserve phases and avoid deployment/cleanup");
    auto cook = request;
    cook["action"] = "cook";
    const auto cooked = unrealAutomationRequest(cook);
    require(!cooked.at("pak").get<bool>() && !cooked.at("ioStore").get<bool>() &&
                !cooked.at("compressed").get<bool>(),
            "Cook omits package-only flags");
    auto loose = request;
    loose["pak"] = false;
    loose["ioStore"] = false;
    loose["compressed"] = false;
    const auto off =
        unrealAutomationArguments(loose, root / "Game.uproject", root / "engine/Editor", root / "output");
    require(std::find(off.begin(), off.end(), "-skipiostore") != off.end() &&
                std::find(off.begin(), off.end(), "-ForceUncompressed") != off.end() &&
                std::find(off.begin(), off.end(), "-pak") == off.end(),
            "Disabled container/compression options are explicit");
    for (const auto *map : {"/Game/../escape", "/Game//Map", "/Game/Map.umap", "/Game/Map+-run",
                            "/Engine/Map", "/Game/Map?listen"}) {
        auto bad = request;
        bad["maps"] = Json::array({map});
        rejects([&] { unrealAutomationRequest(bad); });
    }
    auto bad = request;
    bad["maps"] = Json::array({"/Game/Map", "/Game/map"});
    rejects([&] { unrealAutomationRequest(bad); });
    bad = request;
    bad["target"] = "GameEditor";
    rejects([&] { unrealAutomationRequest(bad); });
    bad = request;
    bad["target"] = "-clean";
    rejects([&] { unrealAutomationRequest(bad); });
    bad = request;
    bad["platform"] = "Android";
    rejects([&] { unrealAutomationRequest(bad); });
    bad = request;
    bad["pak"] = false;
    rejects([&] { unrealAutomationRequest(bad); });
    bad = request;
    bad["timeoutSeconds"] = 0;
    rejects([&] { unrealAutomationRequest(bad); });
    require(BuildOperations::supports("unreal.automationRun") &&
                !BuildOperations::supports("unreal.automationPlan"),
            "Only execution reserves cancellable build ownership");
    // Source-only fixture: this target remains opt-in. No fake tool is ever executed.
    const auto workspace = root / "uat-project", engine = root / "uat-engine";
    auto write = [](const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    };
    write(workspace / "Game.uproject", "{\"EngineAssociation\":\"5.3\"}");
    write(workspace / "Source/Game.Target.cs", "// Game target\n");
    write(workspace / "Content/Maps/One.umap", "fixture asset");
    write(workspace / "Content/Maps/Two.umap", "fixture asset");
    write(engine / "Engine/Build/Build.version", "{\"MajorVersion\":5,\"MinorVersion\":3}");
    write(engine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll", "");
    write(engine / "Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll", "");
#ifdef _WIN32
    write(engine / "Engine/Binaries/ThirdParty/DotNet/fixture/windows/dotnet.exe", "");
    write(engine / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe", "");
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    write(engine / "Engine/Binaries/ThirdParty/DotNet/fixture/mac-arm64/dotnet", "");
#else
    write(engine / "Engine/Binaries/ThirdParty/DotNet/fixture/mac-x64/dotnet", "");
#endif
    write(engine / "Engine/Binaries/Mac/UnrealEditor-Cmd", "");
#else
    write(engine / "Engine/Binaries/ThirdParty/DotNet/fixture/linux/dotnet", "");
    write(engine / "Engine/Binaries/Linux/UnrealEditor-Cmd", "");
#endif
    UnrealProject project(workspace, workspace / "Game.uproject", Json::object(),
                          [&](const std::string &path) {
                              const auto p = utf8Path(path);
                              return p.is_absolute() ? p : workspace / p;
                          });
    request["engineRoot"] = pathUtf8(engine);
    const auto plan = project.automationPlan(request);
    require(!fs::exists(workspace / ".cycode"), "Preview does not create managed output");
    std::atomic<bool> cancelled{true};
    const Json run = {
        {"planId", plan.at("id")}, {"expectedRevision", plan.at("revision")}, {"timeoutSeconds", 3600}};
    rejects([&] { project.automationRun(run, &cancelled); });
    write(workspace / "Source/Game.Target.cs", "// changed rule\n");
    rejects([&] { project.automationRun(run); });
    const auto fresh = project.automationPlan(request);
    auto occupied = workspace / utf8Path(fresh.at("outputDirectory"));
    fs::create_directories(occupied);
    write(occupied / "retained.txt", "preserve");
    rejects([&] {
        project.automationRun({{"planId", fresh.at("id")},
                               {"expectedRevision", fresh.at("revision")},
                               {"timeoutSeconds", 3600}});
    });
    require(fs::is_regular_file(occupied / "retained.txt"), "Occupied output survives rejection");
}
} // namespace cycode

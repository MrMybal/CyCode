#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void write(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}
template <class F> static void rejects(F fn) {
    bool failed = false;
    try {
        fn();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Expected rejection");
}
int main() {
    auto root =
        fs::temp_directory_path() /
        ("cycode-solution-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try {
        auto workspace = root / utf8Path("projet é");
        fs::create_directory(workspace);
        write(workspace / "Source/main.cpp", "int value = 3;\n");
        write(workspace / "Game.vcxproj", "<Project/>\n");
        write(root / "outside.csproj", "<Project/>\n");
        const std::string solution = R"(Microsoft Visual Studio Solution File, Format Version 12.00
Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "Games", "Games", "{11111111-1111-1111-1111-111111111111}"
EndProject
Project("{BC8A1FFA-BEE3-4634-8014-F334798102B3}") = "Game", "Game.vcxproj", "{22222222-2222-2222-2222-222222222222}"
EndProject
Project("{BC8A1FFA-BEE3-4634-8014-F334798102B3}") = "Missing", "Intermediate\Game.vcxproj", "{33333333-3333-3333-3333-333333333333}"
EndProject
Project("{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}") = "External", "..\outside.csproj", "{44444444-4444-4444-4444-444444444444}"
EndProject
Global
GlobalSection(SolutionConfigurationPlatforms) = preSolution
  Development Editor|Win64 = Development Editor|Win64
EndGlobalSection
GlobalSection(NestedProjects) = preSolution
  {22222222-2222-2222-2222-222222222222} = {11111111-1111-1111-1111-111111111111}
EndGlobalSection
EndGlobal
)";
        write(workspace / "Game.sln", "\xef\xbb\xbf" + solution);
        Engine engine(workspace / "Game.sln");
        auto desc = engine.dispatch("workspace.describe");
        check(desc.at("root") == pathUtf8(fs::canonical(workspace)), "Solution workspace root");
        const auto model = desc.at("solution");
        check(model.at("projects").size() == 4 && model.at("missingProjects") == 1 &&
                  model.at("externalProjects") == 1,
              "Project references");
        check(model.at("projects")[1].at("parent") == "{11111111-1111-1111-1111-111111111111}",
              "Nested projects");
        check(model.at("configurations")[0] == "Development Editor|Win64", "Solution configurations");
        check(engine.dispatch("documents.read", {{"path", "Source/main.cpp"}}).at("text") ==
                  "int value = 3;\n",
              "Sources available without generated projects");
        rejects([&] { engine.dispatch("documents.read", {{"path", "../outside.csproj"}}); });
        check(!fs::exists(workspace / "Intermediate") && !fs::exists(workspace / ".cycode"),
              "Inspection must not generate project files");
        write(workspace / "utf16.sln", "\xff\xfe");
        {
            std::ofstream out(workspace / "utf16.sln", std::ios::binary | std::ios::app);
            for (auto c : solution) {
                out.put(c);
                out.put(0);
            }
        }
        Engine utf16(workspace / "utf16.sln");
        check(utf16.dispatch("workspace.describe").at("solution").at("projects").size() == 4,
              "UTF-16 solution");
        write(workspace / "bad.sln", "invalid\n");
        rejects([&] { Engine bad(workspace / "bad.sln"); });
        auto cycle = solution;
        auto offset = cycle.find("EndGlobalSection\nEndGlobal");
        cycle.insert(offset,
                     "{11111111-1111-1111-1111-111111111111} = {11111111-1111-1111-1111-111111111111}\n");
        write(workspace / "cycle.sln", cycle);
        rejects([&] { Engine bad(workspace / "cycle.sln"); });
        write(
            workspace / "Game.uproject",
            R"({"FileVersion":3,"EngineAssociation":"missing-test-engine","Modules":[{"Name":"Game","Type":"Runtime"}]})");
        write(workspace / "Source/Game.Target.cs", "// Game target rules\n");
        write(workspace / "Source/GameEditor.Target.cs", "// Editor target rules\n");
        write(workspace / "Source/Game/Game.Build.cs", "// module rules\n");
        write(workspace / "Plugins/Example/Example.uplugin", "{}\n");
        Engine unreal(workspace / "Game.uproject");
        auto ue = unreal.dispatch("unreal.inspect");
        check(ue.at("available") && ue.at("targets").size() == 2 && ue.at("modules").size() == 1 &&
                  ue.at("plugins").size() == 1,
              "Unreal discovery");
        check(ue.at("profile").at("target") == "GameEditor", "Editor target selected by default");
        rejects([&] { unreal.dispatch("unreal.inspect", {{"project", "../outside.uproject"}}); });
        rejects(
            [&] { unreal.dispatch("unreal.inspect", {{"engineRoot", pathUtf8(root / "not-an-engine")}}); });
        rejects([&] { unreal.dispatch("unreal.build", {{"engineRoot", pathUtf8(root / "not-an-engine")}}); });
        check(!fs::exists(workspace / ".cycode"), "Inspection must not save a build profile");
        auto fakeEngine = root / "engine";
        write(fakeEngine / "Engine/Build/Build.version", "{}");
        write(fakeEngine / "Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll", "not executable");
        for (const auto &[key, value] : std::vector<std::pair<std::string, std::string>>{
                 {"target", "-Project=outside"}, {"platform", "Unknown"}, {"configuration", "Release"}}) {
            rejects([&] {
                unreal.dispatch("unreal.build", {{"engineRoot", pathUtf8(fakeEngine)}, {key, value}});
            });
        }
        std::cout << "Solution/Unreal inspection and confinement passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << " (fixture: " << pathUtf8(root) << ")\n";
        return 1;
    }
    fs::remove_all(root);
    return 0;
}

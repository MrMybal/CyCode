#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() /
                        utf8Path("cycode-unreal-plugin-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected Unreal plugin library");
        std::filesystem::create_directories(folder / "extensions");
        std::filesystem::create_directories(folder / "workspace/Source/Demo");
        const auto library = utf8Path(argv[1]);
        std::filesystem::copy_file(library, folder / "extensions" / library.filename());
        std::ofstream(folder / "extensions/unreal.json") << Json({{"id", "cycode.unreal"},
                                                                  {"abi", 2},
                                                                  {"version", "1.0.0"},
                                                                  {"library", pathUtf8(library.stem())},
                                                                  {"permissions", {"process.execute"}}})
                                                                .dump();
        std::ofstream(folder / "workspace/Demo.uproject")
            << R"({"FileVersion":3,"Modules":[{"Name":"Demo","Type":"Runtime"}]})";
        std::ofstream(folder / "workspace/Source/DemoEditor.Target.cs") << "// target\n";
        std::ofstream(folder / "workspace/Source/Demo/Demo.Build.cs") << "// module\n";
        {
            Engine engine(folder / "workspace/Demo.uproject");
            engine.plugins().load(folder / "extensions");
            const auto inventory = engine.dispatch("plugins.list");
            require(inventory.at("debugAdapters").size()==1&&inventory.at("debugAdapters")[0].at("id")=="cycode.unreal.lldb", "Unreal extension omitted its native DAP contribution");
            const auto adapters=engine.dispatch("debug.adapters");
            require(adapters.at("adapters").size()==2,"Contributed DAP did not reach the shared native service");
            engine.dispatch("debug.configure",{{"adapterId","cycode.unreal.lldb"},{"adapterOptions",{{"sourceMap",Json::object()}}}});
            require(inventory.at("projects").size() == 1 && inventory.at("menus").size() == 5,
                    "Official Unreal extension did not contribute its views");
            const auto project = engine.dispatch(
                "plugins.project", {{"provider", "cycode.unreal.project"}, {"action", "inspect"}});
            require(project.at("available").get<bool>() && project.at("modules").size() == 1,
                    "Unreal provider did not inspect actual project metadata");
            const auto panel = engine.plugins().invoke("cycode.unreal.overview", {{"event", "read"}});
            require(panel.at("properties").size() == 5 && panel.at("rows").size() == 4,
                    "Unreal extension panel omitted targets or modules");
            require(panel.at("rows")[0].at("name")=="Last workspace event" && panel.at("rows")[1].at("name")=="Observed events",
                    "Unreal extension omitted event state");
            const auto changed = engine.plugins().invoke("cycode.unreal.overview",
                                                         {{"event", "commit"},
                                                          {"property", "configuration"},
                                                          {"value", "DebugGame"},
                                                          {"expectedRevision", panel.at("revision")}});
            require(changed.at("revision") != panel.at("revision"),
                    "Unreal extension property transaction did not advance its revision");
            bool staleRejected = false;
            try {
                engine.plugins().invoke("cycode.unreal.overview",
                                        {{"event", "commit"},
                                         {"property", "configuration"},
                                         {"value", "Development"},
                                         {"expectedRevision", panel.at("revision")}});
            } catch (const std::exception &) {
                staleRejected = true;
            }
            require(staleRejected, "Unreal extension accepted a stale edit");
            const auto templatePlan =
                engine.dispatch("templates.preview", {{"template", "cycode.unreal.cppGame"},
                                                      {"name", "GeneratedGame"},
                                                      {"options", {{"engineAssociation", "5.3"}}}});
            require(templatePlan.at("plugin") == "cycode.unreal" && templatePlan.at("files").size() == 9,
                    "Unreal declarative template is missing");
            engine.dispatch("templates.create", {{"id", templatePlan.at("id")}});
            {
                Engine generated(folder / "workspace/GeneratedGame/GeneratedGame.uproject");
                const auto model = generated.dispatch("unreal.inspect");
                require(model.at("available").get<bool>() && model.at("modules").size() == 1 &&
                            model.at("targets").size() == 2,
                        "Generated Unreal project was not recognized with its runtime module and targets");
            }
            const auto pending = engine.dispatch(
                "templates.preview", {{"template", "cycode.unreal.cppGame"}, {"name", "DisabledGame"}});
            engine.dispatch("plugins.configure", {{"id", "cycode.unreal"}, {"enabled", false}});
            require(engine.dispatch("debug.adapters").at("adapters").size()==2&&engine.dispatch("debug.profile").at("adapterId")=="cycode.unreal.lldb","Disabling plugin failed to remove DAP or silently rewrote profile");
            bool adapterRejected=false;
            try{engine.dispatch("debug.launch");}catch(const std::exception &){adapterRejected=true;}
            require(adapterRejected,"Disabled plugin started a DAP session");
            bool templateRejected = false;
            try {
                engine.dispatch("templates.create", {{"id", pending.at("id")}});
            } catch (...) {
                templateRejected = true;
            }
            require(templateRejected && !std::filesystem::exists(folder / "workspace/DisabledGame"),
                    "Disabled Unreal template created a project");
            require(engine.dispatch("plugins.list").at("projects").empty(),
                    "Disabled Unreal extension left its project provider active");
        }
        std::filesystem::remove_all(folder);
        std::cout << "Unreal extension passed: actual project/targets/modules inspection and "
                     "revision-checked profile edits; no build performed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << folder << '\n';
        return 1;
    }
}

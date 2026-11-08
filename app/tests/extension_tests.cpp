#include "cycode/Commands.h"
#include "cycode/Engine.h"
#include "cycode/PluginManifest.h"
#include <fstream>
#include <iostream>
using namespace cycode;
void pluginPackageContracts(const std::filesystem::path &,const std::filesystem::path &);
static void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream(path, std::ios::binary) << text;
}
static void rejected(const std::function<void()> &call) {
    bool failed = false;
    try {
        call();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected an operation rejection");
}
int main(int argc, char **argv) {
    const auto root = std::filesystem::temp_directory_path() /
                      utf8Path("cycode-extensions-" +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 3, "Expected fixture and dependency libraries");
        const auto hostInfo = pluginHostInfo();
        require(pluginCompatibility({{"hostVersion", {{"minimum", hostInfo.at("version")}}},
                                     {"platforms", {hostInfo.at("platform")}}})
                    .at("compatible"),
                "Exact host/platform compatibility was refused");
        require(!pluginCompatibility({{"hostVersion", {{"maximumExclusive", hostInfo.at("version")}}}})
                     .at("compatible")
                     .get<bool>(),
                "Exclusive host version bound was ignored");
        for (const auto &bad : {Json{{"platforms", Json::array()}}, Json{{"platforms", {"unknown-x64"}}},
                                Json{{"platforms", {hostInfo.at("platform"), hostInfo.at("platform")}}},
                                Json{{"hostVersion", {{"minimum", "1.0.0"}, {"maximumExclusive", "1.0.0"}}}},
                                Json{{"hostVersion", {{"minumum", "1.0.0"}}}}})
            rejected([&] { pluginCompatibility(bad); });
        for (const auto *bad : {"1.2", "1.2.3.4", "-1.0.0", "1000000.0.0", "1.2.3-beta"})
            rejected([&] { pluginVersion(bad); });
        std::filesystem::create_directories(root / "a/sub");
        std::filesystem::create_directories(root / "b");
        std::filesystem::create_directories(root / "extensions");
        write(root / "a/main.cpp", "int value;\n");
        write(root / "b/main.cpp", "int other;\n");
        const auto directory = root / "extensions";
        pluginPackageContracts(root,std::filesystem::path(argv[1]));
        for (int i = 1; i <= 2; ++i)
            std::filesystem::copy_file(argv[i], directory / std::filesystem::path(argv[i]).filename());
        const std::string library = std::filesystem::path(argv[1]).stem().string(),
                          addon = std::filesystem::path(argv[2]).stem().string();
        write(directory / "native.json", Json({{"id", "fixture.native"},
                                               {"abi", 2},
                                               {"version", "1.2.0"},
                                               {"library", library},
                                               {"permissions", {"workspace.write"}}})
                                             .dump());
        write(directory / "addon.json",
              Json({{"id", "fixture.addon"},
                    {"abi", 2},
                    {"version", "1.0.0"},
                    {"library", addon},
                    {"dependencies",
                     Json::array(
                         {{{"id", "fixture.native"}, {"minimum", "1.0.0"}, {"maximumExclusive", "2.0.0"}}})}})
                  .dump());
        write(directory / "bad.json", "[]");
        write(directory / "future.json", Json({{"id", "fixture.future"},
                                               {"abi", 2},
                                               {"version", "1.0.0"},
                                               {"library", "not_installed"},
                                               {"hostVersion", {{"minimum", "999999.0.0"}}}})
                                             .dump());
        {
            std::filesystem::create_directories(root / "dormant/.cycode");
            write(root / "dormant/.cycode/plugins.json",
                  Json({{"schemaVersion", 1},
                        {"enabled", {{"fixture.native", false}, {"fixture.addon", true}}}})
                      .dump());
            Engine dormant(root / "dormant");
            dormant.plugins().load(directory);
            require(dormant.dispatch("plugins.list").at("commands").empty(),
                    "Plugin loaded with a disabled required dependency");
            dormant.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            bool resumed = false;
            const auto dormantInventory = dormant.dispatch("plugins.list");
            for (const auto &entry : dormantInventory.at("plugins"))
                if (entry.value("id", std::string{}) == "fixture.addon")
                    resumed = entry.at("active") && entry.at("loaded") && entry.at("lifecycle").at("active");
            require(resumed, "Enabling a dependency did not activate its enabled dormant dependent");
        }
        {
            Engine first(root / "a"), second(root / "b");
            first.plugins().load(directory);
            second.plugins().load(directory);
            auto eventStats=[&](Engine &engine,Json input={}) {input["mode"]="events";return engine.plugins().invoke("fixture.native.inspect",input);};
            auto waitEvents=[&](const auto &predicate) {const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!predicate()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("Event subscriber did not settle");std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
            waitEvents([&]{return eventStats(first).at("resyncs").get<int>()>0;});
            require(eventStats(first).at("initialWriteDenied").get<bool>(),"Initial event callback could mutate before snapshot");
            require(!std::filesystem::exists(root/"a/must-not-create.txt"),"Read-only subscription handshake created a file");
            write(root/"a/event.cpp","int value=0;\n");
            auto eventDoc=first.dispatch("documents.read",{{"path","event.cpp"}});
            eventStats(first,{{"mutate",true}});
            first.dispatch("documents.replace",{{"path","event.cpp"},{"expectedRevision",eventDoc.at("revision")},{"text","int value=1;\n"}});
            waitEvents([&]{return eventStats(first).at("batches").get<int>()>0;});
            auto eventRead=first.dispatch("documents.read",{{"path","event.cpp"}});
            require(eventRead.at("text")=="int value=1;\n// subscriber\n","Automatic subscriber did not use native guarded editing");
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            require(first.dispatch("documents.read",{{"path","event.cpp"}}).at("text")==eventRead.at("text"),"Subscriber received its own mutation recursively");
            const auto journal=first.dispatch("events.read",{{"types",{"document.changed"}}});
            require(journal.at("events").back().at("origin")=="fixture.native","Plugin event origin missing");
            eventStats(first,{{"fail",true},{"mutate",false}});
            first.dispatch("documents.replace",{{"path","event.cpp"},{"expectedRevision",eventRead.at("revision")},{"text","int failed=0;\n"}});
            waitEvents([&]{const auto info=first.dispatch("plugins.list");return !info.at("eventSubscriptions").empty() && info.at("eventSubscriptions")[0].value("error","")!="";});
            eventStats(first,{{"fail",false}});
            first.dispatch("documents.close",{{"path","event.cpp"},{"discard",true}});
            const auto info = first.dispatch("plugins.list");
            require(info.at("host") == hostInfo, "Plugin host metadata is unavailable");
            require(info.at("snippetCatalogues").size()==1 &&
                info.at("snippetCatalogues")[0].at("definitionCount")==1 &&
                !info.at("snippetCatalogues")[0].contains("definitions"),"Snippet inventory must expose summaries");
            {
                const auto source=first.dispatch("documents.read",{{"path","main.cpp"}});
                const auto query=first.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method","snippets.complete"},
                    {"parameters",{{"path","main.cpp"},{"expectedRevision",source.at("revision")},
                        {"position",{{"line",0},{"character",0}}}}}});
                require(query.at("ok")==true && query.at("result").at("result").at("items").size()==1,
                    "Plugin host request did not share its own declarative snippets");
                bool denied = false;
                try {
                    first.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method","snippets.userSave"},
                        {"parameters",{{"text","{}"},{"expectedRevision","absent"}}}});
                } catch (const std::exception &error) {
                    denied = std::string(error.what()).find("settings.write") != std::string::npos;
                }
                require(denied, "Workspace-only plugin changed personal settings");
            }
            bool incompatible = false;
            for (const auto &entry : info.at("plugins"))
                if (entry.value("id", std::string{}) == "fixture.future")
                    incompatible =
                        entry.at("state") == "incompatible" && !entry.at("loaded").get<bool>() &&
                        entry.at("error").get<std::string>().find("Requires CyCode") != std::string::npos;
            require(incompatible, "Host mismatch was not rejected before loading a missing native library");
            rejected(
                [&] { first.dispatch("plugins.configure", {{"id", "fixture.future"}, {"enabled", true}}); });
            require(info.at("abi") == 2 && info.at("commands").size() == 1,
                    "ABI2 contributions were not registered");
            auto result = first.plugins().invoke("fixture.native.inspect", Json::object());
            require(result.at("result").at("root") == pathUtf8(std::filesystem::canonical(root / "a")),
                    "Plugin instance routed to another workspace");
            require(second.plugins().invoke("fixture.native.inspect", Json::object()).at("counter") == 1,
                    "Plugin instances share mutable state");
            require(first.plugins().invoke("fixture.native.inspect", Json::object()).at("counter") == 2,
                    "First plugin instance lost its state");
            const auto lifecycle = first.plugins().invoke("fixture.native.inspect", {{"mode", "lifecycle"}});
            require(lifecycle.at("active") && lifecycle.at("changes") == 1 && lifecycle.at("hostDenied"),
                    "Startup activation notification or host-call isolation failed");
            {
                std::filesystem::create_directories(root / "cleanup");
                Engine cleanup(root / "cleanup");
                cleanup.plugins().load(directory);
            }
            require(first.plugins()
                            .invoke("fixture.native.inspect", {{"mode", "lifecycle"}})
                            .at("legacyCleanupCalls") == 0,
                    "ABI2 instance used the ABI1 cleanup export");
            second.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            require(
                second.plugins().invoke("fixture.native.inspect", {{"mode", "lifecycle"}}).at("changes") == 1,
                "Idempotent configuration repeated an activation notification");
            second.dispatch("plugins.configure", {{"id", "fixture.addon"}, {"enabled", false}});
            second.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
            second.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            const auto resumed = second.plugins().invoke("fixture.native.inspect", {{"mode", "lifecycle"}});
            require(resumed.at("active") && resumed.at("changes") == 3 && resumed.at("hostDenied"),
                    "Disable/reactivate notifications lost plugin instance state");
            auto document = first.dispatch("documents.read", {{"path", "main.cpp"}});
            for(const auto *method:{"language.codeActions","language.codeActionPrepare","language.rename", "language.format",
                                    "changes.preview","changes.apply","changes.undo"}) {
                std::string failure;
                try {
                    first.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method",method},
                        {"parameters",{{"id","not-issued"},{"path",""}}}});
                } catch (const std::exception &error) { failure=error.what(); }
                require(!failure.empty() && failure.find("not exported")==std::string::npos,
                    "Refactoring host request did not reach native validation: "+failure);
            }
            {
                const auto readDirectory=root/"read-extensions";
                std::filesystem::create_directories(readDirectory);
                std::filesystem::create_directories(root/"read-workspace");
                std::filesystem::copy_file(argv[1],readDirectory/std::filesystem::path(argv[1]).filename());
                write(readDirectory/"native.json",Json{{"id","fixture.native"},{"abi",2},{"version","1.2.0"},
                    {"library",library},{"permissions",Json::array()}}.dump());
                Engine readOnly(root/"read-workspace");readOnly.plugins().load(readDirectory);
                for(const auto *method:{"changes.apply","changes.undo"}) {
                    bool denied = false;
                    try {
                        readOnly.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method",method},
                            {"parameters",{{"id","not-issued"}}}});
                    } catch (const std::exception &error) {
                        denied = std::string(error.what()).find("permission") != std::string::npos;
                    }
                    require(denied, "Refactoring host mutation bypassed workspace.write");
                }
            }
            const auto templateList = first.plugins().invoke(
                "fixture.native.inspect",
                {{"mode", "host"}, {"method", "templates.list"}, {"parameters", Json::object()}});
            require(templateList.at("result").at("templates").size() == 7,
                    "Plugin template catalogue is missing");
            const auto templatePlan = first.plugins().invoke(
                "fixture.native.inspect",
                {{"mode", "host"},
                 {"method", "templates.preview"},
                 {"parameters", {{"template", "java.console"}, {"name", "PluginProject"}}}});
            const auto templateCreated = first.plugins().invoke(
                "fixture.native.inspect", {{"mode", "host"},
                                           {"method", "templates.create"},
                                           {"parameters", {{"id", templatePlan.at("result").at("id")}}}});
            require(templateCreated.at("result").at("state") == "created" &&
                        std::filesystem::exists(root / "a/PluginProject/src/main/java/Main.java"),
                    "Plugin template service did not create a project");
            require(first.dispatch("vcs.providers").at("providers").size() == 2,
                    "External provider registry is incomplete");
            const auto updated = first.plugins().invoke("fixture.native.inspect",
                                                        {{"mode", "replace"},
                                                         {"document",
                                                          {{"path", "main.cpp"},
                                                           {"text", "int edited;\n"},
                                                           {"expectedRevision", document.at("revision")}}}});
            require(updated.at("ok").get<bool>(), "Plugin host document service failed");
            rejected([&] {
                first.plugins().invoke("fixture.native.inspect",
                                       {{"mode", "replace"},
                                        {"document",
                                         {{"path", "main.cpp"},
                                          {"text", "stale"},
                                          {"expectedRevision", document.at("revision")}}}});
            });
            rejected([&] { first.plugins().invoke("fixture.native.inspect", {{"mode", "forbidden"}}); });
            auto context = first.dispatch(
                "vcs.configure",
                {{"root", "."}, {"provider", "fixture.native.depot"}, {"expectedRevision", 1}});
            require(context.at("provider") == "fixture.native.depot", "Provider selection was not persisted");
            const auto projectInspection=first.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method","project.inspect"},{"parameters",Json::object()}});
            require(projectInspection.at("ok")&&projectInspection.at("result").contains("profile"),"Plugin cannot inspect shared project tasks");
            for(const auto *method:{"project.configure","project.build","project.test","project.install","project.script","project.launch","project.debug","project.cancel"}) {
                const auto denied=first.plugins().invoke("fixture.native.inspect",{{"mode","host"},{"method",method},{"parameters",Json::object()}});
                require(!denied.at("ok").get<bool>()&&denied.at("error").get<std::string>().find("permission")!=std::string::npos,"Project task plugin service bypassed process.execute");
            }
            require(first.dispatch("vcs.status").at("hostWriteDenied").get<bool>(),
                    "Read provider mutated host documents");
            require(first.dispatch("vcs.status").at("hostChangeDenied").get<bool>(),
                    "Read provider bypassed the refactoring host mutation guard");
            require(!std::filesystem::exists(root / "a/should-not-exist.txt"), "Read status created a file");
            for (const auto *action : {"inspect", "artifacts", "launchProfile", "automationPlan"}) {
                const auto observed = first.plugins().project("fixture.native.project", action, Json::object());
                require(observed.at("hostReadAllowed").get<bool>(), "Project preview lost host read access");
                require(observed.at("hostWriteDenied").get<bool>() &&
                            observed.at("hostChangeDenied").get<bool>(),
                        "Project preview callback bypassed the host mutation guard");
            }
            require(!std::filesystem::exists(root / "a/project-read-must-not-exist.txt"),
                    "Project preview created a file");
            rejected([&] { first.dispatch("vcs.checkout", {{"path", "main.cpp"}}); });
            const auto live = first.dispatch("documents.read", {{"path", "main.cpp"}});
            first.dispatch("documents.save",
                           {{"path", "main.cpp"}, {"expectedRevision", live.at("revision")}});
            require(first.dispatch("vcs.checkout", {{"path", "main.cpp"}}).at("output") == "checkout",
                    "Provider action was not routed");
            rejected([&] { first.dispatch("vcs.checkout", {{"path", "../b/main.cpp"}}); });
            rejected([&] { first.dispatch("vcs.submit"); });
            require(first.dispatch("vcs.submit", {{"description", "Verified changelist"}}).at("output") ==
                        "submit",
                    "Custom required action fields were not accepted");
            rejected([&] { first.dispatch("vcs.stage", {{"path", "main.cpp"}}); });
            rejected([&] { first.dispatch("vcs.configure", {{"root", "../b"}, {"provider", "git"}}); });
            rejected([&] {
                first.dispatch("vcs.configure",
                               {{"root", "."}, {"provider", "git"}, {"expectedRevision", 1}});
            });
            first.dispatch("vcs.configure", {{"root", "sub"}, {"provider", "git"}, {"expectedRevision", 2}});
            require(first.dispatch("vcs.context", {{"contextPath", "sub/new.cpp"}}).at("provider") == "git",
                    "Most-specific mapping was ignored");
            auto plugins = first.dispatch("plugins.list");
            rejected([&] {
                first.dispatch("plugins.configure", {{"id", "fixture.native"},
                                                     {"enabled", false},
                                                     {"expectedRevision", plugins.at("revision")}});
            });
            const auto snippetSource=first.dispatch("documents.read",{{"path","main.cpp"}});
            const Json snippetRequest={{"path","main.cpp"},{"expectedRevision",snippetSource.at("revision")},
                {"position",{{"line",0},{"character",0}}}};
            const auto snippetIssued=first.dispatch("snippets.complete",snippetRequest);
            first.dispatch("plugins.configure", {{"id", "fixture.addon"}, {"enabled", false}});
            first.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
            require(first.dispatch("plugins.list").at("menus").empty(),
                    "Disabled plugin menu remained registered");
            require(first.dispatch("plugins.list").at("snippetCatalogues").empty() &&
                first.dispatch("snippets.complete",snippetRequest).at("result").at("items").empty(),
                "Disabled plugin snippets remained available");
            rejected([&]{first.dispatch("language.completionPrepare",{{"sessionId",snippetIssued.at("sessionId")},
                {"item",0},{"expectedRevision",snippetSource.at("revision")}});});
            rejected([&] { first.plugins().invoke("fixture.native.inspect", Json::object()); });
            require(!first.dispatch("vcs.context").at("available").get<bool>(),
                    "Unavailable selected provider silently fell back to Git");
            Engine restored(root / "a");
            restored.plugins().load(directory);
            require(restored.dispatch("plugins.list").at("commands").empty(),
                    "Plugin activation settings were not restored");
            const auto restoredInfo = restored.dispatch("plugins.list");
            for (const auto &p : restoredInfo.at("plugins"))
                if (p.contains("enabled"))
                    require(!p.at("loaded").get<bool>(), "Disabled plugin binary was loaded at startup");
            restored.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            require(restored.dispatch("plugins.list").at("commands").size() == 1,
                    "A dormant plugin could not be activated");
            require(restored.dispatch("vcs.context").at("provider") == "fixture.native.depot",
                    "Provider mapping was not restored");
            rejected([&] {
                first.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            }); // Another host saved this configuration.
            Keymap keys;
            keys.extensions({{"fixture.native.inspect", "Inspect", "Extensions"}},
                            {{"fixture.native.inspect", {"Ctrl+Alt+F10"}}});
            keys.load("rider");
            require(keys.press("ctrl+alt+f10", 1, false, false).command == "fixture.native.inspect",
                    "Extension shortcut was not dispatched");
            keys.load("rider", {{"fixture.native.inspect", {"Ctrl+Alt+F11"}}});
            require(keys.press("ctrl+alt+f11", 2, false, false).command == "fixture.native.inspect",
                    "Extension shortcut override failed");
            const auto view = restored.plugins().invoke("fixture.native.panel", {{"event", "read"}});
            const auto changed = restored.plugins().invoke("fixture.native.panel",
                                                           {{"event", "commit"},
                                                            {"property", "counter"},
                                                            {"value", 42},
                                                            {"expectedRevision", view.at("revision")}});
            require(changed.at("properties").at(0).at("value") == 42, "Panel property commit failed");
        }
        std::filesystem::remove_all(root);
        std::cout << "Extensions passed: ABI compatibility, instances, permissions, revisions, dependencies, "
                     "activation, provider routing and persistence.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}

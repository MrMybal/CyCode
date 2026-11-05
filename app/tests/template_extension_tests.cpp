#include "TemplateFixture.h"
#include "cycode/Engine.h"
#include "cycode/TemplateDefinitions.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <future>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
static void require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected rejected template");
}
static void validate() {
    const auto definition = fixtureProjectTemplate();
    validateProjectTemplate(definition);
    const auto expanded = expandProjectTemplate(definition, "Hello-Plugin",
                                                {{"standard", "17"}, {"feature", true}, {"scope", "custom"}});
    require(expanded.at("files").at(0).at("text").get<std::string>().find("${CMAKE_CURRENT_SOURCE_DIR}") !=
                std::string::npos,
            "Unrelated source placeholders were changed");
    require(expanded.at("files").at(2).at("text") == "symbol=Hello_Plugin\napi=HELLO_PLUGIN\n",
            "Project substitutions are incorrect");
    for (const auto *path :
         {"../escape.cpp", "/root.cpp", "C:/root.cpp", "src\\main.cpp", "bad\"name.cpp", ".git/config",
          ".cycode/plugins.json", "NUL.cpp", "src/../bad.cpp", "bad./x.cpp", "bad /x.cpp"}) {
        auto bad = definition;
        bad["files"][0]["path"] = path;
        rejects([&] { validateProjectTemplate(bad); });
    }
    for (int mode = 0; mode < 7; ++mode) {
        auto bad = definition;
        if (mode == 0)
            bad["files"].push_back(bad["files"][0]);
        if (mode == 1)
            bad["files"][1]["path"] = "cmakelists.TXT";
        if (mode == 2)
            bad["files"][1]["text"] = "${option.missing}";
        if (mode == 3)
            bad["entry"] = "missing.uproject";
        if (mode == 4)
            bad["openFile"] = "SRC/main.cpp";
        if (mode == 5)
            bad["files"][0]["text"] = std::string(64 * 1024 + 1, 'x');
        if (mode == 6)
            bad["id"] = "cpp.console";
        rejects([&] { validateProjectTemplate(bad); });
    }
    rejects([&] { expandProjectTemplate(definition, "Valid", {{"unknown", true}}); });
    rejects([&] { expandProjectTemplate(definition, "Valid", {{"standard", "23"}}); });
    rejects([&] { expandProjectTemplate(definition, "Valid", {{"feature", "true"}}); });
    rejects([&] { expandProjectTemplate(definition, "Valid", {{"scope", "bad-name"}}); });
}
int main(int argc, char **argv) {
    const auto root = fs::temp_directory_path() /
                      utf8Path("cycode-template-extension été " +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected template fixture DLL");
        validate();
        fs::create_directories(root / "extensions");
        fs::create_directories(root / "workspace");
        const auto library = utf8Path(argv[1]);
        fs::copy_file(library, root / "extensions" / library.filename());
        std::ofstream(root / "extensions/fixture.json") << Json({{"id", "fixture.native"},
                                                                 {"abi", 2},
                                                                 {"version", "1.0.0"},
                                                                 {"library", pathUtf8(library.stem())},
                                                                 {"permissions", {"workspace.write"}}})
                                                               .dump();
        {
            Engine engine(root / "workspace");
            engine.configureLanguage("", false, false);
            engine.plugins().load(root / "extensions");
            auto list = engine.dispatch("templates.list");
            require(list.at("templates").size() == 7, "Plugin template missing");
            require(engine.dispatch("plugins.list").at("templates").size() == 1,
                    "Plugin inventory omitted templates");
            const auto model =
                engine.dispatch("templates.preview",
                                {{"template", "fixture.native.cmake"},
                                 {"name", "HelloPlugin"},
                                 {"options", {{"standard", "20"}, {"feature", true}, {"scope", "custom"}}}});
            require(!fs::exists(root / "workspace/HelloPlugin") && model.at("plugin") == "fixture.native",
                    "Preview wrote files or lost ownership");
            const auto created = engine.dispatch("templates.create", {{"id", model.at("id")}});
            require(created.at("state") == "created", "Plugin project creation failed");
            for (const auto &file : model.at("files"))
                require(readTextFileBytes(root / "workspace/HelloPlugin" / utf8Path(file.at("path"))) ==
                            file.at("text").get<std::string>(),
                        "Files differ from preview");
            rejects([&] { engine.plugins().invoke("fixture.native.cmake", Json::object()); });
            const auto stale = engine.dispatch("templates.preview",
                                               {{"template", "fixture.native.cmake"}, {"name", "Stale"}});
            engine.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
            require(engine.dispatch("templates.list").at("templates").size() == 6,
                    "Disabled template remained in catalogue");
            rejects([&] { engine.dispatch("templates.create", {{"id", stale.at("id")}}); });
            require(!fs::exists(root / "workspace/Stale"), "Disabled template created files");
            engine.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            rejects([&] { engine.dispatch("templates.create", {{"id", stale.at("id")}}); });
            for (int i = 0; i < 5; ++i) {
                const auto name = "Race" + std::to_string(i);
                const auto plan = engine.dispatch("templates.preview",
                                                  {{"template", "fixture.native.cmake"}, {"name", name}});
                auto first = std::async(std::launch::async, [&] {
                    try {
                        engine.dispatch("templates.create", {{"id", plan.at("id")}});
                        return true;
                    } catch (...) {
                        return false;
                    }
                });
                auto second = std::async(std::launch::async, [&] {
                    engine.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
                });
                require(first.wait_for(std::chrono::seconds(5)) == std::future_status::ready &&
                            second.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
                        "Create/disable lock ordering stalled");
                require(first.get() == fs::exists(root / "workspace" / utf8Path(name)),
                        "Creation/disable was only partially committed");
                second.get();
                engine.dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            }
            auto hostPreview = std::async(std::launch::async, [&] {
                for (int i = 0; i < 20; ++i)
                    engine.plugins().invoke(
                        "fixture.native.inspect",
                        {{"mode", "host"},
                         {"method", "templates.preview"},
                         {"parameters", {{"template", "fixture.native.cmake"}, {"name", "ViaHost"}}}});
            });
            for (int i = 0; i < 20; ++i)
                engine.dispatch("templates.preview",
                                {{"template", "fixture.native.cmake"}, {"name", "Direct"}});
            require(hostPreview.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
                    "Concurrent host/native preview deadlocked");
            hostPreview.get();
        }
        {
            Engine project(root / "workspace/HelloPlugin");
            project.configureLanguage("", false, false);
            const auto configured = project.dispatch("build.configure");
            require(configured.at("exitCode") == 0, configured.dump());
            const auto built = project.dispatch("build.run");
            require(built.at("exitCode") == 0, built.dump());
        }
        fs::remove_all(root);
        std::cout << "Plugin templates passed: typed options, path/content validation, exact creation, "
                     "activation/revision guards, callback/native concurrency, atomic disable/create and "
                     "actual CMake compilation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture: " << pathUtf8(root) << '\n';
        return 1;
    }
}

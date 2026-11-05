#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include <algorithm>
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
    require(failed, "Expected guarded rejection");
}
static void success(const Json &result) {
    require(result.at("exitCode") == 0, result.dump());
}
static void command(const std::vector<std::string> &argv, const fs::path &cwd) {
    const auto result = runProcess(argv, cwd, std::chrono::seconds(90));
    require(result.exitCode == 0, result.output);
}
static void compare(const fs::path &root, const Json &plan) {
    size_t count = 0;
    for (const auto &entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            ++count;
    require(count == plan.at("files").size(), "Unexpected generated files");
    for (const auto &file : plan.at("files")) {
        const auto bytes = readTextFileBytes(root / utf8Path(file.at("path")));
        require(bytes == file.at("text").get<std::string>() && bytes.size() == file.at("bytes").get<size_t>(),
                "Created file differs from preview");
    }
    require(!fs::exists(root / "LICENSE") && !fs::exists(root / ".git"),
            "Generator selected a license or initialized Git");
}
static void guards(const fs::path &root) {
    Engine engine(root);
    engine.configureLanguage("", false, false);
    const auto catalogue = engine.dispatch("templates.list");
    require(catalogue.at("templates").size() == 6, "Template catalogue incomplete");
    for (const auto &info : catalogue.at("templates")) {
        const std::string kind = info.at("id");
        auto name = kind;
        std::replace(name.begin(), name.end(), '.', '_');
        const auto plan = engine.dispatch("templates.preview", {{"template", kind}, {"name", name}});
        require(!fs::exists(root / utf8Path(name)), "Preview wrote project content");
        const auto result = engine.dispatch("templates.create", {{"id", plan.at("id")}});
        compare(root / utf8Path(name), plan);
        require(result.at("state") == "created", "Creation state missing");
        rejects([&] { engine.dispatch("templates.create", {{"id", plan.at("id")}}); });
        require(fs::exists(root / utf8Path(result.at("entry"))), "Generated project entry is missing");
    }
    for (const auto &name : {"", "../escape", "bad name", "x;exit", "CON", "Lpt1", "écho"})
        rejects([&] { engine.dispatch("templates.preview", {{"template", "cpp.console"}, {"name", name}}); });
    for (const auto &path :
         {"../Escaped", "/Absolute", ".git/New", ".cycode/New", "New/../Other", "New\\Other", "NUL"})
        rejects([&] {
            engine.dispatch("templates.preview",
                            {{"template", "cpp.console"}, {"name", "Valid"}, {"destination", path}});
        });
    rejects([&] { engine.dispatch("templates.preview", {{"template", "unknown"}, {"name", "Valid"}}); });
    rejects([&] {
        engine.dispatch("templates.preview",
                        {{"template", "csharp.console"}, {"name", "Valid"}, {"framework", "net9.0\" />"}});
    });
    rejects([&] {
        engine.dispatch("templates.preview",
                        {{"template", "cpp.console"}, {"name", "Valid"}, {"files", Json::array()}});
    });
    const auto collision =
        engine.dispatch("templates.preview", {{"template", "cpp.console"}, {"name", "Collision"}});
    fs::create_directory(root / "Collision");
    std::ofstream(root / "Collision/keep.txt") << "external";
    rejects([&] { engine.dispatch("templates.create", {{"id", collision.at("id")}}); });
    require(readTextFileBytes(root / "Collision/keep.txt") == "external" &&
                !fs::exists(root / "Collision/CMakeLists.txt"),
            "Existing destination was changed");
    fs::create_directory(root / "holder");
    const auto replaced = engine.dispatch(
        "templates.preview",
        {{"template", "cpp.console"}, {"name", "Replaced"}, {"destination", "holder/Replaced"}});
    fs::rename(root / "holder", root / "holder-original");
    fs::create_directory(root / "holder");
    rejects([&] { engine.dispatch("templates.create", {{"id", replaced.at("id")}}); });
    require(!fs::exists(root / "holder/Replaced"), "Preview followed a replaced parent directory");
    const auto expired =
        engine.dispatch("templates.preview", {{"template", "cpp.console"}, {"name", "Evicted"}});
    for (int i = 0; i < 32; ++i)
        engine.dispatch("templates.preview",
                        {{"template", "cpp.console"}, {"name", "Preview" + std::to_string(i)}});
    rejects([&] { engine.dispatch("templates.create", {{"id", expired.at("id")}}); });
    const auto race = engine.dispatch("templates.preview", {{"template", "rust.console"}, {"name", "Race"}});
    auto create = [&] {
        try {
            engine.dispatch("templates.create", {{"id", race.at("id")}});
            return 1;
        } catch (...) {
            return 0;
        }
    };
    auto first = std::async(std::launch::async, create), second = std::async(std::launch::async, create);
    require(first.get() + second.get() == 1, "Concurrent create did not have exactly one winner");
    compare(root / "Race", race);
    for (const auto &entry : fs::directory_iterator(root))
        require(!pathUtf8(entry.path().filename()).starts_with(".cycode-new-"), "Staging folder leaked");
    std::error_code error;
    fs::create_directory_symlink(root / "holder", root / "redirect", error);
    if (!error)
        rejects([&] {
            engine.dispatch(
                "templates.preview",
                {{"template", "cpp.console"}, {"name", "Linked"}, {"destination", "redirect/Linked"}});
        });
    std::cout << "Project preview/create guards, all six exact templates, Unicode parent, collisions, "
                 "replaced parent, quota, concurrency and permissions boundary passed\n";
}
static int build(const fs::path &root, const std::string &kind) {
    Json plan;
    {
        Engine engine(root);
        plan = engine.dispatch("templates.preview", {{"template", kind}, {"name", "HelloProject"}});
        engine.dispatch("templates.create", {{"id", plan.at("id")}});
    }
    const auto project = root / "HelloProject";
    compare(project, plan);
    Engine engine(root / utf8Path(plan.at("entry")));
    engine.configureLanguage("", false, false);
    if (kind == "cpp.console" || kind == "c.console") {
        const auto tools = engine.dispatch("toolchains.detect");
        if (tools.at("cmake") == "" || tools.at("toolchains").empty())
            return 77;
        success(engine.dispatch("build.configure"));
        success(engine.dispatch("build.run"));
        auto ctest = utf8Path(tools.at("cmake")).parent_path() /
#ifdef _WIN32
                     "ctest.exe";
#else
                     "ctest";
#endif
        command({pathUtf8(ctest), "--test-dir", "build", "-C", "Debug", "--output-on-failure"}, project);
    } else if (kind == "csharp.console") {
        const auto dotnet = toolPath("dotnet");
        if (dotnet.empty())
            return 77;
        std::ofstream(project / "NuGet.Config")
            << "<configuration><packageSources><clear /></packageSources></configuration>";
        std::ofstream(project / "global.json")
            << "{\"sdk\":{\"version\":\"9.0.100\",\"rollForward\":\"latestFeature\"}}";
        require(engine.dispatch("workspace.describe").at("solution").at("missingProjects") == 0,
                "Generated .sln has missing projects");
        success(engine.dispatch("msbuild.restore", {{"tool", "dotnet"}}));
        success(engine.dispatch("msbuild.build", {{"tool", "dotnet"}}));
        command({dotnet, pathUtf8(project / "bin/Debug/net8.0/HelloProject.dll"), "--self-test"}, project);
    } else {
        const auto inspected = engine.dispatch("project.inspect");
        const std::string system = inspected.at("kind");
        require(system == (kind == "rust.console"   ? "cargo"
                           : kind == "java.console" ? "java"
                                                    : "npm"),
                "Generated project was not detected");
        const auto tools = inspected.at("tools");
        if (tools.at(system == "cargo" ? "cargo" : system == "java" ? "javac" : "npm") == "")
            return 77;
        success(engine.dispatch("project.configure", {{"offline", true}}));
        success(engine.dispatch("project.build", {{"offline", true}}));
        if (system != "java")
            success(engine.dispatch("project.test", {{"offline", true}}));
        else {
            const auto built = engine.dispatch("project.inspect");
            const auto artifact = built.at("artifacts").at(0);
            command({tools.at("java"), "-cp", pathUtf8(engine.resolve(artifact.at("classPath"))),
                     artifact.at("mainClass"), "--self-test"},
                    project);
        }
    }
    std::cout << kind << ": generated project configured, built and tested through native adapters\n";
    return 0;
}
int main(int argc, char **argv) {
    const auto root = fs::temp_directory_path() /
                      utf8Path("cycode-template été " +
                               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        require(argc == 2, "Expected template test mode");
        fs::create_directory(root);
        const std::string mode = argv[1];
        const int code = mode == "guards" ? (guards(root), 0) : build(root, mode);
        fs::remove_all(root);
        return code;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nFixture: " << pathUtf8(root) << '\n';
        return 1;
    }
}

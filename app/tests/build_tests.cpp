#include "cycode/Engine.h"
#include "cycode/CMakePresets.h"
#include "cycode/BuildLimits.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs = std::filesystem;
void buildOutputContracts();
namespace cycode { void compilerLocationContracts(const std::filesystem::path &); }
static void check(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
static void write(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}
template <class F> static void rejects(F call, const std::string &message) {
    bool rejected = false;
    try {
        call();
    } catch (...) {
        rejected = true;
    }
    check(rejected, message);
}
void presetActionContracts(const std::filesystem::path &);
static void presetMetadata(const fs::path &parent) {
    const auto root = parent / utf8Path("presets é");
    write(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.24)\nproject(Presets NONE)\n");
    write(
        root / "cmake/base.json",
        R"({"version":6,"configurePresets":[{"name":"base","hidden":true,"generator":"Ninja","binaryDir":"${fileDir}/../out/${presetName}","cacheVariables":{"CMAKE_BUILD_TYPE":{"type":"STRING","value":"RelWithDebInfo"}},"environment":{"ROOT":"out","LEAF":"$env{ROOT}/nested"}}]})");
    write(
        root / "CMakePresets.json",
        R"({"version":6,"include":["cmake/base.json"],"configurePresets":[{"name":"dev","inherits":"base"},{"name":"env","inherits":"base","binaryDir":"${sourceDir}/$env{LEAF}/${presetName}"},{"name":"unusable"}]})");
    write(
        root / "CMakeUserPresets.json",
        R"({"version":6,"configurePresets":[{"name":"user","inherits":"dev","cacheVariables":{"CMAKE_BUILD_TYPE":"Release"}}],"buildPresets":[{"name":"build-user","configurePreset":"user","configuration":"Release","targets":["helper"]}]})");
    Engine engine(root);
    engine.configureLanguage("", false, false);
    const auto catalogue = engine.dispatch("build.presets");
    check(catalogue.at("error") == "" && catalogue.at("files").size() == 3,
          "Preset files and implicit user include");
    check(catalogue.at("configurePresets").size() == 4 && catalogue.at("buildPresets").size() == 1,
          "Hidden presets excluded from catalogue");
    const auto profile = engine.dispatch("build.inspect", {{"buildPreset", "build-user"}}).at("profile");
    check(profile.at("configurePreset") == "user" && profile.at("configuration") == "Release" &&
              profile.at("buildDirectory") == "out/user",
          "Inherited preset paths and configuration");
    const auto env = engine.dispatch("build.inspect", {{"configurePreset", "env"}}).at("profile");
    check(env.at("buildDirectory") == "out/nested/env", "Recursive preset environment expansion");
    rejects(
        [&] {
            engine.dispatch("build.inspect", {{"configurePreset", "dev"}, {"buildPreset", "build-user"}});
        },
        "Mismatched configure/build presets accepted");
    rejects([&] { engine.dispatch("build.inspect", {{"configurePreset", "base"}}); },
            "Hidden configure preset selected");
    check(!fs::exists(root / "out") && !fs::exists(root / ".cycode"),
          "Preset inspection executed or generated files");
    CMakePresets snapshot(root, [&](const std::string &path) { return engine.resolve(path); });
    snapshot.checkUnchanged();
    write(root / "cmake/base.json", R"({"version":6,"configurePresets":[]})");
    rejects([&] { snapshot.checkUnchanged(); }, "Changed preset snapshot accepted");
    write(root / "CMakePresets.json", R"({"version":6,"include":["CMakePresets.json"]})");
    check(engine.dispatch("build.presets").at("error") != "", "Include cycle silently accepted");
    write(root / "CMakePresets.json",
          R"({"version":6,"configurePresets":[{"name":"loop","inherits":"loop","binaryDir":"build"}]})");
    rejects([&] { engine.dispatch("build.inspect", {{"configurePreset", "loop"}}); },
            "Preset inheritance cycle accepted");
    write(root / "CMakePresets.json",
          R"({"version":6,"configurePresets":[{"name":"escape","binaryDir":"${sourceParentDir}/outside"}]})");
    rejects([&] { engine.dispatch("build.inspect", {{"configurePreset", "escape"}}); },
            "Preset binary directory escaped workspace");
    write(
        root / "CMakePresets.json",
        R"({"version":6,"configurePresets":[{"name":"env-loop","binaryDir":"$env{A}","environment":{"A":"$env{B}","B":"$env{A}"}}]})");
    rejects([&] { engine.dispatch("build.inspect", {{"configurePreset", "env-loop"}}); },
            "Cyclic preset environment accepted");
}
static void cancellationOwnership() {
    BuildOperations operations;
    std::promise<void> entered, observed, release;
    auto released = release.get_future().share();
    auto enteredFuture = entered.get_future();
    auto observedFuture = observed.get_future();
    auto running = std::async(std::launch::async, [&] {
        return operations.execute(
            "build.run", {{"operationId", "first"}}, [&](const std::atomic<bool> *token) {
                entered.set_value();
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (!token->load() && std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                observed.set_value();
                check(token->load(), "Cancellation did not reach the native work owner");
                check(released.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
                      "Test release gate was not opened");
                return Json{{"exitCode", 0}, {"output", ""}, {"timedOut", false}, {"truncated", false}};
            });
    });
    struct Release {
        BuildOperations &operations;
        std::promise<void> &release;
        ~Release() {
            operations.cancel();
            try {
                release.set_value();
            } catch (...) {
            }
        }
    } cleanup{operations, release};
    check(enteredFuture.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "Native owner did not start");
    check(operations.status().at("running") && operations.status().at("operationId") == "first",
          "Build status lost ownership");
    check(!operations.cancel({{"operationId", "stale"}}).at("requested").get<bool>(),
          "Stale cancellation stopped another build");
    check(operations.cancel({{"operationId", "first"}}).at("requested"),
          "Active build cancellation was refused");
    check(observedFuture.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "Build did not observe cancellation");
    check(operations.status().at("running") && operations.status().at("cancelling"),
          "Cancellation released ownership before native work stopped");
    rejects(
        [&] {
            operations.execute("msbuild.build", {{"operationId", "second"}},
                               [](auto *) { return Json::object(); });
        },
        "A second build entered while cancelled work was still running");
    release.set_value();
    const auto cancelled = running.get();
    check(cancelled.at("cancelled") && cancelled.at("exitCode") != 0, "Cancelled build reported success");
    check(!operations.status().at("running").get<bool>(), "Completed work retained its reservation");
    const auto next =
        operations.execute("unreal.build", {{"operationId", "next"}}, [](const std::atomic<bool> *token) {
            check(!token->load(), "Cancellation leaked into the next build");
            return Json{{"exitCode", 0}};
        });
    check(next.at("exitCode") == 0 && !next.at("cancelled").get<bool>(), "Next native build could not run");
    rejects(
        [&] {
            operations.execute("build.configure", {},
                               [](auto *) -> Json { throw std::runtime_error("fixture failure"); });
        },
        "Native work exception was lost");
    check(!operations.status().at("running").get<bool>(), "Failed native work retained its reservation");
    std::atomic<bool> alreadyCancelled{true};
    check(runProcess({"not-an-installed-executable"}, {}, std::chrono::seconds(1), 4096, &alreadyCancelled)
              .cancelled,
          "A pre-cancelled request tried to resolve/start a process");
}
int main() {
    const auto root =
        fs::temp_directory_path() /
        ("cycode-build-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    struct Cleanup {
        fs::path root;
        bool success = false;
        ~Cleanup() {
            if (success) {
                std::error_code ec;
                fs::remove_all(root, ec);
            }
        }
    } cleanup{root};
    try {
        cancellationOwnership();
        buildOutputContracts();
        compilerLocationContracts(root);
        check(buildTimeout(Json::object()).count()==180 && buildTimeout(Json::object(),150).count()==150,"Default build deadlines");
        check(buildTimeout({{"timeoutSeconds",3600}}).count()==3600,"Long build deadline rejected");
        for(const auto &value:Json::array({0,-1,86401,1.5,true,"180"}))rejects([&]{buildTimeout({{"timeoutSeconds",value}});},"Invalid build deadline accepted");
        presetMetadata(root);
        presetActionContracts(root);
        write(root / "CMakeLists.txt", R"(cmake_minimum_required(VERSION 3.24)
project(CyCodeBuildFixture LANGUAGES CXX)
configure_file(config.hpp.in generated/config.hpp @ONLY)
add_library(helper STATIC src/helper.cpp)
target_include_directories(helper PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated" "${CMAKE_CURRENT_SOURCE_DIR}/include dir")
target_compile_definitions(helper PUBLIC CYCODE_VALUE=73)
target_compile_features(helper PUBLIC cxx_std_20)
add_executable(demo main.cpp)
target_link_libraries(demo PRIVATE helper)
)");
        write(root / "config.hpp.in", "#pragma once\nconstexpr int configured_value = 73;\n");
        write(root / "include dir/widget.hpp", "#pragma once\n#if CYCODE_VALUE == 73\nstruct Widget { int "
                                               "semantic_member; };\nint generated_value();\n#endif\n");
        write(root / "src/helper.cpp", "#include \"widget.hpp\"\n#include \"config.hpp\"\nint "
                                       "generated_value() { return configured_value; }\n");
        const std::string source = "#include \"widget.hpp\"\n#include \"config.hpp\"\nint main() { Widget "
                                   "widget{}; return generated_value() == configured_value ? 0 : 1; }\n";
        write(root / "main.cpp", source);
        write(root / ".clangd", "# User-owned configuration must survive build setup\n");
        Engine engine(root);
        auto detected = engine.dispatch("toolchains.detect");
        for (const auto &toolchain : detected.at("toolchains")) {
            const auto family = toolchain.at("family").get<std::string>();
            if (family != "Clang" && family != "GCC") continue;
            const auto driver = family == "Clang" ? "clang++" : "g++";
            check(utf8Path(toolchain.at("cxxCompiler")) == findExecutableOnPath(driver,
                      {utf8Path(toolchain.at("cxxCompiler"))}),
                  "CMake discovery did not preserve the actual C++ driver alias");
        }
        check(detected["cmake"] != "", "CMake detection");
        check(!detected["toolchains"].empty(), "C/C++ toolchain detection");
        rejects([&] { engine.dispatch("build.inspect", {{"buildDirectory", "../outside"}}); },
                "Build path confinement");
        rejects([&] { engine.dispatch("build.configure", {{"buildDirectory", "."}}); },
                "Reject in-source build");
        rejects([&] { engine.dispatch("build.configure", {{"toolchain", "missing"}}); },
                "Unknown compiler rejected");
        rejects([&] { engine.dispatch("build.configure", {{"generator", "missing"}}); },
                "Unknown generator rejected");
        const auto doc = engine.dispatch("documents.read", {{"path", "main.cpp"}});
        auto semantic = [&] {
            return engine.dispatch("language.definition", {{"path", "main.cpp"},
                                                           {"expectedRevision", doc["revision"]},
                                                           {"position", {{"line", 2}, {"character", 13}}}});
        };
        const auto clangd = LanguageService::defaultExecutable();
        bool haveClangd = !runProcess({clangd, "--version"}, root, std::chrono::seconds(3), 4096).exitCode;
        if (haveClangd) {
            auto baseline = semantic();
            for (const auto &location : baseline["locations"])
                check(location["path"] != "include dir/widget.hpp",
                      "Fixture unexpectedly resolves without CMake flags: " + baseline.dump());
        }
        auto configured = engine.dispatch("build.configure", {{"configuration", "Release"}});
        check(configured["exitCode"] == 0, "Configure: " + configured["output"].get<std::string>());
        const auto streamed = engine.dispatch("build.output");
        check(!streamed.at("running").get<bool>() && streamed.at("byteCount").get<uint64_t>() > 0 &&
                  !streamed.at("output").get<std::string>().empty(),
              "CMake execution did not publish its retained build output");
        const auto &project = configured["project"];
        bool haveDemo = false, haveHelper = false;
        for (const auto &target : project["targets"]) {
            haveDemo |= target["name"] == "demo";
            haveHelper |= target["name"] == "helper";
        }
        check(haveDemo && haveHelper, "CMake File API targets");
        check(!project["toolchains"].empty(), "CMake File API actual compiler");
        const auto database = project["compilationDatabase"];
        check(database["available"].get<bool>() && database["entries"].get<size_t>() >= 2,
              "Compilation database export");
        std::ifstream db(root / utf8Path(database["path"]));
        const auto commands = Json::parse(db);
        db.close();
        check(commands.dump().find("CYCODE_VALUE=73") != std::string::npos &&
                  commands.dump().find("include dir") != std::string::npos,
              "CMake definitions and includes exported");
        if (haveClangd) {
            const auto definition = semantic();
            check(!definition["locations"].empty() &&
                      definition["locations"][0]["path"] == "include dir/widget.hpp",
                  "clangd adopted the configured database: " + definition.dump());
        }
        auto built = engine.dispatch("build.run", {{"target", "demo"}});
        check(built["exitCode"] == 0, "Build target: " + built["output"].get<std::string>());
        fs::path binary;
        for (const auto &target : built["project"]["targets"])
            if (target["name"] == "demo" && !target["artifacts"].empty())
                binary = root / "build" / utf8Path(target["artifacts"][0]["path"]);
        check(!binary.empty() && fs::exists(binary), "Configured executable artifact");
        check(runProcess({pathUtf8(binary)}, root).exitCode == 0, "Built executable behavior");
        const auto paths = engine.dispatch("workspace.files")["files"];
        for (const auto &path : paths)
            check(!path.get<std::string>().starts_with("build/"),
                  "Generated build tree excluded from project search");
        rejects([&] { engine.dispatch("build.run", {{"target", "missing"}}); }, "Unknown target rejected");
        rejects(
            [&] {
                engine.dispatch(
                    "build.configure",
                    {{"generator", project["generator"] == "Ninja" ? "Unix Makefiles" : "Ninja"}});
            },
            "Generator change rejected");
        {
            Engine reopened(root);
            auto restored = reopened.dispatch("build.inspect");
            check(restored["profile"]["configuration"] == "Release" &&
                      restored["profile"]["target"] == "demo",
                  "Persist active profile");
        }
        if (haveClangd) {
            std::ifstream input(root / "CMakeLists.txt");
            std::string original((std::istreambuf_iterator<char>(input)), {});
            input.close();
            auto modified = original;
            modified.replace(modified.find("CYCODE_VALUE=73"), std::string("CYCODE_VALUE=73").size(),
                             "CYCODE_VALUE=84");
            write(root / "CMakeLists.txt", modified);
            auto reconfigured = engine.dispatch("build.configure");
            check(reconfigured["exitCode"] == 0, "Reconfigure changed macro");
            const auto changed = semantic();
            for (const auto &location : changed["locations"])
                check(location["path"] != "include dir/widget.hpp",
                      "clangd kept obsolete macros after reconfigure");
            write(root / "CMakeLists.txt", original);
            auto reverted = engine.dispatch("build.configure");
            check(reverted["exitCode"] == 0 && !semantic()["locations"].empty(),
                  "Reconfigure restores original flags");
        }
        if (detected["ninja"] != "") {
            for (const auto &toolchain : detected["toolchains"]) {
                if (toolchain["family"] != "Clang" && toolchain["family"] != "GCC")
                    continue;
                auto ninja = engine.dispatch("build.configure", {{"buildDirectory", "out-ninja"},
                                                                 {"generator", "Ninja"},
                                                                 {"toolchain", toolchain["id"]},
                                                                 {"target", "demo"},
                                                                 {"configuration", "Release"}});
                check(ninja["exitCode"] == 0, "Ninja configure: " + ninja["output"].get<std::string>());
                check(ninja["project"]["compilationDatabase"]["origin"] == "cmake",
                      "Native Ninja compilation database");
                auto ninjaBuild = engine.dispatch("build.run");
                check(ninjaBuild["exitCode"] == 0, "Ninja build: " + ninjaBuild["output"].get<std::string>());
                break;
            }
        }
        // Return to the original tree to verify failed configuration persistence.
        auto restored = engine.dispatch("build.configure", {{"buildDirectory", "build"},
                                                            {"toolchain", "automatic"},
                                                            {"generator", "automatic"},
                                                            {"target", "demo"},
                                                            {"configuration", "Release"}});
        check(restored["exitCode"] == 0, "Restore original build tree");
        write(root / "CMakeLists.txt", "message(FATAL_ERROR \"intentional configure failure\")\n");
        auto failed = engine.dispatch("build.configure", {{"configuration", "Debug"}});
        check(failed["exitCode"] != 0 && failed["project"].is_null(), "Configure failure propagated");
        check(engine.dispatch("build.inspect")["profile"]["configuration"] == "Release",
              "Failed configure preserves active profile");
        check(std::ifstream(root / ".clangd").peek() == '#', "User clangd configuration preserved");
        engine.configureLanguage(clangd, false, false);
        cleanup.success = true;
        std::cout << "CMake: detection, targets, configure/build/execute, compilation flags, live clangd "
                     "adoption, profile persistence and failure guards passed.\nArtifacts: "
                  << root << '\n';
        // Engine owns the language child until this scope ends; temporary files may be indexed.
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}

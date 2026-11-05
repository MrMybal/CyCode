#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool v, const std::string &message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> static void rejects(F f, const char *message) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    const auto root = fs::temp_directory_path() /
                      ("cycode-language-plugin-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "extensions");
    bool success = false;
    try {
#ifdef _WIN32
        SetEnvironmentVariableW(L"CYCODE_TEST_LANGUAGE_SERVER", utf8Path(argv[2]).c_str());
#else
        setenv("CYCODE_TEST_LANGUAGE_SERVER", argv[2], 1);
#endif
        const auto fixture = fs::absolute(utf8Path(argv[1]));
        fs::copy_file(fixture, root / "extensions" / fixture.filename());
        auto write = [&](const fs::path &file, const std::string &text) {
            std::ofstream out(file, std::ios::binary);
            out << text;
        };
        write(root / "extensions/plugin.json", Json({{"id", "fixture.language"},
                                                     {"name", "Language fixture"},
                                                     {"version", "1.0.0"},
                                                     {"abi", 2},
                                                     {"library", pathUtf8(fixture.stem())},
                                                     {"permissions", {"process.execute"}}})
                                                   .dump());
        const std::string source =
            "struct Widget { int value; };\nint main() { Widget widget; return widget.value; }\n";
        write(root / "main.cycpp", source);
        write(
            root / "compile_commands.json",
            Json::array({{{"directory", pathUtf8(root)},
                          {"file", pathUtf8(root / "main.cycpp")},
                          {"arguments", {"clang++", "-xc++", "-std=c++20", pathUtf8(root / "main.cycpp")}}}})
                .dump());
        {
            Engine engine(root);
            engine.configureLanguage("", false, false);
            engine.plugins().load(root / "extensions");
            auto inventory = engine.dispatch("plugins.list");
            check(inventory.at("languageServers").size() == 1,
                  "No language contribution: " + inventory.dump());
            auto doc = engine.dispatch("documents.read", {{"path", "main.cycpp"}});
            check(engine.supportsLanguage("main.cycpp"), "Dynamic document routing");
            check(engine.languageBackendFor("CyDialect") == "fixture.language.cpp", "Filename routing");
            check(!engine.languageStatus().at("servers").at("fixture.language.cpp").at("enabled"),
                  "Inspection started a process");
            engine.dispatch("language.configure", {{"language", "fixture.language.cpp"}});
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (!engine.languageStatus().at("servers").at("fixture.language.cpp").value("ready", false) &&
                   std::chrono::steady_clock::now() < end)
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            check(engine.languageStatus().at("servers").at("fixture.language.cpp").value("ready", false),
                  "Plugin server never ready: " + engine.languageStatus().dump());
            const auto hover =
                engine.dispatch("language.hover", {{"path", "main.cycpp"},
                                                   {"expectedRevision", doc.at("revision")},
                                                   {"position", {{"line", 1}, {"character", 15}}}});
            check(hover.dump().find("Widget") != std::string::npos, "Actual plugin hover: " + hover.dump());
            const auto symbols =
                engine.dispatch("language.documentSymbols",
                                {{"path", "main.cycpp"}, {"expectedRevision", doc.at("revision")}});
            check(symbols.dump().find("Widget") != std::string::npos, "Actual plugin symbols");
            const auto renamed =
                engine.dispatch("language.rename", {{"path", "main.cycpp"},
                                                    {"expectedRevision", doc.at("revision")},
                                                    {"position", {{"line", 1}, {"character", 15}}},
                                                    {"newName", "Gadget"}});
            check(renamed.dump().find("Gadget") != std::string::npos, "Plugin semantic rename");
            const auto custom =
                Json::array({argv[2], "--background-index", "--log=error", "--header-insertion=never"});
            engine.dispatch("language.configure", {{"language", "fixture.language.cpp"},
                                                   {"arguments", custom},
                                                   {"settings", {{"fixture", true}}}});
            engine.dispatch("language.configure", {{"language", "fixture.language.cpp"}, {"enabled", false}});
            engine.dispatch("language.configure", {{"language", "fixture.language.cpp"}});
            const auto configured = engine.languageStatus().at("servers").at("fixture.language.cpp");
            check(configured.at("arguments") == custom && configured.at("settings").at("fixture") == true,
                  "Restart discarded custom launch settings");
            const auto at = source.rfind("widget.value") + 7;
            const auto start = source.rfind('\n', at);
            const auto proposals = engine.dispatch(
                "language.complete", {{"path", "main.cycpp"},
                                      {"expectedRevision", doc.at("revision")},
                                      {"position", {{"line", 1}, {"character", at - start - 1}}}});
            check(!proposals.at("result").at("items").empty(), "Plugin completion missing");
            engine.dispatch("plugins.configure", {{"id", "fixture.language"}, {"enabled", false}});
            check(!engine.supportsLanguage("main.cycpp"), "Disabled plugin retained route");
            check(!engine.languageStatus().at("servers").contains("fixture.language.cpp"),
                  "Disabled plugin retained process owner");
            rejects(
                [&] {
                    engine.dispatch("language.completionResolve", {{"sessionId", proposals.at("sessionId")},
                                                                   {"index", 0},
                                                                   {"expectedRevision", doc.at("revision")}});
                },
                "Disabled plugin accepted a stale completion");
            rejects(
                [&] {
                    engine.dispatch("language.hover", {{"path", "main.cycpp"},
                                                       {"expectedRevision", doc.at("revision")},
                                                       {"position", {{"line", 1}, {"character", 15}}}});
                },
                "Disabled server accepted a query");
            engine.dispatch("plugins.configure", {{"id", "fixture.language"}, {"enabled", true}});
            check(engine.supportsLanguage("main.cycpp"), "Reactivation routing");
            check(!engine.languageStatus().at("servers").at("fixture.language.cpp").at("enabled"),
                  "Reactivation automatically executed server");
            check(languageBackend("main.py") == "python" && languageBackend("Shader.hlsli") == "hlsl" &&
                      languageBackend("Shader.geom") == "glsl",
                  "New default backend selectors");
        }
        success = true;
        std::cout << "Plugin language routing, actual clangd hover/symbols/rename and lifecycle passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
    if (success) {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    return success ? 0 : 1;
}

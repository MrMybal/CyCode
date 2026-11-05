#include "cycode/Engine.h"
#include "cycode/TextEdits.h"
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace cycode;
void symbolCatalogueContracts();
void navigationLocationContracts();
void occurrenceContracts();
static void require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F fn, const std::string &message) {
    bool rejected = false; try { fn(); } catch (const std::exception &) { rejected = true; }
    require(rejected, message);
}
static void write(const std::filesystem::path &path, const std::string &text) { std::ofstream out(path, std::ios::binary); out << text; }
static std::string read(const std::filesystem::path &path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
static Json position(const std::string &text, const std::string &needle) {
    const auto at = text.rfind(needle); require(at != std::string::npos, "Missing test symbol");
    auto start = text.rfind('\n', at); start = start == std::string::npos ? 0 : start+1;
    return {{"line", std::count(text.begin(), text.begin()+at, '\n')}, {"character", LanguageClient::utf16Length(text.substr(start, at-start))}};
}
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / ("cycode-semantic-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto root = base / "workspace", sdk = base / "sdk", include = sdk / "Engine/Source";
    fs::create_directories(root); fs::create_directories(include); fs::create_directories(sdk / "Engine/Build");
#ifdef _WIN32
    const auto previous = _wgetenv(L"CYCODE_UNREAL_ENGINE"); const std::wstring previousValue = previous ? previous : L"";
    SetEnvironmentVariableW(L"CYCODE_UNREAL_ENGINE", sdk.c_str());
#else
    const auto previous = std::getenv("CYCODE_UNREAL_ENGINE"); const std::string previousValue = previous ? previous : "";
    setenv("CYCODE_UNREAL_ENGINE", sdk.c_str(), 1);
#endif
    int code = 0;
    try {
        symbolCatalogueContracts();
        navigationLocationContracts();
        occurrenceContracts();
        const auto range = [](int a, int b) { return Json{{"start", {{"line", 0}, {"character", a}}}, {"end", {{"line", 0}, {"character", b}}}}; };
        require(utf16Offset("é🛠x", {{"line",0},{"character",3}}) == 6, "UTF-16 to UTF-8 byte offset");
        rejects([&] { utf16Offset("é🛠x", {{"line",0},{"character",2}}); }, "Surrogate split accepted");
        rejects([&] { applyTextEdits("abcd", Json::array({{{"range",range(0,2)},{"newText","x"}},{{"range",range(1,3)},{"newText","y"}}})); }, "Overlapping edits accepted");
        rejects([&] { applyTextEdits("abcd", Json::array({{{"range",range(1,1)},{"newText","x"}},{{"range",range(1,1)},{"newText","y"}}})); }, "Ambiguous insertions accepted");
        require(applyTextEdits("é🛠x", Json::array({{{"range",range(3,4)},{"newText","result"}}})).text == "é🛠result", "Unicode text edit");
        write(sdk / "Engine/Build/Build.version", "{\"MajorVersion\":5,\"MinorVersion\":3,\"PatchVersion\":2}");
        write(include / "NativeActor.h", "#pragma once\nstruct NativeActor { int health; int Health() const { return health; } };\n");
        write(base / "private.h", "#pragma once\nstruct Private { int secret; };\n");
        write(root / "Test.uproject", "{\"FileVersion\":3,\"Modules\":[]}");
        write(root / ".clang-format", "BasedOnStyle: LLVM\nIndentWidth: 4\nAllowShortFunctionsOnASingleLine: None\n");
        const std::string header = "#pragma once\nstruct Widget { int Value; };\nint Read(Widget);\n";
        const std::string implementation = "#include \"widget.hpp\"\nint Read(Widget w) { return w.Value; }\n";
        const std::string name = "résumé.cpp";
        const std::string text = "#include \"widget.hpp\"\n#include \"NativeActor.h\"\n#include \"private.h\"\nstruct Other { int Value; };\nint Probe(Widget w, NativeActor a, Private p) { const char* s = \"🛠\"; return w.Value + a.health + p.secret; } // Value stays in comments\n";
        write(root / "widget.hpp", header); write(root / "widget.cpp", implementation); write(root / utf8Path(name), text);
        Json compilation = Json::array();
        for (const auto &file : {name, std::string("widget.cpp")}) compilation.push_back({{"directory",pathUtf8(root)}, {"file",pathUtf8(root/utf8Path(file))},
            {"arguments",Json::array({"clang++","-std=c++20","-I"+pathUtf8(root),"-I"+pathUtf8(include),"-I"+pathUtf8(base),"-c",pathUtf8(root/utf8Path(file))})}});
        write(root / "compile_commands.json", compilation.dump());
        {
            Engine engine(root); engine.configureLanguage(argv[1], true, true);
            auto doc = engine.dispatch("documents.read", {{"path",name}});
            engine.dispatch("documents.read", {{"path","widget.hpp"}}); engine.dispatch("documents.read", {{"path","widget.cpp"}});
            auto query = [&](const std::string &needle, bool external) { return engine.dispatch("language.definition", {{"path",name},{"expectedRevision",doc["revision"]},{"position",position(doc["text"],needle)},{"includeExternal",external}}); };
            const auto omitted = query("health",false);
            require(omitted["locations"].empty() && omitted["outsideWorkspace"] == 1, "Default workspace-only navigation policy");
            const auto target = query("health",true);
            require(target["locations"].size() == 1 && target["locations"][0]["external"] == true, "SDK header definition should be navigable: "+target.dump());
            const auto handle = target["locations"][0]["path"].get<std::string>();
            auto external = engine.dispatch("sources.read", {{"source",handle}});
            require(external["readOnly"] == true && external["text"].get<std::string>().find("NativeActor") != std::string::npos, "SDK source is a read-only view");
            rejects([&] { engine.dispatch("documents.replace", {{"path",handle},{"text","broken"},{"expectedRevision",external["revision"]}}); }, "SDK handle became editable");
            rejects([&] { engine.dispatch("documents.read", {{"path",pathUtf8(base/"private.h")}}); }, "Arbitrary external file became readable");
            require(query("secret",true)["locations"].empty(), "Non-SDK external source leaked through navigation");
            rejects([&] { engine.dispatch("sources.read", {{"source",pathUtf8(base/"private.h")}}); }, "Source reader accepted an arbitrary path");
            auto outline = engine.dispatch("language.documentSymbols", {{"path",handle},{"expectedRevision",external["revision"]},{"includeExternal",true}});
            require(!outline["symbols"].empty(), "SDK outline should navigate within the same read-only source");
            engine.dispatch("sources.close", {{"source",handle}});
            auto reopened = engine.dispatch("sources.read", {{"source",handle}});
            require(reopened["revision"] != external["revision"], "SDK reopen must invalidate old queries");
            Json symbols;
            for (int i = 0; i < 40; ++i) {
                symbols = engine.dispatch("language.symbols", {{"query","Widget"},{"typesOnly",true}});
                if (!symbols["symbols"].empty()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            require(!symbols["symbols"].empty() && symbols["symbols"][0]["name"] == "Widget", "Workspace type search: "+symbols.dump());
            auto members = engine.dispatch("language.documentSymbols", {{"path",name},{"expectedRevision",doc["revision"]}});
            require(std::any_of(members["symbols"].begin(), members["symbols"].end(), [](const auto &s) { return s["name"] == "Probe"; }), "Document outline");
            // Position the request on Value, after a supplementary Unicode character.
            auto renameParams = [&]() { auto p = position(doc["text"],"w.Value"); p["character"] = p["character"].get<int>()+2; return p; };
            auto plan = engine.dispatch("language.rename", {{"path",name},{"expectedRevision",doc["revision"]},{"position",renameParams()},{"newName","Amount"}});
            require(plan["state"] == "preview" && plan["files"].size() == 3, "Cross-file rename preview: "+plan.dump());
            require(engine.dispatch("documents.read", {{"path",name}})["text"] == text, "Preview modified live text");
            const auto preview = engine.dispatch("changes.preview", {{"id",plan["id"]},{"path",name}});
            require(preview["after"].get<std::string>().find("w.Amount") != std::string::npos && preview["after"].get<std::string>().find("Other { int Value;") != std::string::npos && preview["after"].get<std::string>().find("// Value stays") != std::string::npos, "Rename must distinguish semantic references, shadowing and comments");
            engine.dispatch("changes.apply", {{"id",plan["id"]}});
            require(engine.dispatch("documents.read", {{"path","widget.cpp"}})["text"].get<std::string>().find("w.Amount") != std::string::npos, "Rename implementation references");
            require(read(root/utf8Path(name)) == text && read(root/"widget.hpp") == header && read(root/"widget.cpp") == implementation, "Applying a refactoring saved disk source");
            engine.dispatch("changes.undo", {{"id",plan["id"]}});
            require(engine.dispatch("documents.read", {{"path",name}})["text"] == text, "Undo must restore all original live buffers");
            doc = engine.dispatch("documents.read", {{"path",name}});
            plan = engine.dispatch("language.rename", {{"path",name},{"expectedRevision",doc["revision"]},{"position",renameParams()},{"newName","Amount"}});
            auto changed = engine.dispatch("documents.read", {{"path","widget.hpp"}});
            engine.dispatch("documents.replace", {{"path","widget.hpp"},{"expectedRevision",changed["revision"]},{"text",header+"// concurrent edit\n"}});
            rejects([&] { engine.dispatch("changes.apply", {{"id",plan["id"]}}); }, "Concurrent edit should reject entire refactoring");
            require(engine.dispatch("documents.read", {{"path",name}})["text"] == text && engine.dispatch("documents.read", {{"path","widget.cpp"}})["text"] == implementation, "Rejected rename partially modified another file");
            changed = engine.dispatch("documents.read", {{"path","widget.hpp"}});
            engine.dispatch("documents.replace", {{"path","widget.hpp"},{"expectedRevision",changed["revision"]},{"text",header}});
            doc = engine.dispatch("documents.read", {{"path",name}});
            plan = engine.dispatch("language.rename", {{"path",name},{"expectedRevision",doc["revision"]},{"position",renameParams()},{"newName","Amount"}});
            write(root/"widget.cpp", implementation+"// external disk edit\n");
            rejects([&] { engine.dispatch("changes.apply", {{"id",plan["id"]}}); }, "Disk conflict should reject entire refactoring");
            require(engine.dispatch("documents.read", {{"path",name}})["text"] == text, "Disk conflict partially changed source");
            write(root/"widget.cpp",implementation);
            rejects([&] { engine.dispatch("language.rename", {{"path",name},{"expectedRevision",doc["revision"]},{"position",renameParams()},{"newName","invalid name"}}); }, "Invalid rename name accepted");
            write(root/"format.cpp","int  FormatMe( ){return  1;}\n");
            auto format = engine.dispatch("documents.read", {{"path","format.cpp"}});
            auto formatPlan = engine.dispatch("language.format", {{"path","format.cpp"},{"expectedRevision",format["revision"]}});
            require(formatPlan["editCount"] > 0, "Formatting should return a concrete edit preview");
            engine.dispatch("changes.apply", {{"id",formatPlan["id"]}});
            auto formatted = engine.dispatch("documents.read", {{"path","format.cpp"}});
            require(formatted["text"].get<std::string>().find("\n    return 1;") != std::string::npos, "Formatting must respect the project's .clang-format: "+formatted.dump());
            engine.dispatch("documents.save", {{"path","format.cpp"},{"expectedRevision",formatted["revision"]}});
            engine.dispatch("changes.undo", {{"id",formatPlan["id"]}});
            require(engine.dispatch("documents.read", {{"path","format.cpp"}})["text"] == format["text"], "Refactoring undo should work after an explicit save");
        }
        fs::remove_all(base);
        std::cout << "Real clangd: symbols/outline, SDK read-only navigation, cross-file rename preview/apply/undo, Unicode, atomic conflicts and project formatting passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << "\nArtifacts: " << base << '\n'; code = 1; }
#ifdef _WIN32
    SetEnvironmentVariableW(L"CYCODE_UNREAL_ENGINE", previousValue.empty() ? nullptr : previousValue.c_str());
#else
    if (previousValue.empty()) unsetenv("CYCODE_UNREAL_ENGINE"); else setenv("CYCODE_UNREAL_ENGINE", previousValue.c_str(), 1);
#endif
    return code;
}

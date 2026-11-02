#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
static void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
static void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}
static Json position(const std::string &text, const std::string &needle) {
    auto at = text.rfind(needle);
    require(at != std::string::npos, "Test symbol missing");
    auto start = text.rfind('\n', at);
    start = start == std::string::npos ? 0 : start + 1;
    return {{"line", std::count(text.begin(), text.begin() + at, '\n')},
            {"character", LanguageClient::utf16Length(text.substr(start, at - start))}};
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    const auto root =
        std::filesystem::temp_directory_path() /
        ("cycode-lsp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    try {
        const std::string name = "résumé #1.cpp";
        const auto source = root / utf8Path(name);
        const std::string header =
            "#pragma once\nstruct Widget { int number; void print(); int calculate(int left, int right); };\nint compute(const Widget&);\n";
        const std::string implementation =
            "#include \"widget.hpp\"\nint compute(const Widget& w) { return w.number; }\n";
        const std::string disk = "#include \"widget.hpp\"\nint main() { const char* s = \"🛠\"; Widget "
                                 "widget{}; return compute(widget) + widget.number; }\n";
        write(source, disk);
        write(root / "widget.hpp", header);
        write(root / "widget.cpp", implementation);
        Json commands = Json::array();
        for (const auto &file : {name, std::string("widget.cpp")})
            commands.push_back({{"directory", pathUtf8(root)},
                                {"file", pathUtf8(root / utf8Path(file))},
                                {"arguments", Json::array({"clang++", "-std=c++20", "-I" + pathUtf8(root),
                                                           "-c", pathUtf8(root / utf8Path(file))})}});
        write(root / "compile_commands.json", commands.dump());
        require(LanguageClient::fromUri(LanguageClient::uri(source)) == std::filesystem::absolute(source),
                "Unicode URI roundtrip");
        require(LanguageClient::utf16Length("é🛠") == 3, "UTF-16 supplementary character length");
        bool invalidUri = false;
        try {
            LanguageClient::fromUri("file:///tmp/bad%0");
        } catch (...) {
            invalidUri = true;
        }
        require(invalidUri, "Malformed URI rejected");
        {
            Engine engine(root);
            engine.configureLanguage(argv[1], true, true);
            auto doc = engine.dispatch("documents.read", {{"path", name}});
            engine.dispatch("documents.read", {{"path", "widget.hpp"}});
            engine.dispatch("documents.read", {{"path", "widget.cpp"}});
            auto query = [&](const std::string &method, const std::string &text, const std::string &symbol,
                             Json extra = nullptr) {
                Json params = {{"path", name},
                               {"expectedRevision", doc["revision"]},
                               {"position", position(text, symbol)}};
                if (extra.is_object())
                    params.update(extra);
                return engine.dispatch(method, params);
            };
            auto definition = query("language.definition", disk, "number");
            require(definition["locations"].size() == 1 && definition["locations"][0]["path"] == "widget.hpp",
                    "Member definition in header: " + definition.dump());
            require(definition["locations"][0]["range"]["start"] == position(header, "number"),
                    "Definition target range");
            Json function;
            for (int attempt = 0; attempt < 30; ++attempt) {
                function = query("language.definition", disk, "compute");
                if (!function["locations"].empty() && function["locations"][0]["path"] == "widget.cpp")
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            require(!function["locations"].empty() && function["locations"][0]["path"] == "widget.cpp",
                    "Cross-file function definition: " + function.dump());
            const std::string live = "#include \"widget.hpp\"\nint main() { const char* s = \"🛠\"; Widget "
                                     "widget{}; return compute(widget) + widget.number + widget.number; }\n";
            const auto oldRevision = doc["revision"];
            doc = engine.dispatch("documents.replace",
                                  {{"path", name}, {"text", live}, {"expectedRevision", oldRevision}});
            auto refs = query("language.references", live, "number", {{"includeDeclaration", true}});
            int liveReferences = 0;
            for (const auto &location : refs["locations"])
                if (location["path"] == name)
                    ++liveReferences;
            require(liveReferences == 2, "References use unsaved buffer: " + refs.dump());
            require(refs["positionEncoding"] == "utf-16", "Explicit position encoding");
            auto withoutDeclaration =
                query("language.references", live, "number", {{"includeDeclaration", false}});
            for (const auto &location : withoutDeclaration["locations"])
                require(location["path"] != "widget.hpp", "Declaration excluded");
            auto limited = query("language.references", live, "number", {{"limit", 1}});
            require(limited["locations"].size() == 1 && limited["truncated"].get<bool>(),
                    "Reference result limit");
            bool stale = false;
            try {
                engine.dispatch("language.definition", {{"path", name},
                                                        {"position", position(live, "number")},
                                                        {"expectedRevision", oldRevision}});
            } catch (const std::exception &e) {
                stale = std::string(e.what()).starts_with("RevisionConflict");
            }
            require(stale, "Stale semantic query rejected");
            bool invalidPosition = false;
            try {
                query("language.definition", live, "number",
                      {{"position", {{"line", 1}, {"character", 10000}}}});
            } catch (...) {
                invalidPosition = true;
            }
            require(invalidPosition, "Out-of-line position rejected");
            bool splitSurrogate = false;
            auto emoji = position(live, "🛠");
            emoji["character"] = emoji["character"].get<size_t>() + 1;
            try {
                query("language.definition", live, "number", {{"position", emoji}});
            } catch (const std::exception &) {
                splitSurrogate = true;
            }
            require(splitSurrogate, "Positions inside a surrogate pair are rejected");
            const std::string completionText =
                "#include \"widget.hpp\"\nvoid test() { Widget widget; widget. }\n";
            doc = engine.dispatch(
                "documents.replace",
                {{"path", name}, {"text", completionText}, {"expectedRevision", doc["revision"]}});
            auto completion = query("language.complete", completionText, " }");
            const auto items = completion.at("result").at("items");
            bool found = false;
            for (const auto &item : items)
                if (item.value("label", std::string{}).find("number") != std::string::npos)
                    found = true;
            require(found, "Shared service retains real member completion");
            const auto selected=std::find_if(items.begin(),items.end(),[](const Json &item) { return item.value("label",std::string{}).find("number")!=std::string::npos; });
            const auto prepared=engine.dispatch("language.completionPrepare",{{"sessionId",completion.at("sessionId")},{"item",std::distance(items.begin(),selected)},{"expectedRevision",doc.at("revision")}});
            const auto preview=engine.dispatch("changes.preview",{{"id",prepared.at("plan").at("id")},{"path",name}});
            require(preview.at("after").get<std::string>().find("widget.number")!=std::string::npos,"clangd resolved insertion preview");
            engine.dispatch("changes.apply",{{"id",prepared.at("plan").at("id")}});engine.dispatch("changes.undo",{{"id",prepared.at("plan").at("id")}});
            doc=engine.dispatch("documents.read",{{"path",name}});
            const auto callText=std::string("#include \"widget.hpp\"\nvoid test() { Widget widget; widget.cal }\n");
            doc=engine.dispatch("documents.replace",{{"path",name},{"text",callText},{"expectedRevision",doc.at("revision")}});
            const auto calls=query("language.complete",callText," }");const auto &callItems=calls.at("result").at("items");
            const auto callable=std::find_if(callItems.begin(),callItems.end(),[](const Json &item) { return item.value("label",std::string{}).find("calculate")!=std::string::npos; });
            require(callable!=callItems.end(),"clangd callable completion missing");
            const auto callPlan=engine.dispatch("language.completionPrepare",{{"sessionId",calls.at("sessionId")},{"item",std::distance(callItems.begin(),callable)},{"expectedRevision",doc.at("revision")}});
            require(callPlan.at("snippet").is_object() && callPlan.at("snippet").at("fields").size()==2,"Actual clangd parameter placeholders missing: "+callPlan.dump());
            require(engine.languageStatus()["ready"].get<bool>(), "Shared service status");
            std::ifstream in(source, std::ios::binary);
            std::string actual((std::istreambuf_iterator<char>(in)), {});
            require(actual == disk, "Semantic requests never save live edits");
        }
        std::filesystem::remove_all(root);
        std::cout << "Real clangd: shared completion, cross-file definitions/references, unsaved overlay, "
                     "Unicode URI/UTF-16, revisions, bounds and shutdown passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nArtifacts: " << root << '\n';
        return 1;
    }
}

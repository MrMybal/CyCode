#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
namespace fs = std::filesystem;
static void check(bool v, const std::string &message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> static bool wait(F f) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    do {
        if (f())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (std::chrono::steady_clock::now() < end);
    return false;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const std::string backend = argv[1];
    const bool python = backend == "python";
    const auto root = fs::temp_directory_path() /
                      ("cycode-extra-language-" + backend +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    bool success = false;
    if (defaultServerLaunch(root, backend).argv.empty()) {
        std::cout << "Run setup_languages.py --" << backend << "\n";
        fs::remove_all(root);
        return 77;
    }
    try {
        const auto path = python ? "main.py" : "main.vert";
        const auto name = python ? "greet" : "amplify";
        const std::string text =
            python ? "def greet(value: int) -> int:\n    return value + 1\nanswer = greet(2)\n"
                   : "#version 450\nfloat amplify(float value) { return value * 2.0; }\nvoid main() { float "
                     "result = amplify(1.0); gl_Position = vec4(result); }\n";
        std::ofstream(root / path) << text;
        {
            Engine engine(root);
            engine.configureLanguage("", false, false);
            auto doc = engine.dispatch("documents.read", {{"path", path}});
            check(!engine.languageStatus().at("servers").at(backend).value("enabled", false),
                  "Opening started server");
            engine.dispatch("language.configure", {{"language", backend}});
            check(
                wait([&] { return engine.languageStatus().at("servers").at(backend).value("ready", false); }),
                "Server not ready: " + engine.languageStatus().dump());
            const auto methods = engine.languageStatus().at("servers").at(backend).at("methods");
            if (methods.value("documentSymbols", false)) {
                const auto symbols = engine.dispatch(
                    "language.documentSymbols", {{"path", path}, {"expectedRevision", doc.at("revision")}});
                check(symbols.dump().find(name) != std::string::npos,
                      "Actual symbols missing: " + symbols.dump());
            } else {
                bool rejected = false;
                try {
                    engine.dispatch("language.documentSymbols",
                                    {{"path", path}, {"expectedRevision", doc.at("revision")}});
                } catch (const std::exception &e) {
                    rejected = std::string(e.what()).find("does not support") != std::string::npos;
                }
                check(rejected, "Unsupported symbols were not reported explicitly");
            }
            const auto call = text.rfind(name);
            const auto start = text.rfind('\n', call);
            Json position = {{"line", std::count(text.begin(), text.begin() + call, '\n')},
                             {"character", call - (start == std::string::npos ? 0 : start + 1) + 1}};
            Json query = {{"path", path}, {"expectedRevision", doc.at("revision")}, {"position", position}};
            Json hover;
            check(wait([&] {
                      hover = engine.dispatch("language.hover", query);
                      return hover.value("available", false);
                  }),
                  "Actual hover missing: " + hover.dump());
            auto definitions = engine.dispatch("language.definition", query);
            check(!definitions.at("locations").empty(), "Actual definition missing");
            if (methods.value("rename", false)) {
                auto rename = query;
                rename["newName"] = "renamedFunction";
                const auto plan = engine.dispatch("language.rename", rename);
                check(plan.dump().find("renamedFunction") != std::string::npos, "Actual rename missing");
            }
            if (methods.value("format", false))
                check(engine
                          .dispatch("language.format",
                                    {{"path", path}, {"expectedRevision", doc.at("revision")}})
                          .is_object(),
                      "Actual formatting preview missing");
            const std::string partial =
                python ? "def greet(value: int) -> int:\n    return value + 1\nanswer = gre\n"
                       : "#version 450\nfloat amplify(float value) { return value * 2.0; }\nvoid main() { "
                         "amp }\n";
            doc = engine.dispatch(
                "documents.replace",
                {{"path", path}, {"expectedRevision", doc.at("revision")}, {"text", partial}});
            Json completion;
            position = {{"line", 2}, {"character", python ? 12 : 17}};
            check(wait([&] {
                      completion = engine.dispatch(
                          "language.complete",
                          {{"path", path}, {"expectedRevision", doc.at("revision")}, {"position", position}});
                      return completion.dump().find(name) != std::string::npos;
                  }),
                  "Actual completion missing: " + completion.dump());
            if (python) {
                doc = engine.dispatch("documents.replace", {{"path", path},
                                                            {"expectedRevision", doc.at("revision")},
                                                            {"text", text + "broken: int = 'text'\n"}});
                check(wait([&] {
                          return engine.languageStatus()
                                     .at("servers")
                                     .at(backend)
                                     .at("diagnostics")
                                     .dump()
                                     .find("not assignable") != std::string::npos;
                      }),
                      "Actual Python type diagnostic missing");
            }
            engine.dispatch("language.configure", {{"language", backend}, {"enabled", false}});
            check(wait([&] {
                      return !engine.languageStatus().at("servers").at(backend).value("running", false);
                  }),
                  "Disabled server retained process");
        }
        success = true;
        std::cout << backend
                  << " advertised capabilities, hover, definition, completion and lifecycle passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
    }
    if (success) {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    return success ? 0 : 1;
}

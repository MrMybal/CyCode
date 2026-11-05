#include "cycode/Engine.h"
#include <fstream>

namespace cycode {
void compilerLocationContracts(const std::filesystem::path &parent) {
    namespace fs = std::filesystem;
    const auto root = parent / "compiler-locations";
    fs::create_directories(root / "nested" / "source dir");
    const auto source = root / "nested" / "source dir" / utf8Path("été.cpp");
    {
        std::ofstream file(source);
        file << "int value;\n";
    }
    Engine engine(root);
    engine.configureLanguage("", false, false);
    const auto relative = pathUtf8(source.lexically_relative(root));
    auto check = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    for (const auto &text : std::vector<std::string>{
             relative + "(12,3): error C1000: message", relative + ":12:3: error: message",
             "  --> " + relative + ":12:3", "1>" + relative + "(12,3): warning"}) {
        const auto location = engine.dispatch("build.location", {{"text", text}});
        check(location.at("mapped") && location.at("path") == relative && location.at("line") == 12 &&
                  location.at("column") == 3,
              "Compiler source coordinates were lost");
    }
    for (const auto &text :
         std::vector<std::string>{relative + ":12: message", "ERROR: " + relative + ":12: message",
                                  "\"" + pathUtf8(source) + "\"(12): error", relative + "(12) : error"}) {
        const auto location = engine.dispatch("build.location", {{"text", text}});
        check(location.at("mapped") && location.at("line") == 12 && location.at("column") == 1,
              "Single-coordinate/quoted compiler location failed");
    }
    check(
        engine
            .dispatch("build.location", {{"text", "source dir/été.cpp:7:2: error"}, {"directory", "nested"}})
            .at("mapped"),
        "Actual nested working directory was ignored");
    for (const auto &text :
         std::vector<std::string>{"ERROR: 0:12: syntax error", "plain message", relative + ":0:2: error",
                                  relative + ":999999999999999999999: error", "missing.cpp(1): error",
                                  "file://" + pathUtf8(source) + ":12:3: error", "../outside.cpp:1:2: error"})
        check(!engine.dispatch("build.location", {{"text", text}}).at("mapped").get<bool>(),
              "Ambiguous, invalid or missing location mapped");
    const auto outside = parent / "outside.cpp";
    {
        std::ofstream file(outside);
        file << "// external\n";
    }
    check(!engine.dispatch("build.location", {{"text", pathUtf8(outside) + "(1): error"}})
               .at("mapped")
               .get<bool>(),
          "External compiler path escaped workspace guard");
    std::error_code ec;
    fs::create_symlink(outside, root / "escape.cpp", ec);
    if (!ec)
        check(!engine.dispatch("build.location", {{"text", "escape.cpp:1: error"}}).at("mapped").get<bool>(),
              "Symlink compiler path escaped workspace guard");
    for (const auto &parameters : Json::array({{{"text", "bad\nline"}},
                                               {{"text", std::string("bad\0line", 8)}},
                                               {{"text", std::string(16385, 'x')}},
                                               {{"text", "line"}, {"directory", "../"}},
                                               {{"text", "line"}, {"directory", "missing"}},
                                               {{"text", "line"}, {"unknown", true}},
                                               {{"text", true}}})) {
        bool rejected = false;
        try {
            engine.dispatch("build.location", parameters);
        } catch (const std::exception &) {
            rejected = true;
        }
        check(rejected, "Invalid compiler-location request was accepted");
    }
}
} // namespace cycode

#include "cycode/BuildSystem.h"
#include "cycode/CMakePresets.h"
#include "cycode/TextEdits.h"
#include <fstream>
#include <stdexcept>
namespace {
using namespace cycode;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    try {
        fn();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Invalid preset action accepted");
}
} // namespace
void presetActionContracts(const std::filesystem::path &parent) {
    using namespace cycode;
    namespace fs = std::filesystem;
    const auto root = parent / "preset-actions";
    fs::create_directories(root);
    std::ofstream(root / "CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.25)\nproject(PresetMetadata NONE)\n";
    const auto definition = Json::parse(R"({
      "version":6,
      "configurePresets":[
        {"name":"dev","generator":"Ninja","binaryDir":"${sourceDir}/out","environment":{"OUTPUT":"configured"}},
        {"name":"other","generator":"Ninja","binaryDir":"${sourceDir}/other"}],
      "buildPresets":[{"name":"compile","configurePreset":"dev"}],
      "testPresets":[
        {"name":"base","hidden":true,"configurePreset":"dev","environment":{"OUTPUT":"inherited"},
         "output":{"outputLogFile":"$env{OUTPUT}/test.log"}},
        {"name":"unit","inherits":"base","output":{"outputOnFailure":true}},
        {"name":"own","inherits":"base","environment":{"OUTPUT":"explicit"}},
        {"name":"escape","inherits":"base","output":{"outputLogFile":"${sourceDir}/../outside.log"}},
        {"name":"different","configurePreset":"other"}],
      "packagePresets":[{"name":"zip","configurePreset":"dev","generators":["ZIP"],"packageDirectory":"packages"}],
      "workflowPresets":[
        {"name":"ci","steps":[{"type":"configure","name":"dev"},{"type":"build","name":"compile"},
                            {"type":"test","name":"unit"},{"type":"package","name":"zip"}]},
        {"name":"bad-first","steps":[{"type":"test","name":"unit"}]},
        {"name":"bad-root","steps":[{"type":"configure","name":"dev"},{"type":"test","name":"different"}]}]
    })");
    auto save = [&](const Json &value) { std::ofstream(root / "CMakePresets.json") << value.dump(2); };
    save(definition);
    auto resolver = [&](const std::string &value) {
        const auto resolved = fs::weakly_canonical(root / utf8Path(value));
        if (!containsPath(root, resolved))
            throw std::runtime_error("Escaped test workspace");
        return resolved;
    };
    CMakePresets metadata(root, resolver);
    const auto catalogue = metadata.catalogue();
    check(catalogue.at("testPresets").size() == 4, "Hidden action preset leaked into selector");
    BuildSystem build(root, resolver);
    auto plan = build.presetPlan({{"kind", "workflow"}, {"preset", "ci"}});
    check(plan.at("steps").size() == 4 &&
              plan.at("arguments") == Json::array({"--workflow", "--preset", "ci"}),
          "Workflow argv or ordered steps");
    check(plan.at("steps")[2].at("outputs")[0] == "out/configured/test.log",
          "Configure environment precedence or inherited output fields");
    const auto own = build.presetPlan({{"kind", "test"}, {"preset", "own"}});
    check(own.at("outputs")[0] == "out/explicit/test.log", "Explicit action environment lost");
    const auto package = build.presetPlan({{"kind", "package"}, {"preset", "zip"}});
    check(package.at("program") == "cpack" && package.at("outputs")[0] == "out/packages", "CPack plan paths");
    rejects([&] { build.presetPlan({{"kind", "workflow"}, {"preset", "bad-first"}}); });
    rejects([&] { build.presetPlan({{"kind", "workflow"}, {"preset", "bad-root"}}); });
    rejects([&] { build.presetPlan({{"kind", "test"}, {"preset", "base"}}); });
    rejects([&] { build.presetPlan({{"kind", "test"}, {"preset", "escape"}}); });
    rejects(
        [&] { build.runPreset({{"kind", "workflow"}, {"preset", "ci"}, {"expectedRevision", "obsolete"}}); });
    std::ofstream(root / "CMakeUserPresets.json") << R"({"version":6})";
    rejects([&] { metadata.checkUnchanged(); });
    auto changed = definition;
    changed["workflowPresets"][0]["steps"].erase(3);
    save(changed);
    check(build.presetPlan({{"kind", "workflow"}, {"preset", "ci"}}).at("revision") != plan.at("revision"),
          "Changed workflow reused its review token");
}

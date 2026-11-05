#include "cycode/Paths.h"
#include "cycode/ShaderBuild.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid shader contract accepted");
}
} // namespace
void shaderBuildContracts(const std::filesystem::path &fixture) {
    using namespace cycode;
    namespace fs = std::filesystem;
    const auto root = fixture / ".cycode" / utf8Path("shader-contract & été");
    fs::create_directories(root / "include");
    const auto resolve = [&](const std::string &text) {
        auto path = utf8Path(text);
        if (!path.is_absolute())
            path = root / path;
        path = fs::weakly_canonical(path);
        auto relative = path.lexically_relative(fs::canonical(root));
        if (relative.empty() || relative.is_absolute())
            throw std::runtime_error("Outside shader fixture");
        for (const auto &part : relative)
            if (part == "..")
                throw std::runtime_error("Outside shader fixture");
        return path;
    };
    const auto source = root / utf8Path("source & été.frag.hlsl"), output = root / "shader.spv";
    Json profile = {{"shaderTarget", "vulkan1.2"},
                    {"shaderDefines", Json::array({"MODE=1", "TEXT=with spaces & punctuation"})},
                    {"shaderIncludes", Json::array({"include"})}};
    const auto hlsl = shaderBuildPlan(profile, source, output, resolve);
    require(hlsl.tool == "dxc" && hlsl.stage == "frag" && hlsl.format == "spirv",
            "HLSL compiler/target/stage selection");
    require(std::find(hlsl.arguments.begin(), hlsl.arguments.end(), "ps_6_0") != hlsl.arguments.end(),
            "HLSL actual profile");
    require(hlsl.arguments.back() == pathUtf8(source), "Unicode shader source argv");
    require(std::find(hlsl.arguments.begin(), hlsl.arguments.end(), "-DTEXT=with spaces & punctuation") !=
                hlsl.arguments.end(),
            "Macro remains one argument");
    const auto glsl = shaderBuildPlan(profile, root / "source.comp.glsl", output, resolve);
    require(glsl.tool == "glslangValidator" && glsl.stage == "comp", "GLSL stage inference");
    require(shaderSourceFile("source.vert.glsl") && shaderSourceFile("source.rchit") &&
                !shaderSourceFile("source.hlsli"),
            "Shader candidates exclude includes");
    rejects([&] { shaderBuildPlan(profile, root / "source.glsl", output, resolve); });
    auto invalid = profile;
    invalid["shaderTarget"] = "-Fo";
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderEntry"] = "-DENTRY";
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderIncludes"] = Json::array({"../../outside"});
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderDefines"] = Json::array({"MODE=1", "MODE=2"});
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderDefines"] = Json::array({"MODE=1\n#define INJECTED 1"});
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderStage"] = "rgen";
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderLanguage"] = "glsl";
    invalid["shaderTarget"] = "dxil";
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    invalid = profile;
    invalid["shaderLanguage"] = "glsl";
    invalid["shaderEntry"] = "custom";
    rejects([&] { shaderBuildPlan(invalid, source, output, resolve); });
    const auto diagnostics = shaderBuildDiagnostics(
        pathUtf8(source) + ":7:4: error: unknown identifier\nWARNING: 0:3: extension warning\nERROR: 7:9: "
                           "unknown numeric source\nERROR: /outside.glsl:9: cannot map\n",
        source, root, root, resolve);
    require(diagnostics.size() == 2 && diagnostics[0].at("line") == 7 && diagnostics[0].at("column") == 4 &&
                diagnostics[1].at("severity") == "warning",
            "Actual DXC/GLSL diagnostic formats and guarded paths");
    {
        std::string bytes(20, '\0');
        bytes[0] = 3;
        bytes[1] = 2;
        bytes[2] = 35;
        bytes[3] = 7;
        std::ofstream out(output, std::ios::binary);
        out.write(bytes.data(), bytes.size());
    }
    validateShaderArtifact(output, "spirv");
    rejects([&] { validateShaderArtifact(output, "dxil"); });
    rejects([&] { validateShaderArtifact(output, "unknown"); });
}

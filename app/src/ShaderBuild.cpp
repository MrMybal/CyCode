#include "cycode/ShaderBuild.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
bool symbol(const std::string &text) {
    return !text.empty() && text.size() <= 128 &&
           ((text.front() >= 'a' && text.front() <= 'z') || (text.front() >= 'A' && text.front() <= 'Z') ||
            text.front() == '_') &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
           });
}
std::string field(const Json &p, const char *name, const char *fallback) {
    auto text = p.value(name, std::string(fallback));
    if (text.size() > 8192 || text.find_first_of("\r\n") != std::string::npos ||
        text.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid shader option: " + std::string(name));
    return text;
}
const std::map<std::string, std::string> stages = {
    {"vert", "vs"}, {"frag", "ps"}, {"geom", "gs"}, {"tesc", "hs"}, {"tese", "ds"},
    {"comp", "cs"}, {"mesh", "ms"}, {"task", "as"}, {"rgen", ""},   {"rint", ""},
    {"rahit", ""},  {"rchit", ""},  {"rmiss", ""},  {"rcall", ""}};
} // namespace
bool shaderSourceFile(const fs::path &path) {
    auto ext = lower(pathUtf8(path.extension()));
    return ext == ".hlsl" || ext == ".glsl" || (ext.size() > 1 && stages.contains(ext.substr(1)));
}
ShaderBuildPlan shaderBuildPlan(const Json &p, const fs::path &source, const fs::path &output,
                                const std::function<fs::path(const std::string &)> &resolve) {
    ShaderBuildPlan plan;
    plan.language = field(p, "shaderLanguage", "automatic");
    if (plan.language == "automatic")
        plan.language = lower(pathUtf8(source.extension())) == ".hlsl" ? "hlsl" : "glsl";
    if (plan.language != "hlsl" && plan.language != "glsl")
        throw std::runtime_error("Select HLSL or GLSL");
    plan.stage = field(p, "shaderStage", "automatic");
    if (plan.stage == "automatic") {
        auto ext = lower(pathUtf8(source.extension()));
        if (ext == ".hlsl" || ext == ".glsl")
            ext = lower(pathUtf8(source.stem().extension()));
        if (ext.size() > 1 && stages.contains(ext.substr(1)))
            plan.stage = ext.substr(1);
        else
            throw std::runtime_error("Select a shader stage, or use a stage suffix such as .frag.glsl");
    }
    if (!stages.contains(plan.stage))
        throw std::runtime_error("Unsupported shader stage");
    const auto entry = field(p, "shaderEntry", "main");
    if (!symbol(entry))
        throw std::runtime_error("Invalid shader entry point");
    auto target = field(p, "shaderTarget", "automatic");
    if (target == "automatic")
        target = plan.language == "hlsl" ? "dxil" : "vulkan1.2";
    static const std::set<std::string> targets = {"dxil",      "opengl",    "vulkan1.0",
                                                  "vulkan1.1", "vulkan1.2", "vulkan1.3"};
    if (!targets.contains(target) || (plan.language == "glsl" && target == "dxil") ||
        (plan.language == "hlsl" && target == "opengl"))
        throw std::runtime_error("Shader language and target are incompatible");
    plan.format = target == "dxil" ? "dxil" : "spirv";
    plan.extension = plan.format == "dxil" ? ".dxil" : ".spv";
    if (plan.language == "hlsl") {
        plan.tool = "dxc";
        const auto prefix = stages.at(plan.stage);
        if (prefix.empty())
            throw std::runtime_error(
                "HLSL ray tracing requires library/export models, which are not connected here");
        const auto model = field(p, "shaderModel", "6_0");
        if (!std::regex_match(model, std::regex("6_[0-9]")))
            throw std::runtime_error("Expected an HLSL shader model such as 6_0");
        plan.arguments = {"-T",
                          prefix + "_" + model,
                          "-E",
                          entry,
                          "-Fo",
                          pathUtf8(output),
                          "-fdiagnostics-format=clang",
                          "-fdiagnostics-color=never"};
        if (target != "dxil")
            plan.arguments.insert(plan.arguments.end(), {"-spirv", "-fspv-target-env=" + target});
        plan.arguments.push_back(field(p, "configuration", "Debug") == "Debug" ? "-Od" : "-O3");
    } else {
        if (entry != "main")
            throw std::runtime_error("GLSL source entry point must be main");
        plan.tool = "glslangValidator";
        plan.arguments = {"-S", plan.stage, "--target-env", target, "-o", pathUtf8(output)};
        // Release uses the compiler's default optimization; optional optimizer builds differ.
        if (field(p, "configuration", "Debug") == "Debug")
            plan.arguments.push_back("-Od");
    }
    auto definitions = p.value("shaderDefines", Json::array()),
         includes = p.value("shaderIncludes", Json::array());
    if (!definitions.is_array() || definitions.size() > 128 || !includes.is_array() || includes.size() > 64)
        throw std::runtime_error("Too many shader definitions/include directories");
    std::set<std::string> names;
    for (const auto &value : definitions) {
        const auto text = value.get<std::string>(), name = text.substr(0, text.find('='));
        if (!symbol(name) || !names.insert(name).second || text.size() > 4096 ||
            text.find_first_of("\r\n") != std::string::npos || text.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid or duplicate shader macro definition");
        plan.arguments.push_back("-D" + text);
    }
    for (const auto &value : includes) {
        const auto text = value.get<std::string>();
        if (text.empty() || text.size() > 8192 || text.find('\0') != std::string::npos ||
            text.find_first_of("\r\n") != std::string::npos)
            throw std::runtime_error("Invalid shader include directory");
        const auto directory = resolve(text);
        if (!fs::is_directory(directory))
            throw std::runtime_error("Shader include directory does not exist");
        plan.arguments.push_back("-I" + pathUtf8(directory));
    }
    plan.arguments.push_back(pathUtf8(source));
    return plan;
}
Json shaderBuildDiagnostics(const std::string &text, const fs::path &source, const fs::path &workspace,
                            const fs::path &directory,
                            const std::function<fs::path(const std::string &)> &resolve) {
    Json result = Json::array();
    const std::regex clang(R"(^(.*):([0-9]+):([0-9]+): (fatal error|error|warning|note): (.*)$)");
    const std::regex glsl(R"(^(ERROR|WARNING): (.+?):([0-9]+):[ ]*(.*)$)");
    std::istringstream lines(text.substr(0, 4 * 1024 * 1024));
    std::string line;
    while (result.size() < 5000 && std::getline(lines, line)) {
        if (line.size() > 32768)
            continue;
        if (line.ends_with('\r'))
            line.pop_back();
        std::smatch match;
        std::string file, severity, message;
        int row = 0, column = 1;
        try {
            if (std::regex_match(line, match, clang)) {
                file = match[1];
                row = std::stoi(match[2]);
                column = std::stoi(match[3]);
                severity = match[4];
                message = match[5];
            } else if (std::regex_match(line, match, glsl)) {
                file = match[2];
                row = std::stoi(match[3]);
                severity = match[1] == "WARNING" ? "warning" : "error";
                message = match[4];
                // Numeric GLSL source IDs do not name a verified workspace file.
                if (file != "0" && std::all_of(file.begin(), file.end(),
                                               [](unsigned char c) { return c >= '0' && c <= '9'; }))
                    continue;
                if (file == "0")
                    file = pathUtf8(source);
            } else
                continue;
            if (file.empty() || file.find('\0') != std::string::npos || row < 1 || row > 10000000 ||
                column < 1 || column > 1000000)
                continue;
            auto path = utf8Path(file);
            if (!path.is_absolute())
                path = directory / path;
            path = resolve(pathUtf8(path));
            result.push_back({{"path", pathUtf8(path.lexically_relative(workspace))},
                              {"line", row},
                              {"column", column},
                              {"severity", severity == "fatal error" ? "error" : severity},
                              {"message", message},
                              {"code", "shader"}});
        } catch (const std::exception &) { /* Keep unmappable tool output in the build log. */
        }
    }
    return result;
}
void validateShaderArtifact(const fs::path &path, const std::string &format) {
    if (format != "spirv" && format != "dxil")
        throw std::runtime_error("Unknown shader artifact format");
    if (fs::is_symlink(fs::symlink_status(path)) || !fs::is_regular_file(path))
        throw std::runtime_error("Shader compiler did not produce a regular output artifact");
    const auto size = fs::file_size(path);
    if (size < (format == "spirv" ? 20u : 32u) || size > 64 * 1024 * 1024 || (format == "spirv" && size % 4))
        throw std::runtime_error("Invalid shader artifact size");
    std::array<unsigned char, 4> magic{};
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char *>(magic.data()), magic.size());
    const auto expected = format == "spirv" ? std::array<unsigned char, 4>{3, 2, 35, 7}
                                            : std::array<unsigned char, 4>{'D', 'X', 'B', 'C'};
    if (!input || magic != expected)
        throw std::runtime_error("Shader artifact format does not match the selected target");
}
} // namespace cycode

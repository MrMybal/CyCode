#include "cycode/Process.h"
#include "cycode/UnrealLaunch.h"
#include "cycode/UnrealProject.h"
#include <fstream>
namespace cycode {
using Json = nlohmann::json;
void unrealArtifactContracts(const std::filesystem::path &root) {
    namespace fs = std::filesystem;
    auto require = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto write = [](const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    };
    const auto workspace = root / "artifact-project", engine = root / "artifact-engine";
    write(workspace / "Game.uproject", R"({"Modules":[{"Name":"Game","Type":"Runtime"}]})");
    write(workspace / "Source/Game.Build.cs", "// metadata fixture only");
    write(workspace / "Source/GameEditor.Target.cs", "// metadata fixture only");
    write(engine / "Engine/Build/Build.version", R"({"MajorVersion":5,"MinorVersion":3})");
#ifdef _WIN32
    const std::string host = "Win64";
#elif defined(__APPLE__)
    const std::string host = "Mac";
#elif defined(__aarch64__)
    const std::string host = "LinuxArm64";
#else
    const std::string host = "Linux";
#endif
    const auto editor = unrealEditorExecutable(engine, 5, host, "Development");
    // Non-executable bytes used only by opt-in metadata assertions; no tool is dispatched.
    write(editor, "metadata fixture, not an executable");
    const auto engineManifest =
        engine / "Engine/Binaries" / (host == "LinuxArm64" ? "Linux" : host) / "UnrealEditor.modules";
    write(engineManifest, R"({"BuildId":"engine-id","Modules":{}})");
    UnrealProject owner(workspace, workspace / "Game.uproject", Json::object(), [&](const std::string &s) {
        auto p = utf8Path(s);
        return p.is_absolute() ? p : workspace / p;
    });
    Json input = {{"project", "Game.uproject"},
                  {"engineRoot", pathUtf8(engine)},
                  {"target", "GameEditor"},
                  {"platform", host},
                  {"configuration", "Development"}};
    auto result = owner.artifacts(input);
    require(!result.at("ready").get<bool>() && result.at("receiptRequired").get<bool>(),
            "Code projects require their Editor receipt");
    require(!fs::exists(workspace / ".cycode"), "Artifact inspection creates no output/profile/cache");
    const auto directory = workspace / "Binaries" / host, receipt = directory / "GameEditor.target",
               manifest = directory / "UnrealEditor.modules", module = directory / "Game.bin";
    const auto projectPrefix = "$(ProjectDir)/Binaries/" + host + "/";
    Json data = {
        {"TargetName", "GameEditor"},
        {"TargetType", "Editor"},
        {"Platform", host},
        {"Configuration", "Development"},
        {"Project", "../../Game.uproject"},
        {"Version", {{"BuildId", "engine-id"}}},
        {"Launch", "$(EngineDir)/" + pathUtf8(editor.lexically_relative(engine / "Engine"))},
        {"BuildProducts",
         Json::array({{{"Path", projectPrefix + "Game.bin"}, {"Type", "DynamicLibrary"}},
                      {{"Path", projectPrefix + "UnrealEditor.modules"}, {"Type", "RequiredResource"}}})}};
    write(receipt, data.dump());
    write(module, "non-executable binary fixture");
    const Json modules = {{"BuildId", "engine-id"}, {"Modules", {{"Game", "Game.bin"}}}};
    write(manifest, modules.dump());
    result = owner.artifacts(input);
    require(result.at("ready").get<bool>() && result.at("complete").get<bool>() &&
                result.at("modules").size() == 1,
            "Matching Editor receipt/manifests and nonempty files pass the scoped assessment");
    auto wrong = data;
    wrong["TargetType"] = "Game";
    write(receipt, wrong.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(),
            "Receipt target type is checked rather than inferred from the filename");
    wrong = data;
    wrong["Configuration"] = "DebugGame";
    write(receipt, wrong.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(), "Receipt configuration must match");
    wrong = data;
    wrong["Version"]["BuildId"] = "foreign";
    write(receipt, wrong.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(), "Receipt BuildId must match the engine");
    wrong = data;
    wrong["Project"] = "../../../outside.uproject";
    write(receipt, wrong.dump());
    require(!owner.artifacts(input).at("complete").get<bool>(),
            "Receipt paths cannot escape the selected project/engine roots");
    write(receipt, data.dump());
    fs::remove(module);
    require(!owner.artifacts(input).at("ready").get<bool>(),
            "A missing manifest-listed binary blocks readiness");
    write(module, "non-executable binary fixture");
    auto stale = modules;
    stale["BuildId"] = "foreign";
    write(manifest, stale.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(), "Project module BuildId must match the engine");
    stale = modules;
    stale["Modules"]["Game"] = "../escape.bin";
    write(manifest, stale.dump());
    require(!owner.artifacts(input).at("complete").get<bool>(), "Module map entries must be plain filenames");
    write(manifest, modules.dump());
    wrong = data;
    wrong["BuildProducts"][0]["Path"] = "$(Unknown)/Game.bin";
    write(receipt, wrong.dump());
    require(!owner.artifacts(input).at("complete").get<bool>(),
            "Unknown receipt variables remain unverified");
    write(receipt, std::string(4 * 1024 * 1024 + 1, ' '));
    require(!owner.artifacts(input).at("complete").get<bool>(), "Metadata read limits remain explicit");
    write(receipt, data.dump());
    const auto pluginManifest = workspace / "Plugins/Foo/Binaries" / host / "UnrealEditor.modules";
    auto withPlugin = data;
    withPlugin["BuildProducts"].push_back(
        {{"Path", "$(ProjectDir)/Plugins/Foo/Binaries/" + host + "/UnrealEditor.modules"},
         {"Type", "RequiredResource"}});
    write(receipt, withPlugin.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(),
            "Receipt-listed project plugin manifests are required");
    write(pluginManifest, Json{{"BuildId", "foreign"}, {"Modules", Json::object()}}.dump());
    require(!owner.artifacts(input).at("ready").get<bool>(), "Receipt-listed plugin BuildIds are checked");
    write(pluginManifest, Json{{"BuildId", "engine-id"}, {"Modules", Json::object()}}.dump());
    require(owner.artifacts(input).at("ready").get<bool>(),
            "Matching receipt-listed plugin metadata is accepted within scope");
    auto unsupported = input;
    unsupported["platform"] = "Android";
    require(!owner.artifacts(unsupported).at("supported").get<bool>(),
            "Unsupported layouts do not claim readiness");
    write(workspace / "Game.uproject", "{}");
    fs::remove(workspace / "Source/Game.Build.cs");
    fs::remove(receipt);
    fs::remove(manifest);
    auto content = owner.artifacts(input);
    require(!content.at("receiptRequired").get<bool>() && content.at("ready").get<bool>(),
            "Content-only projects do not require a project code receipt; unlisted plugin activation is "
            "outside scope");
}
} // namespace cycode

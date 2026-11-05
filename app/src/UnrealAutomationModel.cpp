#include "cycode/BuildLimits.h"
#include "cycode/Process.h"
#include "cycode/UnrealAutomation.h"
#include <algorithm>
#include <regex>
#include <set>
namespace cycode {
using Json = nlohmann::json;
Json unrealAutomationRequest(const Json &params) {
    const auto action = params.at("action").get<std::string>();
    if (action != "cook" && action != "package")
        throw std::runtime_error("Choose cook or package");
    for (const auto *key : {"project", "engineRoot"}) {
        const auto text = params.at(key).get<std::string>();
        if (text.empty() || text.size() > 8192 ||
            std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("Invalid Unreal project/engine path");
    }
    const auto target = params.at("target").get<std::string>(),
               platform = params.at("platform").get<std::string>();
    const auto configuration = params.at("configuration").get<std::string>();
    if (target.size() > 128 || !std::regex_match(target, std::regex("[A-Za-z_][A-Za-z0-9_]*")) ||
        target.ends_with("Editor"))
        throw std::runtime_error("Choose a discovered game target, not an Editor target");
    if (!std::set<std::string>{"Win64", "Linux", "LinuxArm64", "Mac"}.contains(platform))
        throw std::runtime_error("Unreal automation currently supports desktop packaging platforms");
    if (!std::set<std::string>{"Debug", "DebugGame", "Development", "Shipping", "Test"}.contains(
            configuration))
        throw std::runtime_error("Unsupported Unreal automation configuration");
    const auto maps = params.value("maps", Json::array());
    validateUnrealMapPackages(maps);
    const bool package = action == "package";
    const bool pak = package && params.value("pak", true),
               ioStore = package && params.value("ioStore", false);
    const bool compressed = package && params.value("compressed", false);
    if ((ioStore || compressed) && !pak)
        throw std::runtime_error("IoStore/compressed packaging requires Pak enabled");
    return {{"project", params.at("project")},
            {"engineRoot", params.at("engineRoot")},
            {"target", target},
            {"platform", platform},
            {"configuration", configuration},
            {"action", action},
            {"maps", maps},
            {"build", params.value("build", true)},
            {"pak", pak},
            {"ioStore", ioStore},
            {"compressed", compressed},
            {"timeoutSeconds", buildTimeout(params, 3600).count()}};
}
void validateUnrealMapPackages(const Json &maps) {
    if (!maps.is_array() || maps.size() > 64)
        throw std::runtime_error("Choose at most 64 explicit /Game maps");
    std::set<std::string> seen;
    size_t mapBytes = 0;
    for (const auto &map : maps) {
        auto text = map.get<std::string>();
        mapBytes += text.size() + 1;
        if (mapBytes > 8000)
            throw std::runtime_error("Explicit Unreal map list exceeds its 8000-byte argument budget");
        if (text.size() > 512 || !text.starts_with("/Game/") || text.size() == 6 || text.back() == '/' ||
            text.find("//") != std::string::npos)
            throw std::runtime_error("Invalid Unreal /Game map package name");
        for (unsigned char c : text)
            if (c < 128 && !(c == '/' || c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9')))
                throw std::runtime_error(
                    "Map names cannot contain options, separators or object/file extensions");
        (void)Json(text).dump();
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; });
        if (!seen.insert(text).second)
            throw std::runtime_error("Duplicate or case-ambiguous Unreal map");
    }
}
std::vector<std::string> unrealAutomationArguments(const Json &request, const std::filesystem::path &project,
                                                   const std::filesystem::path &editor,
                                                   const std::filesystem::path &output) {
    // Revalidate callers of the pure planner too; no raw argv or shell fragments.
    const auto value = unrealAutomationRequest(request);
    std::vector<std::string> args = {"-ScriptsForProject=" + pathUtf8(project),
                                     "BuildCookRun",
                                     "-project=" + pathUtf8(project),
                                     "-nop4",
                                     "-unattended",
                                     "-utf8output",
                                     "-nocompileeditor",
                                     "-cook",
                                     "-target=" + value.at("target").get<std::string>(),
                                     "-platform=" + value.at("platform").get<std::string>(),
                                     "-clientconfig=" + value.at("configuration").get<std::string>(),
                                     "-unrealexe=" + pathUtf8(editor)};
    if (value.at("build").get<bool>())
        args.push_back("-build");
    if (!value.at("maps").empty()) {
        std::string maps;
        for (const auto &map : value.at("maps")) {
            if (!maps.empty())
                maps += '+';
            maps += map.get<std::string>();
        }
        args.push_back("-map=" + maps);
    }
    if (value.at("action") == "package") {
        args.insert(args.end(),
                    {"-stage", "-package", "-archive", "-stagingdirectory=" + pathUtf8(output / "stage"),
                     "-archivedirectory=" + pathUtf8(output / "archive")});
        if (value.at("pak").get<bool>())
            args.push_back("-pak");
        if (value.at("ioStore").get<bool>())
            args.push_back("-iostore");
        else
            args.push_back("-skipiostore");
        if (value.at("compressed").get<bool>())
            args.push_back("-compressed");
        else
            args.push_back("-ForceUncompressed");
    }
    return args;
}
} // namespace cycode

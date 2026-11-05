#include "cycode/UnrealAutomation.h"
#include "cycode/BuildLimits.h"
#include "cycode/BuildOperations.h"
#include "cycode/FileTransactions.h"
#include "cycode/Recovery.h"
#include "cycode/Tools.h"
#include "cycode/UnrealProject.h"
#include <algorithm>
#include <fstream>
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
void ordinaryAutomation(const fs::path &path) {
    const auto absolute = fs::absolute(path).lexically_normal(), canonical = fs::weakly_canonical(path);
#ifdef _WIN32
    const bool same = _wcsicmp(absolute.c_str(), canonical.c_str()) == 0;
#else
    const bool same = absolute == canonical;
#endif
    if (!same || fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Redirected Unreal automation output is unsupported");
}
Json automationStamp(const fs::path &path, bool content = false) {
    if (!fs::is_regular_file(path))
        throw std::runtime_error("Required Unreal input/tool file is missing: " + pathUtf8(path));
    Json result = {{"path", pathUtf8(fs::canonical(path))},
                   {"bytes", fs::file_size(path)},
                   {"modified", fs::last_write_time(path).time_since_epoch().count()}};
    if (content) {
        const auto size = fs::file_size(path);
        if (size > 4 * 1024 * 1024)
            throw std::runtime_error("Unreal automation metadata exceeds 4 MiB");
        std::ifstream file(path, std::ios::binary);
        std::string bytes(static_cast<size_t>(size) + 1, '\0');
        file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (file.bad() || file.gcount() != static_cast<std::streamsize>(size))
            throw std::runtime_error("Unreal metadata changed while reading");
        bytes.resize(static_cast<size_t>(size));
        result["guard"] = RecoveryStore::fingerprint(bytes);
    }
    return result;
}
} // namespace
UnrealProject::Json UnrealProject::automationContext(const Json &params, const std::string &id) {
    auto model = inspectLocked(params);
    if (!model.value("available", false))
        throw std::runtime_error("Open an Unreal project before cooking or packaging");
    auto input = params;
    input.update(model.at("profile"));
    const auto request = unrealAutomationRequest(input);
    const auto engineRoot = utf8Path(request.at("engineRoot").get<std::string>());
    if (engineRoot.empty() || model.at("engineVersion").value("MajorVersion", 0) != 5)
        throw std::runtime_error("Select an installed Unreal Engine 5 with precompiled AutomationTool");
    fs::path targetFile;
    for (const auto &target : model.at("targets"))
        if (target.at("name") == request.at("target")) {
            if (!targetFile.empty())
                throw std::runtime_error("Ambiguous Unreal automation target");
            targetFile = resolve_(target.at("path"));
        }
    if (targetFile.empty())
        throw std::runtime_error("Choose a game target discovered from project Target.cs rules");
    const auto project = resolve_(request.at("project"));
    const auto output = resolve_(".cycode/unreal-automation/" + id);
    for (const auto &path : {root_ / ".cycode", root_ / ".cycode/unreal-automation", output})
        ordinaryAutomation(path);
    if (fs::exists(fs::symlink_status(output)))
        throw std::runtime_error("Unreal automation output is already occupied; create a fresh preview");
    auto tool = unrealManagedTool(engineRoot, "AutomationTool");
    if (tool.empty())
        throw std::runtime_error(
            "Precompiled AutomationTool is missing; build/install it through Unreal first");
    auto executable = utf8Path(toolPath(tool.front()));
    if (!executable.is_absolute() || !fs::is_regular_file(executable))
        throw std::runtime_error("Unreal AutomationTool needs an installed .NET host");
    tool[0] = pathUtf8(fs::canonical(executable));
#ifdef _WIN32
    const auto editor = engineRoot / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe";
#elif defined(__APPLE__)
    auto editor = engineRoot / "Engine/Binaries/Mac/UnrealEditor-Cmd";
    if (!fs::is_regular_file(editor))
        editor = engineRoot / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor";
#else
    const auto editor = engineRoot / "Engine/Binaries/Linux/UnrealEditor-Cmd";
#endif
    Json guards = Json::array({automationStamp(project, true), automationStamp(targetFile, true),
                               automationStamp(engineRoot / "Engine/Build/Build.version", true),
                               automationStamp(editor)});
    for (const auto &part : tool)
        guards.push_back(automationStamp(utf8Path(part)));
    const auto runtimeConfig =
        engineRoot / "Engine/Binaries/DotNET/AutomationTool/AutomationTool.runtimeconfig.json";
    if (fs::is_regular_file(runtimeConfig))
        guards.push_back(automationStamp(runtimeConfig, true));
    for (const auto &map : request.at("maps")) {
        const auto name = map.get<std::string>().substr(6) + ".umap";
        const auto path = resolve_(pathUtf8(project.parent_path() / "Content" / utf8Path(name)));
        guards.push_back(automationStamp(path));
    }
    auto args = unrealAutomationArguments(request, project, editor, output);
    tool.insert(tool.end(), args.begin(), args.end());
    for (const auto &arg : tool)
        if (arg.size() > 8192 || arg.find('\0') != std::string::npos)
            throw std::runtime_error("AutomationTool argument exceeds its text bound");
    const auto cwd = engineRoot / "Engine/Binaries/DotNET/AutomationTool";
    if (!fs::is_directory(cwd) || pathUtf8(cwd).size() > 8192)
        throw std::runtime_error("AutomationTool program directory is missing");
    Json result = {{"id", id},
                   {"request", request},
                   {"arguments", tool},
                   {"cwd", pathUtf8(cwd)},
                   {"outputDirectory", pathUtf8(output.lexically_relative(root_))},
                   {"archiveDirectory", request.at("action") == "package"
                                            ? pathUtf8((output / "archive").lexically_relative(root_))
                                            : std::string{}},
                   {"stagingDirectory", request.at("action") == "package"
                                            ? pathUtf8((output / "stage").lexically_relative(root_))
                                            : std::string{}},
                   {"logDirectory", pathUtf8((output / "logs").lexically_relative(root_))},
                   {"cooksContent", true},
                   {"packages", request.at("action") == "package"},
                   {"buildsCode", request.at("build")},
                   {"executesProjectCode", true},
                   {"mayWriteEngineCaches", true},
                   {"launchesPackagedGame", false}};
    result["revision"] = RecoveryStore::fingerprint(result.dump() + guards.dump() + directoryIdentity(root_) +
                                                    directoryIdentity(engineRoot));
    return result;
}
UnrealProject::Json UnrealProject::automationPlan(const Json &params) {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        throw std::runtime_error("An Unreal operation is running; wait or cancel it before previewing");
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(automationPlans_,
                  [&](const auto &item) { return now - item.second.created > std::chrono::minutes(10); });
    while (automationPlans_.size() >= 16) {
        const auto oldest = std::min_element(
            automationPlans_.begin(), automationPlans_.end(),
            [](const auto &a, const auto &b) { return a.second.created < b.second.created; });
        automationPlans_.erase(oldest);
    }
    const auto id =
        "uat-" + RecoveryStore::fingerprint(params.dump() + std::to_string(now.time_since_epoch().count()) +
                                            std::to_string(++automationSequence_));
    auto plan = automationContext(params, id);
    automationPlans_.emplace(id, AutomationPlan{plan, now});
    return plan;
}
UnrealProject::Json UnrealProject::automationRun(const Json &params, const std::atomic<bool> *cancellation) {
    auto lock = cancellableBuildLock(mutex_, cancellation);
    const auto id = params.at("planId").get<std::string>();
    auto found = automationPlans_.find(id);
    if (found == automationPlans_.end() ||
        std::chrono::steady_clock::now() - found->second.created > std::chrono::minutes(10))
        throw std::runtime_error("Unreal automation preview expired or was consumed; review again");
    const auto plan = found->second.value;
    if (params.at("expectedRevision") != plan.at("revision") || !params.contains("timeoutSeconds") ||
        buildTimeout(params).count() != plan.at("request").at("timeoutSeconds").get<int>() ||
        automationContext(plan.at("request"), id).at("revision") != plan.at("revision"))
        throw std::runtime_error("Unreal automation inputs/tools/deadline changed; review a fresh preview");
    const auto output = resolve_(plan.at("outputDirectory"));
    checkBuildCancellation(cancellation);
    automationPlans_.erase(found); // One attempt; no retry after output creation or uncertain execution.
    const auto parent = resolve_(".cycode/unreal-automation");
    ordinaryAutomation(root_ / ".cycode");
    ordinaryAutomation(parent);
    fs::create_directories(parent);
    ordinaryAutomation(parent);
    if (!fs::create_directory(output))
        throw std::runtime_error("Unreal automation output was occupied before dispatch");
    ordinaryAutomation(output);
    const auto outputIdentity = directoryIdentity(output);
    fs::create_directory(output / "logs");
    writeFileExclusive(output / "intent.json", Json({{"schemaVersion", 1}, {"plan", plan}}).dump(2) + "\n");
    std::map<std::string, std::string> environment = {{"uebp_LogFolder", pathUtf8(output / "logs")},
                                                      {"DOTNET_CLI_TELEMETRY_OPTOUT", "1"},
                                                      {"DOTNET_SKIP_FIRST_TIME_EXPERIENCE", "1"}};
    const auto args = plan.at("arguments").get<std::vector<std::string>>();
    if (args.size() > 1 && utf8Path(args[1]).extension() == ".dll") {
        environment["DOTNET_ROOT"] = pathUtf8(utf8Path(args[0]).parent_path());
        environment["DOTNET_HOST_PATH"] = args[0];
    }
    const auto process = runProcess(args, utf8Path(plan.at("cwd")), buildTimeout(plan.at("request"), 3600),
                                    8 * 1024 * 1024, cancellation, environment, processOutputObserver(cancellation));
    const auto outputText =
        Json::parse(Json(process.output).dump(-1, ' ', false, Json::error_handler_t::replace))
            .get<std::string>();
    Json result = {{"exitCode", process.exitCode},
                   {"output", outputText},
                   {"outputRepaired", outputText != process.output},
                   {"timedOut", process.timedOut},
                   {"truncated", process.truncated},
                   {"cancelled", process.cancelled},
                   {"planId", id},
                   {"outputDirectory", plan.at("outputDirectory")},
                   {"archiveDirectory", plan.at("archiveDirectory")},
                   {"logDirectory", plan.at("logDirectory")},
                   {"archiveAvailable", false},
                   {"project", {{"available", false}}}};
    try {
        ordinaryAutomation(output);
        if (directoryIdentity(output) != outputIdentity)
            throw std::runtime_error("Automation output identity changed");
        writeFileExclusive(output / "output.log", process.output);
        if (plan.at("packages").get<bool>()) {
            const auto archive = output / "archive";
            ordinaryAutomation(archive);
            bool available = false;
            size_t entries = 0;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            if (fs::is_directory(archive))
                for (const auto &entry : fs::recursive_directory_iterator(archive)) {
                    if (++entries > 2000 || std::chrono::steady_clock::now() > deadline)
                        throw std::runtime_error("Archive inspection exceeded its bound");
                    ordinaryAutomation(entry.path());
                    if (entry.is_regular_file()) {
                        available = true;
                        break;
                    }
                }
            result["archiveAvailable"] = available;
            if (!process.exitCode && !process.cancelled && !process.truncated && !process.timedOut &&
                !available)
                throw std::runtime_error(
                    "AutomationTool exited successfully but no archive file was observed");
        }
    } catch (const std::exception &error) {
        result["inspectionError"] = error.what();
    }
    try {
        result["project"] = inspectLocked(Json::object());
    } catch (const std::exception &error) {
        result["inspectionError"] = result.value("inspectionError", std::string{}) + "\n" + error.what();
    }
    try {
        ordinaryAutomation(output);
        if (directoryIdentity(output) != outputIdentity)
            throw std::runtime_error("Automation result directory changed");
        writeFileExclusive(output / "result.json", result.dump(2) + "\n");
    } catch (const std::exception &error) {
        result["receiptError"] = error.what();
    }
    return result;
}
} // namespace cycode

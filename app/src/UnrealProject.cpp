#include "cycode/UnrealProject.h"
#include "cycode/BuildLimits.h"
#include "cycode/BuildOperations.h"
#include "cycode/UnrealAutomation.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
using Json = nlohmann::json;
namespace fs = std::filesystem;
static Json readJson(const fs::path &path) {
    if (fs::file_size(path) > 4 * 1024 * 1024)
        throw std::runtime_error("Unreal metadata exceeds 4 MiB");
    std::ifstream in(path);
    return Json::parse(in);
}
static bool file(const fs::path &path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}
static bool engine(const fs::path &path) {
    return !path.empty() && file(path / "Engine/Build/Build.version");
}
static fs::path analysisPath(const Json &profile) {
    const auto target = profile.at("target").get<std::string>();
    const auto platform = profile.at("platform").get<std::string>();
    const auto configuration = profile.at("configuration").get<std::string>();
    for (const auto &value : {target, platform, configuration})
        if (!std::regex_match(value, std::regex("[A-Za-z_][A-Za-z0-9_]*")))
            throw std::runtime_error("Invalid Unreal analysis profile");
    return fs::path(".cycode/unreal-analysis") / utf8Path(target) / utf8Path(platform) /
           utf8Path(configuration);
}
static fs::path environment(const char *key) {
#ifdef _WIN32
    std::wstring wide(key, key + std::char_traits<char>::length(key));
    auto size = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (!size)
        return {};
    std::wstring value(size, 0);
    GetEnvironmentVariableW(wide.c_str(), value.data(), size);
    value.resize(size - 1);
    return value;
#else
    auto value = std::getenv(key);
    return value ? fs::path(value) : fs::path{};
#endif
}
#ifdef _WIN32
static fs::path registry(HKEY hive, const std::wstring &key, const std::wstring &name) {
    DWORD bytes = 0;
    if (RegGetValueW(hive, key.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &bytes) !=
        ERROR_SUCCESS)
        return {};
    std::wstring text(bytes / sizeof(wchar_t), 0);
    if (RegGetValueW(hive, key.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, text.data(), &bytes) !=
        ERROR_SUCCESS)
        return {};
    while (!text.empty() && !text.back())
        text.pop_back();
    return text;
}
#endif
static fs::path findEngine(const std::string &association, const Json &solution) {
    if (auto path = environment("CYCODE_UNREAL_ENGINE"); engine(path))
        return fs::canonical(path);
#ifdef _WIN32
    auto name = utf8Path(association).wstring();
    for (auto path : {registry(HKEY_CURRENT_USER, L"SOFTWARE\\Epic Games\\Unreal Engine\\Builds", name),
                      registry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\EpicGames\\Unreal Engine\\" + name,
                               L"InstalledDirectory")})
        if (engine(path))
            return fs::canonical(path);
#else
#ifdef __APPLE__
    auto ini = environment("HOME") / "Library/Application Support/Epic/UnrealEngine/Install.ini";
#else
    auto ini = environment("HOME") / ".config/Epic/UnrealEngine/Install.ini";
#endif
    if (file(ini)) {
        std::ifstream in(ini);
        std::string line;
        while (std::getline(in, line)) {
            auto equals = line.find('=');
            if (equals != std::string::npos && line.substr(0, equals) == association) {
                auto value = line.substr(equals + 1);
                if (!value.empty() && value.back() == '\r')
                    value.pop_back();
                auto path = utf8Path(value);
                if (engine(path))
                    return fs::canonical(path);
            }
        }
    }
#endif
    if (solution.is_object())
        for (const auto &p : solution.at("projects")) {
            auto path = utf8Path(p.at("path"));
            for (int i = 0; i < 12 && !path.empty(); ++i, path = path.parent_path()) {
                if (engine(path))
                    return fs::canonical(path);
                if (path == path.parent_path())
                    break;
            }
        }
    return {};
}
std::vector<std::string> unrealManagedTool(const fs::path &engineRoot, const std::string &tool) {
    if (engineRoot.empty())
        return {};
    auto directory = engineRoot / "Engine/Binaries/DotNET" / tool;
    auto dll = directory / (tool + ".dll");
    if (!file(dll)) {
#ifdef _WIN32
        auto exe = directory / (tool + ".exe");
        if (file(exe))
            return {pathUtf8(exe)};
#endif
        return {};
    }
    auto bundled = engineRoot / "Engine/Binaries/ThirdParty/DotNet";
#ifdef _WIN32
#ifdef _M_ARM64
    const std::vector<std::string> platforms = {"win-arm64"};
#else
    const std::vector<std::string> platforms = {"windows", "win-x64"};
#endif
    const auto name = "dotnet.exe";
#elif defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    const std::vector<std::string> platforms = {"mac-arm64", "osx-arm64"};
#else
    const std::vector<std::string> platforms = {"mac-x64", "osx-x64"};
#endif
    const auto name = "dotnet";
#else
#if defined(__aarch64__)
    const std::vector<std::string> platforms = {"linux-arm64"};
#else
    const std::vector<std::string> platforms = {"linux", "linux-x64"};
#endif
    const auto name = "dotnet";
#endif
    // UE versions use both version/platform and platform/version layouts.
    std::error_code ec;
    if (fs::is_directory(bundled, ec)) {
        for (const auto &item : fs::directory_iterator(bundled)) {
            for (const auto &platform : platforms) {
                for (const auto &candidate : {item.path() / platform / name, item.path() / name})
                    if (file(candidate) && pathUtf8(candidate).find(platform) != std::string::npos)
                        return {pathUtf8(candidate), pathUtf8(dll)};
                if (item.path().filename() == platform)
                    for (const auto &version : fs::directory_iterator(item.path())) {
                        auto candidate = version.path() / name;
                        if (file(candidate))
                            return {pathUtf8(candidate), pathUtf8(dll)};
                    }
            }
        }
    }
#ifdef _WIN32
    auto exe = directory / (tool + ".exe");
    if (file(exe))
        return {pathUtf8(exe)};
#endif
    return {"dotnet", pathUtf8(dll)};
}
UnrealProject::UnrealProject(fs::path root, fs::path entry, Json solution, BuildSystem::Resolver resolve)
    : root_(std::move(root)), entry_(std::move(entry)), solution_(std::move(solution)),
      resolve_(std::move(resolve)) {}
Json UnrealProject::inspect(const Json &params) {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        throw std::runtime_error("An Unreal operation is running; wait or cancel it before refreshing");
    return inspectLocked(params);
}
Json UnrealProject::inspectLocked(const Json &params) {
    Json profile = Json::object();
    auto saved = resolve_(".cycode/unreal.json");
    if (file(saved))
        profile = readJson(saved);
    profile.update(params);
    fs::path project;
    if (profile.contains("project") && !profile.at("project").get<std::string>().empty())
        project = resolve_(profile.at("project"));
    else if (entry_.extension() == ".uproject")
        project = resolve_(pathUtf8(entry_));
    else {
        auto candidate = root_ / entry_.stem();
        candidate += ".uproject";
        if (file(candidate))
            project = resolve_(pathUtf8(candidate));
        else
            for (const auto &item : fs::directory_iterator(root_))
                if (item.path().extension() == ".uproject") {
                    if (!project.empty())
                        throw std::runtime_error("Several Unreal projects: select a .uproject explicitly");
                    project = resolve_(pathUtf8(item.path()));
                }
    }
    if (project.empty())
        return {{"available", false}};
    if (project.extension() != ".uproject" || !file(project))
        throw std::runtime_error("Select an existing .uproject in the workspace");
    auto metadata = readJson(project);
    const auto association = metadata.value("EngineAssociation", std::string{});
    auto engineRoot = profile.value("engineRoot", "").empty() ? findEngine(association, solution_)
                                                              : utf8Path(profile.at("engineRoot"));
    if (!engineRoot.empty() && !engine(engineRoot))
        throw std::runtime_error("Engine directory must contain Engine/Build/Build.version");
    if (!engineRoot.empty())
        engineRoot = fs::canonical(engineRoot);
    Json targets = Json::array(), modules = Json::array(), plugins = Json::array();
    bool truncated = false;
    size_t scanned = 0;
    for (const auto &folder : {project.parent_path() / "Source", project.parent_path() / "Plugins"}) {
        std::error_code ec;
        if (!fs::is_directory(folder, ec))
            continue;
        for (fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec),
             end;
             it != end; it.increment(ec)) {
            if (++scanned > 30000) {
                truncated = true;
                break;
            }
            auto name = pathUtf8(it->path().filename());
            if (it->is_directory(ec)) {
                if (name == "Binaries" || name == "Intermediate" || name == "Content" || name == ".git" ||
                    it.depth() >= 16)
                    it.disable_recursion_pending();
                continue;
            }
            if (!file(it->path()))
                continue;
            auto local = resolve_(pathUtf8(it->path())); // Reject linked rules outside the workspace.
            auto relative = pathUtf8(local.lexically_relative(root_));
            if (name.ends_with(".Target.cs")) {
                name.resize(name.size() - 10);
                targets.push_back({{"name", name}, {"path", relative}});
            } else if (name.ends_with(".Build.cs")) {
                name.resize(name.size() - 9);
                modules.push_back({{"name", name}, {"path", relative}});
            } else if (local.extension() == ".uplugin")
                plugins.push_back({{"name", pathUtf8(local.stem())}, {"path", relative}});
        }
        if (truncated)
            break;
    }
    std::string target;
    for (const auto &item : targets)
        if (target.empty() || item.at("name").get<std::string>().ends_with("Editor"))
            target = item.at("name");
    if (profile.contains("target") && !profile.at("target").get<std::string>().empty())
        target = profile.at("target");
#ifdef _WIN32
    const char *host = "Win64";
#elif defined(__APPLE__)
    const char *host = "Mac";
#else
    const char *host = "Linux";
#endif
    profile = {{"project", pathUtf8(project.lexically_relative(root_))},
               {"engineRoot", pathUtf8(engineRoot)},
               {"target", target},
               {"platform", profile.value("platform", std::string(host))},
               {"configuration", profile.value("configuration", "Development")}};
    Json version = engineRoot.empty() ? Json::object() : readJson(engineRoot / "Engine/Build/Build.version");
    auto command = unrealManagedTool(engineRoot, "UnrealBuildTool");
    auto database = resolve_(
        pathUtf8((target.empty() ? fs::path(".cycode/unreal-analysis/unconfigured") : analysisPath(profile)) /
                 "compile_commands.json"));
    if (!file(database))
        database =
            resolve_(pathUtf8(project.parent_path().lexically_relative(root_) / "compile_commands.json"));
    return {{"available", true},
            {"profile", profile},
            {"name", pathUtf8(project.stem())},
            {"engineAssociation", association},
            {"engineVersion", version},
            {"buildTool", command},
            {"ready", !command.empty()},
            {"targets", targets},
            {"modules", modules},
            {"plugins", plugins},
            {"truncated", truncated},
            {"declaredModules", metadata.value("Modules", Json::array())},
            {"declaredPlugins", metadata.value("Plugins", Json::array())},
            {"compilationDatabase",
             {{"available", file(database)}, {"path", pathUtf8(database.lexically_relative(root_))}}}};
}
Json UnrealProject::run(const std::string &method, const Json &params,
                        const std::atomic<bool> *cancellation) {
    checkBuildCancellation(cancellation);
    auto lock = cancellableBuildLock(mutex_, cancellation);
    return runLocked(method, params, cancellation);
}
Json UnrealProject::runLocked(const std::string &method, const Json &params,
                              const std::atomic<bool> *cancellation) {
    const auto timeout = buildTimeout(params);
    auto model = inspectLocked(params);
    if (!model.at("available").get<bool>())
        throw std::runtime_error("No Unreal .uproject in this workspace");
    if (!model.at("ready").get<bool>())
        throw std::runtime_error(
            "UnrealBuildTool is missing. Select the matching installed Unreal engine directory.");
    auto profile = model.at("profile");
    const std::string target = profile.at("target"), platform = profile.at("platform"),
                      configuration = profile.at("configuration");
    bool known = false;
    for (const auto &t : model.at("targets"))
        known |= t.at("name") == target;
    if (method != "unreal.generate" &&
        (!known || !std::regex_match(target, std::regex("[A-Za-z_][A-Za-z0-9_]*"))))
        throw std::runtime_error("Select an Unreal target discovered from Source/*.Target.cs");
    if (!std::set<std::string>{"Win64", "Linux", "LinuxArm64", "Mac", "Android", "IOS"}.contains(platform))
        throw std::runtime_error("Unsupported Unreal platform");
    if (!std::set<std::string>{"Debug", "DebugGame", "Development", "Shipping", "Test"}.contains(
            configuration))
        throw std::runtime_error("Unsupported Unreal configuration");
    std::vector<std::string> command = model.at("buildTool");
    auto project = resolve_(profile.at("project"));
    if (method == "unreal.generate") {
        command.push_back("-ProjectFiles");
        command.push_back("-Game");
    } else {
        if (method == "unreal.database") {
            command.push_back("-Mode=GenerateClangDatabase");
            command.push_back(
                "-ExecCodeGenActions"); // Run UHT/code generators without compiling the project.
            command.push_back(
                "-ForceHeaderGeneration"); // UBT timestamps alone miss individually deleted UHT outputs.
            auto directory = resolve_(pathUtf8(analysisPath(profile)));
            fs::create_directories(directory);
            command.push_back("-OutputDir=" + pathUtf8(directory));
        } else if (method != "unreal.build")
            throw std::runtime_error("Unknown Unreal operation");
        command.insert(command.end(), {target, platform, configuration});
    }
    command.push_back("-Project=" + pathUtf8(project));
    command.push_back("-WaitMutex");
    command.push_back("-NoHotReloadFromIDE");
    auto cwd = utf8Path(profile.at("engineRoot")) / "Engine/Source";
    if (!fs::is_directory(cwd))
        throw std::runtime_error("Unreal engine Source directory is missing");
    checkBuildCancellation(cancellation);
    auto result = runProcess(command, cwd, timeout, 8 * 1024 * 1024, cancellation,
                             {}, processOutputObserver(cancellation));
    Json response = {{"exitCode", result.exitCode},   {"output", result.output},
                     {"timedOut", result.timedOut},   {"truncated", result.truncated},
                     {"cancelled", result.cancelled}, {"project", {{"available", false}}}};
    try {
        if (!result.exitCode && !result.cancelled && !result.truncated &&
            !(cancellation && cancellation->load())) {
            auto saved = resolve_(".cycode/unreal.json");
            fs::create_directories(saved.parent_path());
            std::ofstream out(saved);
            out << profile.dump(2);
            if (!out)
                throw std::runtime_error("Cannot save Unreal profile");
        }
        response["project"] = inspectLocked(profile);
        if (method == "unreal.build")
            response["artifacts"] = artifactsLocked(response.at("project"));
    } catch (const std::exception &e) {
        response["inspectionError"] = e.what();
    }
    return response;
}
Json UnrealProject::prepare(const Json &params, const std::atomic<bool> *cancellation) {
    buildTimeout(params);
    checkBuildCancellation(cancellation);
    auto lock = cancellableBuildLock(mutex_, cancellation);
    auto model = inspectLocked(params);
    if (!model.value("available", false))
        throw std::runtime_error("No Unreal project to analyse");
    const auto profile = model.at("profile");
    auto directory = resolve_(pathUtf8(analysisPath(profile)));
    auto stamp = directory / "inputs.json";
    std::vector<std::string> inputs{profile.dump(), model.at("engineVersion").dump()};
    auto addRule = [&](const fs::path &path) {
        if (file(path))
            inputs.push_back(pathUtf8(path) + ':' +
                             std::to_string(fs::last_write_time(path).time_since_epoch().count()));
    };
    addRule(resolve_(profile.at("project")));
    for (const auto *section : {"targets", "modules", "plugins"})
        for (const auto &item : model.at(section))
            addRule(resolve_(item.at("path")));
    for (const auto &folder : {root_ / "Source", root_ / "Plugins"}) {
        std::error_code ec;
        if (!fs::is_directory(folder, ec))
            continue;
        size_t count = 0;
        for (fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec),
             end;
             it != end && count++ < 50000; it.increment(ec)) {
            if (cancellation && *cancellation)
                throw std::runtime_error("Unreal analysis cancelled");
            auto name = pathUtf8(it->path().filename());
            if (it->is_directory(ec)) {
                if (name == "Binaries" || name == "Intermediate" || name == "Content" || name == ".git" ||
                    it.depth() >= 20)
                    it.disable_recursion_pending();
            } else if (it->is_regular_file(ec)) {
                inputs.push_back(pathUtf8(it->path().lexically_relative(root_)));
                auto ext = it->path().extension();
                if (ext == ".h" || ext == ".hpp" || ext == ".hxx")
                    addRule(it->path());
            }
        }
    }
    std::sort(inputs.begin(), inputs.end());
    Json signature = inputs;
    bool cached = false;
    if (file(stamp)) {
        try {
            auto saved = readJson(stamp);
            cached =
                saved.is_object() && saved.value("cacheVersion", 0) == 2 && saved.at("inputs") == signature;
            if (cached)
                for (const auto &header : saved.at("generatedHeaders"))
                    if (!file(resolve_(header)))
                        cached = false;
            auto dbPath = resolve_(model.at("compilationDatabase").at("path"));
            if (cached && file(dbPath) && fs::file_size(dbPath) < 64 * 1024 * 1024) {
                std::ifstream in(dbPath);
                auto entries = Json::parse(in);
                cached = entries.is_array() && !entries.empty();
                const std::regex response(R"re(@"([^"]+)"|@([^\s]+))re");
                for (const auto &entry : entries) {
                    const auto command = entry.value("command", "");
                    for (std::sregex_iterator it(command.begin(), command.end(), response), end; it != end;
                         ++it) {
                        auto path = utf8Path((*it)[1].matched ? (*it)[1].str() : (*it)[2].str());
                        if (path.is_relative())
                            path = utf8Path(entry.at("directory")) / path;
                        if (!file(path))
                            cached = false;
                    }
                }
            } else
                cached = false;
        } catch (const std::exception &) {
            cached = false;
        }
    }
    checkBuildCancellation(cancellation);
    if (cached)
        return {{"exitCode", 0},      {"output", "Unreal analysis cache ready.\n"},
                {"timedOut", false},  {"truncated", false},
                {"cancelled", false}, {"project", model},
                {"cached", true}};
    auto launch = profile;
    if (params.contains("timeoutSeconds"))
        launch["timeoutSeconds"] = params.at("timeoutSeconds");
    auto result = runLocked("unreal.database", launch, cancellation);
    if (result.at("project").value("available", false) && !result.contains("inspectionError") &&
        result.at("exitCode") == 0 && !result.value("cancelled", false) &&
        !result.value("truncated", false) && !(cancellation && cancellation->load())) {
        Json headers = Json::array();
        std::error_code ec;
        auto intermediate = root_ / "Intermediate";
        if (fs::is_directory(intermediate, ec)) {
            size_t count = 0;
            for (fs::recursive_directory_iterator
                     it(intermediate, fs::directory_options::skip_permission_denied, ec),
                 end;
                 it != end && count++ < 100000; it.increment(ec))
                if (it->is_regular_file(ec) && pathUtf8(it->path().filename()).ends_with(".generated.h"))
                    headers.push_back(pathUtf8(it->path().lexically_relative(root_)));
        }
        fs::create_directories(directory);
        std::ofstream out(stamp);
        out << Json({{"cacheVersion", 2}, {"inputs", signature}, {"generatedHeaders", headers}}).dump(2);
        if (!out)
            throw std::runtime_error("Cannot save Unreal analysis cache");
    }
    result["cached"] = false;
    return result;
}
} // namespace cycode

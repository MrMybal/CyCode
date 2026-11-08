#include "cycode/ProcessText.h"
#include "cycode/FileTime.h"
#include "cycode/MsBuild.h"
#include "cycode/BuildOperations.h"
#include "cycode/BuildLimits.h"
#include "cycode/Solution.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <regex>
#include <sstream>
#include <thread>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
using Json = nlohmann::json;
namespace fs = std::filesystem;
static std::string lower(std::string text) { for (auto &c : text) c = char(std::tolower(static_cast<unsigned char>(c))); return text; }
static std::string trim(std::string text) { const auto a = text.find_first_not_of(" \r\n\t"); return a == std::string::npos ? "" : text.substr(a,text.find_last_not_of(" \r\n\t")-a+1); }
static bool projectFile(const fs::path &path) { const auto ext = lower(pathUtf8(path.extension())); return ext == ".vcxproj" || ext == ".csproj" || ext == ".fsproj" || ext == ".vbproj"; }
static std::string read(const fs::path &path, size_t limit = 4*1024*1024) {
    std::ifstream input(path,std::ios::binary); if (!input) throw std::runtime_error("Cannot read MSBuild metadata");
    input.seekg(0,std::ios::end); const auto size = input.tellg(); if (size < 0 || size > std::streamoff(limit)) throw std::runtime_error("MSBuild metadata exceeds size limit");
    input.seekg(0); std::string text(size_t(size),0); input.read(text.data(),size); if (!input) throw std::runtime_error("MSBuild metadata changed during read"); return text;
}
static void atomic(const fs::path &path,const std::string &text) {
    if (fs::is_regular_file(path) && read(path,32*1024*1024) == text) return;
    fs::create_directories(path.parent_path()); auto temporary = path; temporary += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    { std::ofstream out(temporary,std::ios::binary); out << text; out.flush(); if (!out) { fs::remove(temporary); throw std::runtime_error("Cannot save MSBuild metadata"); } }
#ifdef _WIN32
    DWORD error = ERROR_SUCCESS;
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { error = ERROR_SUCCESS; break; }
        error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION && error != ERROR_ACCESS_DENIED) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (error != ERROR_SUCCESS) { fs::remove(temporary); throw std::system_error(int(error),std::system_category(),"Cannot replace MSBuild metadata: "+pathUtf8(path)); }
#else
    fs::rename(temporary,path);
#endif
}
static void property(const std::string &value) {
    if (value.empty() || value.size() > 128 || value.find_first_of(";,=%$\"\r\n|") != std::string::npos || value.find('\0') != std::string::npos) throw std::runtime_error("Invalid MSBuild configuration/platform value");
}
static std::string cachePath(const Json &profile) {
    std::uint64_t hash = 14695981039346656037ULL; for (unsigned char c : profile.dump()) { hash ^= c; hash *= 1099511628211ULL; }
    return ".cycode/msbuild-cache/"+std::to_string(hash);
}
static Json inputStamp(const fs::path &path,const fs::path &root) {
    return {{"path",pathUtf8(path.lexically_relative(root))},{"time",fileTimeToken(fs::last_write_time(path))},{"size",fs::file_size(path)}};
}
static Json inputs(const Json &model,const fs::path &root,const BuildSystem::Resolver &resolve,const std::vector<std::string> &additional = {}) {
    std::set<std::string> paths{model.at("profile").at("entry")};
    for (const auto &item : model.at("projects")) if (!item.at("folder").get<bool>() && !item.at("external").get<bool>() && item.at("exists").get<bool>()) paths.insert(item.at("relativePath"));
    for (const auto &path : additional) try { paths.insert(pathUtf8(resolve(path).lexically_relative(root))); } catch (...) {}
    Json result = Json::array(); for (const auto &path : paths) { auto file = resolve(path); if (fs::is_regular_file(file)) result.push_back(inputStamp(file,root)); }
    if (result.size() > 1024) throw std::runtime_error("MSBuild configuration exceeds 1,024 local input files"); return result;
}
static bool fresh(const Json &data,const fs::path &root,const BuildSystem::Resolver &resolve) {
    if (!data.contains("inputs") || !data.at("inputs").is_array() || data.at("inputs").size() > 1024) return false;
    try { for (const auto &stamp : data.at("inputs")) if (inputStamp(resolve(stamp.at("path")),root) != stamp) return false; } catch (...) { return false; } return true;
}
static Json process(const ProcessResult &value) { return {{"exitCode",value.exitCode},{"output",processOutputText(value.output, true)},{"timedOut",value.timedOut},{"truncated",value.truncated},{"cancelled",value.cancelled}}; }
static Json diagnostics(const std::string &output,const fs::path &root,const BuildSystem::Resolver &resolve) {
    static const std::regex diagnostic(R"re(^(?:[0-9]+>)?(.*)\(([0-9]+)(?:,([0-9]+))?(?:,[0-9]+,[0-9]+)?\)\s*:\s*(?:fatal )?(error|warning)\s+([A-Za-z]+[0-9]+)\s*:\s*(.*)$)re");
    Json result = Json::array(); std::set<std::string> seen; std::istringstream lines(output); std::string line;
    while (result.size() < 2000 && std::getline(lines,line)) { line = trim(line); std::smatch match; if (!std::regex_match(line,match,diagnostic)) continue;
        auto name = trim(match[1]); std::string path;
        try { path = pathUtf8(resolve(name).lexically_relative(root)); } catch (...) {
            // MSVC can expand a Windows 8.3 temp-directory alias in its diagnostics.
            // Match the actual ancestor directory identity before resolving relative text.
            std::error_code error; const auto source = fs::weakly_canonical(utf8Path(name),error);
            if (!error && source.is_absolute()) for (auto parent = source.parent_path(); !parent.empty(); parent = parent.parent_path()) {
                if (fs::equivalent(parent,root,error) && !error) { try { path = pathUtf8(resolve(pathUtf8(source.lexically_relative(parent))).lexically_relative(root)); } catch (...) {} break; }
                if (parent == parent.parent_path()) break;
            }
        }
        std::uint64_t row = 1, column = 1; try { row = std::clamp<std::uint64_t>(std::stoull(match[2]),1,10000000); if (match[3].matched) column = std::clamp<std::uint64_t>(std::stoull(match[3]),1,10000000); } catch (...) { continue; }
        Json item = {{"path",path},{"displayPath",name},{"line",row},{"column",column},{"severity",match[4].str()},{"code",match[5].str()},{"message",trim(match[6])}};
        if (seen.insert(item.dump()).second) result.push_back(std::move(item));
    }
    return result;
}
MsBuild::MsBuild(fs::path root,fs::path entry,BuildSystem::Resolver resolve) : root_(std::move(root)),entry_(std::move(entry)),resolve_(std::move(resolve)) {}
Json MsBuild::tools(bool refresh, const std::atomic<bool> *cancellation) {
    auto lock = cancellableBuildLock(mutex_, cancellation); if (!refresh && tools_.is_object()) return tools_;
    Json items = Json::array();
    if (auto exe = toolPath("msbuild"); !exe.empty()) { auto version = runProcess({exe,"-nologo","-version"},root_,std::chrono::seconds(5),4096,cancellation);
        if (!version.exitCode) items.push_back({{"id","msbuild"},{"name","Visual Studio MSBuild"},{"executable",exe},{"version",trim(version.output)},{"nativeCpp",true}}); }
    if (auto exe = toolPath("dotnet"); !exe.empty()) { auto sdks = runProcess({exe,"--list-sdks"},root_,std::chrono::seconds(5),65536,cancellation);
        if (!sdks.exitCode && !trim(sdks.output).empty()) items.push_back({{"id","dotnet"},{"name",".NET SDK / MSBuild"},{"executable",exe},{"version",trim(sdks.output)},{"nativeCpp",false}}); }
    checkBuildCancellation(cancellation); tools_ = {{"tools",items}}; return tools_;
}
Json MsBuild::inspect(const Json &params) { std::lock_guard lock(mutex_); return inspectLocked(params); }
Json MsBuild::inspectLocked(const Json &params) {
    fs::path entry;
    if (fs::is_regular_file(entry_) && (projectFile(entry_) || isSolutionPath(entry_))) entry = entry_;
    else if (auto solution = inspectSolution(root_); solution.is_object()) entry = utf8Path(solution.at("path"));
    else { for (const auto &item : fs::directory_iterator(root_)) if (item.is_regular_file() && projectFile(item.path())) { if (!entry.empty()) { entry.clear(); break; } entry = item.path(); } }
    Json profile = {{"entry",entry.empty() ? "" : pathUtf8(entry.lexically_relative(root_))},{"configuration",""},{"platform",""},{"project",""},{"tool","automatic"}};
    const auto saved = resolve_(".cycode/msbuild.json");
    if (fs::exists(saved)) { auto data = Json::parse(read(saved,65536));
        if (data.value("schemaVersion",0) != 1 || !data.at("profile").is_object()) throw std::runtime_error("Invalid .cycode/msbuild.json profile");
        // Opening a different explicit solution/project in the same directory keeps its identity.
        if (entry.empty() || data.at("profile").value("entry","") == profile.at("entry").get<std::string>()) for (auto i = data.at("profile").begin(); i != data.at("profile").end(); ++i) { if (!profile.contains(i.key()) || !i->is_string()) throw std::runtime_error("Invalid MSBuild profile field"); profile[i.key()] = i.value(); }
    }
    const auto previousEntry = profile.at("entry");
    for (auto i = profile.begin(); i != profile.end(); ++i) if (params.contains(i.key())) profile[i.key()] = params.at(i.key()).get<std::string>();
    if (params.contains("entry") && profile.at("entry") != previousEntry) { if (!params.contains("project")) profile["project"] = ""; if (!params.contains("configuration")) profile["configuration"] = ""; if (!params.contains("platform")) profile["platform"] = ""; }
    Json model = {{"available",false},{"profile",profile},{"configurations",Json::array()},{"projects",Json::array()},{"artifacts",Json::array()},{"tools",Json::array()},
                  {"compilationDatabase",{{"available",false},{"path",""},{"entries",0}}},{"analysisWarnings",Json::array()},{"message","Open a .sln, .vcxproj or managed project."}};
    if (profile.at("entry") == "") return model;
    entry = resolve_(profile.at("entry")); if (!fs::is_regular_file(entry) || (!projectFile(entry) && !isSolutionPath(entry))) throw std::runtime_error("Select a workspace .sln/.slnx or MSBuild project");
    profile["entry"] = pathUtf8(entry.lexically_relative(root_));
    const auto solution = inspectSolution(entry);
    if (solution.is_object()) { model["projects"] = solution.at("projects"); model["configurations"] = solution.at("configurations"); }
    else model["projects"].push_back({{"id",profile.at("entry")},{"name",pathUtf8(entry.stem())},{"path",pathUtf8(entry)},{"relativePath",profile.at("entry")},{"exists",true},{"external",false},{"folder",false},{"parent",""},{"configurations",Json::object()}});
    if (profile.at("configuration") == "" || profile.at("platform") == "") {
        if (!model.at("configurations").empty()) { auto first = model.at("configurations")[0].get<std::string>(); auto split = first.find('|'); if (split == std::string::npos) throw std::runtime_error("Invalid solution configuration");
            if (profile.at("configuration") == "") profile["configuration"] = first.substr(0,split); if (profile.at("platform") == "") profile["platform"] = first.substr(split+1);
        } else { if (profile.at("configuration") == "") profile["configuration"] = "Debug"; if (profile.at("platform") == "") profile["platform"] = lower(pathUtf8(entry.extension())) == ".vcxproj" ? "x64" : "AnyCPU"; }
    }
    property(profile.at("configuration")); property(profile.at("platform"));
    const auto selectedConfiguration = profile.at("configuration").get<std::string>()+"|"+profile.at("platform").get<std::string>();
    if (solution.is_object() && std::find(model.at("configurations").begin(),model.at("configurations").end(),selectedConfiguration) == model.at("configurations").end()) throw std::runtime_error("Unknown solution configuration/platform");
    bool selected = profile.at("project") == "";
    for (auto &item : model.at("projects")) {
        const auto &mappings = item.at("configurations"); item["activeConfiguration"] = mappings.value(selectedConfiguration,Json{{"configuration",profile.at("configuration")},{"platform",profile.at("platform")},{"build",!solution.is_object()},{"deploy",false}});
        if (!item.at("folder").get<bool>() && (profile.at("project") == item.at("id") || profile.at("project") == item.at("relativePath"))) { profile["project"] = item.at("id"); selected = true; if (item.at("external").get<bool>()) throw std::runtime_error("Select a project inside the workspace"); }
    }
    if (!selected) throw std::runtime_error("Unknown MSBuild project selection");
    if (profile.at("tool") != "automatic" && profile.at("tool") != "msbuild" && profile.at("tool") != "dotnet") throw std::runtime_error("Unknown MSBuild tool");
    model["available"] = true; model["profile"] = profile; model["tools"] = tools_.is_object() ? tools_.at("tools") : Json::array();
    model["message"] = !tools_.is_object() ? "Detect MSBuild tools to select a build engine." : model.at("tools").empty() ? "Install Visual Studio Build Tools or a .NET SDK." : "";
    const auto metadata = resolve_(cachePath(profile)+"/model.json");
    if (fs::is_regular_file(metadata)) { auto data = Json::parse(read(metadata)); if (data.value("profile",Json()) == profile && fresh(data,root_,resolve_)) {
        model["artifacts"] = data.value("artifacts",Json::array()); model["analysisWarnings"] = data.value("analysisWarnings",Json::array());
        const auto database = resolve_(cachePath(profile)+"/compile_commands.json"); if (data.value("entries",0) > 0 && fs::is_regular_file(database)) model["compilationDatabase"] = {{"available",true},{"path",pathUtf8(database.lexically_relative(root_))},{"entries",data.at("entries")}};
    } }
    return model;
}
std::vector<std::string> MsBuild::command(const Json &model, const std::atomic<bool> *cancellation) {
    const auto tool = model.at("profile").at("tool").get<std::string>();
    const auto discovered = tools(false,cancellation);
    for (const auto &item : discovered.at("tools")) if (tool == "automatic" || item.at("id") == tool) {
        if (item.at("id") == "dotnet") for (const auto &project : model.at("projects")) if (!project.at("folder").get<bool>() && lower(pathUtf8(utf8Path(project.at("path")).extension())) == ".vcxproj") throw std::runtime_error("C++ projects require Visual Studio MSBuild on Windows");
        std::vector<std::string> result{item.at("executable")}; if (item.at("id") == "dotnet") result.push_back("msbuild");
        result.insert(result.end(),{"-nologo","-noAutoResponse","-nodeReuse:false","-verbosity:minimal"}); return result;
    }
    throw std::runtime_error("The selected MSBuild tool is unavailable");
}
Json MsBuild::evaluate(const Json &model,const Json &item,const std::atomic<bool> *cancellation) {
    checkBuildCancellation(cancellation);
    if (!item.at("exists").get<bool>() || item.at("external").get<bool>() || item.at("folder").get<bool>()) throw std::runtime_error("Select an existing workspace project to evaluate");
    const auto file = resolve_(item.at("relativePath")); auto args = command(model,cancellation);
    const auto &config = item.at("activeConfiguration"); property(config.at("configuration")); property(config.at("platform"));
    args.insert(args.end(),{pathUtf8(file),"-p:Configuration="+config.at("configuration").get<std::string>(),"-p:Platform="+config.at("platform").get<std::string>(),
        "-getProperty:TargetPath,TargetFramework,TargetFrameworks,Configuration,Platform,ConfigurationType,VCToolsInstallDir,IncludePath,MSBuildProjectDirectory,AssemblyName,MSBuildAllProjects",
        "-getItem:ClCompile,ClInclude,ProjectReference"});
    auto value = runProcess(args,root_,std::chrono::seconds(15),8*1024*1024,cancellation); auto result = process(value); result["project"] = item;
    if (!value.exitCode && !value.truncated && !value.cancelled) { const auto start = value.output.find('{'), end = value.output.rfind('}');
        auto metadata = start == std::string::npos ? Json() : Json::parse(value.output.substr(start,end-start+1),nullptr,false);
        if (!metadata.is_object() || !metadata.contains("Properties") || !metadata.contains("Items")) throw std::runtime_error("MSBuild 17.8+ is required for structured project evaluation"); result["evaluation"] = metadata;
    } else result["evaluation"] = nullptr;
    return result;
}
static std::string targetName(const Json &model,const Json &item) {
    auto escaped = [](std::string value) { for (auto &c : value) if (std::string("%$@;.()'").find(c) != std::string::npos) c = '_'; if (value.find_first_of(":,\r\n\\/") != std::string::npos) throw std::runtime_error("Unsupported solution target name"); return value; };
    std::string result = escaped(item.at("name")); auto parent = item.value("parent","");
    while (!parent.empty()) { bool found = false; for (const auto &p : model.at("projects")) if (p.at("id") == parent) { result = escaped(p.at("name"))+"\\"+result; parent = p.at("parent"); found = true; break; } if (!found) throw std::runtime_error("Invalid solution target parent"); }
    return result;
}
static std::vector<std::string> list(const std::string &text) { std::vector<std::string> result; std::istringstream in(text); std::string part; while (std::getline(in,part,';')) { part = trim(part); if (!part.empty() && part.find("%(") == std::string::npos && part.find("$(") == std::string::npos) result.push_back(part); } return result; }
Json MsBuild::run(const std::string &method,const Json &params,const std::atomic<bool> *cancellation) {
    buildTimeout(params,150);
    checkBuildCancellation(cancellation);
    auto lock = cancellableBuildLock(mutex_,cancellation);
    auto model = inspectLocked(params); if (!model.at("available").get<bool>()) throw std::runtime_error("Open a solution or MSBuild project first");
    const auto profile = model.at("profile"); const auto entry = resolve_(profile.at("entry"));
    Json chosen;
    for (const auto &item : model.at("projects")) if (!item.at("folder").get<bool>() && (profile.at("project") == item.at("id") || (profile.at("project") == "" && !chosen.is_object() && item.at("exists").get<bool>() && !item.at("external").get<bool>() && item.at("activeConfiguration").value("build",false)))) chosen = item;
    auto save = [&] { atomic(resolve_(".cycode/msbuild.json"),Json{{"schemaVersion",1},{"profile",profile}}.dump(2)); };
    if (method == "msbuild.build" || method == "msbuild.rebuild" || method == "msbuild.clean" || method == "msbuild.restore") {
        auto args = command(model,cancellation); std::string target = method.substr(8); target[0] = char(std::toupper(static_cast<unsigned char>(target[0])));
        auto buildEntry = entry; auto configuration = profile.at("configuration").get<std::string>(), platform = profile.at("platform").get<std::string>();
        if (profile.at("project") != "" && isSolutionPath(entry)) {
            if (method == "msbuild.restore") { buildEntry = resolve_(chosen.at("relativePath")); configuration = chosen.at("activeConfiguration").at("configuration"); platform = chosen.at("activeConfiguration").at("platform"); }
            else target = targetName(model,chosen)+(method == "msbuild.build" ? "" : ":"+target);
        }
        args.insert(args.end(),{pathUtf8(buildEntry),"-maxCpuCount:4","-p:Configuration="+configuration,"-p:Platform="+platform,"-target:"+target});
        if (params.value("restore",false) && method != "msbuild.restore") args.push_back("-restore");
        checkBuildCancellation(cancellation);
        auto result = process(runProcess(args,root_,buildTimeout(params,150),8*1024*1024,cancellation,{},processOutputObserver(cancellation))); if (result.at("exitCode") == 0 && !result.value("cancelled",false) && !result.value("truncated",false) && !(cancellation&&cancellation->load())) save(); result["project"] = inspectLocked(profile); result["diagnostics"] = diagnostics(result.at("output"),root_,resolve_); return result;
    }
    if (method == "msbuild.evaluate") {
        if (!chosen.is_object()) throw std::runtime_error("Select a workspace project to evaluate"); auto result = evaluate(model,chosen,cancellation);
        if (result.at("exitCode") == 0 && !result.at("evaluation").is_null() && !(cancellation&&cancellation->load())) {
            const auto &props = result.at("evaluation").at("Properties"); Json artifact = {{"project",chosen.at("id")},{"name",chosen.at("name")},{"path",props.value("TargetPath","")},{"framework",props.value("TargetFramework","")},{"type",props.value("ConfigurationType","")}};
            auto sources = list(props.value("MSBuildAllProjects","")); const auto cached = resolve_(cachePath(profile)+"/model.json");
            if (fs::is_regular_file(cached)) { const auto previous = Json::parse(read(cached)); if (fresh(previous,root_,resolve_)) for (const auto &stamp : previous.at("inputs")) sources.push_back(stamp.at("path")); }
            model["artifacts"] = Json::array({artifact}); atomic(cached,Json{{"profile",profile},{"artifacts",model.at("artifacts")},{"entries",model.at("compilationDatabase").value("entries",0)},{"analysisWarnings",model.at("analysisWarnings")},{"inputs",inputs(model,root_,resolve_,sources)}}.dump(2)); save();
        }
        result["project"] = inspectLocked(profile); return result;
    }
    if (method != "msbuild.prepare") throw std::runtime_error("Unsupported MSBuild operation");
    Json database = Json::array(), artifacts = Json::array(), warnings = Json::array(); ProcessResult combined; combined.exitCode = 0; int evaluated = 0; size_t databaseBytes = 0;
    auto warning = [&](const std::string &text) { if (warnings.size() < 128) warnings.push_back(text); };
    std::vector<std::string> configurationInputs;
    for (const auto &item : model.at("projects")) {
        checkBuildCancellation(cancellation);
        if (item.at("folder").get<bool>() || item.at("external").get<bool>() || !item.at("exists").get<bool>() || lower(pathUtf8(utf8Path(item.at("path")).extension())) != ".vcxproj") continue;
        if (profile.at("project") != "" && profile.at("project") != item.at("id")) continue;
        if (profile.at("project") == "" && !item.at("activeConfiguration").value("build",false)) continue;
        if (++evaluated > 8) { warning("Analysis is limited to eight C++ projects per operation. Select another project to prepare its own index."); break; }
        auto value = evaluate(model,item,cancellation); combined.output += value.at("output").get<std::string>().substr(0,8192);
        if (value.at("exitCode") != 0 || value.at("evaluation").is_null() || value.value("cancelled",false)) { combined.exitCode = value.at("exitCode") == 0 ? 1 : value.at("exitCode").get<int>(); combined.truncated = value.at("truncated"); combined.timedOut = value.at("timedOut"); combined.cancelled=value.value("cancelled",false); break; }
        const auto &evaluation = value.at("evaluation"), &props = evaluation.at("Properties"); const auto directory = utf8Path(props.at("MSBuildProjectDirectory"));
        for (const auto &path : list(props.value("MSBuildAllProjects",""))) configurationInputs.push_back(path);
        artifacts.push_back({{"project",item.at("id")},{"name",item.at("name")},{"path",props.value("TargetPath","")},{"framework",props.value("TargetFramework","")},{"type",props.value("ConfigurationType","")}});
        auto tools = utf8Path(props.value("VCToolsInstallDir","")); const auto platform = lower(props.at("Platform"));
        auto compiler = tools/"bin/Hostx64"/(platform == "win32" ? "x86" : platform == "arm64" ? "arm64" : "x64")/"cl.exe";
        for (const auto &file : evaluation.at("Items").value("ClCompile",Json::array())) {
            checkBuildCancellation(cancellation);
            if (lower(file.value("ExcludedFromBuild","false")) == "true") continue;
            if (database.size() >= 20000) throw std::runtime_error("MSBuild analysis exceeds 20,000 translation units");
            fs::path source; try { source = resolve_(pathUtf8(directory/utf8Path(file.at("Identity")))); } catch (...) { warning("Skipped a source outside the workspace: "+file.at("Identity").get<std::string>()); continue; }
            Json args = Json::array({fs::is_regular_file(compiler) ? pathUtf8(compiler) : "cl.exe","/nologo","/c",pathUtf8(source)});
            auto addPaths = [&](const std::string &value,const char *flag) { for (const auto &path : list(value)) { args.push_back(flag); args.push_back(pathUtf8((directory/utf8Path(path)).lexically_normal())); } };
            addPaths(props.value("IncludePath",""),"/I"); addPaths(file.value("AdditionalIncludeDirectories",""),"/I"); addPaths(file.value("ForcedIncludeFiles",""),"/FI");
            for (const auto &define : list(file.value("PreprocessorDefinitions",""))) args.push_back("/D"+define);
            const auto standard = file.value("LanguageStandard",""); if (!standard.empty()) { if (standard == "stdcpplatest") args.push_back("/std:c++latest"); else if (standard.starts_with("stdcpp")) args.push_back("/std:c++"+standard.substr(6)); }
            const auto compileAs = file.value("CompileAs",""); if (compileAs == "CompileAsC") args.push_back("/TC"); else if (compileAs == "CompileAsCpp") args.push_back("/TP");
            if (!file.value("AdditionalOptions","").empty()) warning("AdditionalOptions require review for "+file.at("Identity").get<std::string>()+"; the derived index uses evaluated includes, definitions, language mode and forced includes.");
            Json unit = {{"directory",pathUtf8(directory)},{"file",pathUtf8(source)},{"arguments",args}};
            databaseBytes += unit.dump(2).size()+16; if (databaseBytes > 32*1024*1024) throw std::runtime_error("MSBuild analysis exceeds 32 MiB; select a smaller project"); database.push_back(std::move(unit));
        }
    }
    checkBuildCancellation(cancellation);
    if (!evaluated) throw std::runtime_error("No enabled local C++ project to index");
    if (!combined.exitCode && database.empty()) { combined.exitCode = 1; combined.output += "\nNo supported C++ translation units were evaluated.\n"; }
    if (!combined.exitCode) { const auto text = database.dump(2); if (text.size() > 32*1024*1024) throw std::runtime_error("MSBuild analysis exceeds 32 MiB; select a smaller project"); atomic(resolve_(cachePath(profile)+"/compile_commands.json"),text); atomic(resolve_(cachePath(profile)+"/model.json"),Json{{"profile",profile},{"artifacts",artifacts},{"entries",database.size()},{"analysisWarnings",warnings},{"inputs",inputs(model,root_,resolve_,configurationInputs)}}.dump(2)); save(); }
    auto result = process(combined); result["project"] = inspectLocked(profile); return result;
}
} // namespace cycode

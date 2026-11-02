#include "cycode/Engine.h"
#include "cycode/Solution.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include "cycode/LanguageInfo.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace cycode {
static constexpr std::size_t maxFile = maxTextFileBytes;
static std::string readFile(const std::filesystem::path &path) {
    return readTextFileBytes(path);
}
static void validateText(const std::string &text) {
    validateEditorText(text);
}
static std::string normalize(std::string text) {
    return normalizeEditorText(text);
}
Engine::Engine(std::filesystem::path root)
    : root_(workspaceRoot(root)), entry_(std::filesystem::canonical(root)), solution_(inspectSolution(root)),
      build_(root_, [this](const std::string &path) { return resolve(path); }),
      msbuild_(root_,entry_,[this](const std::string &path) { return resolve(path); }),
      unreal_(root_, std::filesystem::canonical(root), solution_,
              [this](const std::string &path) { return resolve(path); }),
      language_(root_), projects_(root_,entry_,[this](const std::string &path) { return resolve(path); },terminals_),
      debugger_(root_, [this](const std::string &path) { return resolve(path); }, terminals_) {
    if (!std::filesystem::is_directory(root_))
        throw std::runtime_error("Workspace must be a directory");
    addSdkRoots(Json::object());
    language_.setProject(entry_);
    try {
        language_.setCompilationDatabase(build_.activeDatabaseDirectory());
        const auto msbuild = msbuild_.inspect();
        if (msbuild.at("compilationDatabase").at("available").get<bool>()) language_.setCompilationDatabase(resolve(msbuild.at("compilationDatabase").at("path")).parent_path());
        auto unreal = unreal_.inspect();
        addSdkRoots(unreal);
        if (unreal.value("available", false) && unreal.at("compilationDatabase").value("available", false))
            language_.setCompilationDatabase(resolve(unreal.at("compilationDatabase").at("path")).parent_path());
    } catch (
        const std::exception &) { /* Build inspection reports invalid profiles without preventing editing. */
    }
}
std::filesystem::path Engine::resolve(const std::string &relative) const {
    auto input = utf8Path(relative);
    auto full = std::filesystem::weakly_canonical(input.is_absolute() ? input : root_ / input);
    auto a = root_.begin(), b = full.begin();
    for (; a != root_.end(); ++a, ++b) {
        if (b == full.end())
            throw std::runtime_error("Path is outside the workspace");
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()) != 0)
            throw std::runtime_error("Path is outside the workspace");
#else
        if (*a != *b)
            throw std::runtime_error("Path is outside the workspace");
#endif
    }
    return full;
}
Json Engine::documentJson(const std::string &key, const Document &doc) const {
    return {{"path", key},
            {"text", doc.text},
            {"revision", doc.revision},
            {"dirty", doc.dirty},
            {"lineEnding", doc.crlf     ? "CRLF"
                           : doc.crOnly ? "CR"
                                        : "LF"},
            {"encoding", doc.bom ? "UTF-8 BOM" : "UTF-8"}};
}
Engine::Document &Engine::openLocked(const std::string &path) {
    auto full = resolve(path);
    auto key = pathUtf8(full.lexically_relative(root_));
    auto found = documents_.find(key);
    if (found != documents_.end())
        return found->second;
    auto raw = readFile(full);
    Document doc;
    doc.revision = nextRevision_++;
    doc.disk = raw;
    doc.bom = raw.starts_with("\xef\xbb\xbf");
    if (doc.bom)
        raw.erase(0, 3);
    validateText(raw);
    doc.crlf = raw.find("\r\n") != std::string::npos;
    doc.crOnly = raw.find('\r') != std::string::npos && raw.find('\n') == std::string::npos;
    doc.text = normalize(raw);
    doc.saved = doc.text;
    auto &opened = documents_.emplace(key, std::move(doc)).first->second;
    language_.sync(key, opened.text, opened.revision);
    return opened;
}
void Engine::configureLanguage(const std::string &executable, bool enabled, bool prewarm) {
    language_.configure(executable, enabled, prewarm);
}
Json Engine::prepareUnrealAnalysis(const Json &params, const std::atomic<bool> *cancellation) {
    auto result = unreal_.prepare(params, cancellation);
    addSdkRoots(result.at("project"));
    if (result.at("exitCode") == 0) {
        const auto &db = result.at("project").at("compilationDatabase");
        if (db.at("available").get<bool>()) language_.setCompilationDatabase(resolve(db.at("path")).parent_path(), true);
    }
    return result;
}
Json Engine::languageQuery(const std::string &method, const Json &params) {
    const auto supplied = params.at("path").get<std::string>();
    const auto path = supplied.starts_with("sdk://") ? supplied : pathUtf8(resolve(supplied).lexically_relative(root_));
    if (!LanguageService::supports(path))
        throw std::runtime_error("No language backend for this file type");
    std::future<Json> reply;
    std::uint64_t revision, generation, epoch;
    const auto &position = params.at("position");
    auto line = position.at("line").get<std::int64_t>();
    auto character = position.at("character").get<std::int64_t>();
    if (line < 0 || character < 0)
        throw std::runtime_error("Position must be zero-based UTF-16");
    {
        std::lock_guard lock(mutex_);
        auto [document, full] = semanticDocumentLocked(path);
        auto &doc = *document;
        revision = doc.revision;
        generation = nextRevision_;
        epoch = language_.epoch(path);
        if (params.at("expectedRevision").get<std::uint64_t>() != revision)
            throw std::runtime_error("RevisionConflict: document changed; read it again");
        size_t start = 0;
        for (std::int64_t i = 0; i < line; ++i) {
            auto next = doc.text.find('\n', start);
            if (next == std::string::npos)
                throw std::runtime_error("Position is outside document");
            start = next + 1;
        }
        auto end = doc.text.find('\n', start);
        if (end == std::string::npos)
            end = doc.text.size();
        auto text = doc.text.substr(start, end - start);
        if (static_cast<size_t>(character) > LanguageClient::utf16Length(text))
            throw std::runtime_error("Position is outside line");
        // Reject positions inside a surrogate pair instead of silently targeting another symbol.
        size_t units = 0;
        for (unsigned char c : text) {
            if ((c & 0xc0) == 0x80)
                continue;
            if (units == static_cast<size_t>(character))
                break;
            units += c >= 0xf0 ? 2 : 1;
        }
        if (units != static_cast<size_t>(character))
            throw std::runtime_error("Position splits a UTF-16 surrogate pair");
        Json request = {{"textDocument", {{"uri", LanguageClient::uri(full)}}},
                        {"position", position}};
        if (method == "language.references")
            request["context"] = {{"includeDeclaration", params.value("includeDeclaration", true)}};
        if (method == "language.signature") request["context"] = {{"triggerKind",1},{"isRetrigger",false}};
        // Engine's mutex orders document changes and requests; the service owns the transport.
        reply = language_.request(method == "language.definition"   ? "textDocument/definition"
                                  : method == "language.references" ? "textDocument/references"
                                  : method == "language.hover"      ? "textDocument/hover"
                                  : method == "language.signature"  ? "textDocument/signatureHelp"
                                                                    : "textDocument/completion",
                                  std::move(request));
    }
    auto result = reply.get(); // Backend deadline includes initialization (15s C++, 60s C#).
    std::lock_guard lock(mutex_);
    const auto current = path.starts_with("sdk://")
        ? (externalSources_.contains(path) && externalSources_.at(path).loaded ? &externalSources_.at(path).document : nullptr)
        : (documents_.contains(path) ? &documents_.at(path) : nullptr);
    if (!current || current->revision != revision || generation != nextRevision_ || language_.epoch(path)!=epoch)
        throw std::runtime_error("RevisionConflict: document changed during language request");
    if (method == "language.complete")
        return issueCompletion(path,revision,generation,epoch,position,result);
    if (method=="language.hover" || method=="language.signature") {
        auto information=method=="language.hover" ? hoverInformation(result,current->text) : signatureInformation(result);
        information["path"]=path;information["revision"]=revision;information["positionEncoding"]="utf-16";return information;
    }
    Json locations = Json::array();
    std::set<std::string> unique;
    size_t outside = 0;
    bool truncated = false;
    auto limit = static_cast<size_t>(std::clamp(params.value("limit", 1000), 1, 5000));
    if (result.is_object())
        result = Json::array({result});
    if (result.is_array())
        for (const auto &location : result) {
            if (!location.is_object())
                continue;
            const auto uri = location.value("targetUri", location.value("uri", std::string{}));
            const auto range = location.value("targetSelectionRange", location.value("range", Json()));
            if (!range.is_object())
                continue;
            Json entry;
            try {
                entry = sourceLocationLocked(uri, range, params.value("includeExternal", false));
            } catch (const std::exception &) {
                ++outside;
                continue;
            }
            if (entry.is_null()) { ++outside; continue; }
            if (!unique.insert(entry.dump()).second)
                continue;
            if (locations.size() >= limit) {
                truncated = true;
                continue;
            }
            locations.push_back(std::move(entry));
        }
    std::sort(locations.begin(), locations.end(), [](const Json &a, const Json &b) {
        if (a["path"] != b["path"])
            return a["path"] < b["path"];
        const auto &x = a["range"]["start"], &y = b["range"]["start"];
        return x["line"] == y["line"] ? x["character"] < y["character"] : x["line"] < y["line"];
    });
    return {{"path", path},           {"revision", revision},        {"positionEncoding", "utf-16"},
            {"locations", locations}, {"outsideWorkspace", outside}, {"truncated", truncated}};
}
Json Engine::files() {
    auto result = runProcess({"rg", "--files", "--hidden", "--no-require-git", "-0", "-g", "!.git", "-g",
                              "!runtime", "-g", "!data", "-g", "!.cycode"},
                             root_);
    Json paths = Json::array();
    if (result.exitCode != 0 && result.exitCode != 1)
        throw std::runtime_error(result.output);
    std::size_t start = 0;
    while (start < result.output.size() && paths.size() < 20000) {
        auto end = result.output.find('\0', start);
        if (end == std::string::npos)
            break;
        auto path = result.output.substr(start, end - start);
        std::replace(path.begin(), path.end(), '\\', '/');
        paths.push_back(path);
        start = end + 1;
    }
    std::sort(paths.begin(), paths.end());
    return {{"files", paths}, {"truncated", result.truncated || paths.size() >= 20000}};
}
Json Engine::searchText(const Json &params, const std::atomic<bool> *cancellation) {
    auto query = params.at("query").get<std::string>();
    if (query.empty())
        return {{"matches", Json::array()}, {"truncated", false}};
    auto limit = std::clamp(params.value("limit", 500), 1, 5000);
    bool sensitive = params.value("caseSensitive", false);
    bool regex = params.value("regex", false);
    std::vector<std::string> args{"rg", "--json", "--hidden", "--no-require-git",
                                  "-g", "!.git",  "-g",       "!runtime",
                                  "-g", "!data",  "-g",       "!.cycode"};
    if (!sensitive)
        args.push_back("--ignore-case");
    if (!regex)
        args.push_back("--fixed-strings");
    args.insert(args.end(), {"--", query, "."});
    auto result = runProcess(args, root_, std::chrono::seconds(15), 8 * 1024 * 1024, cancellation);
    if (result.cancelled)
        return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
    if (result.exitCode > 1 && !result.truncated && !result.timedOut)
        throw std::runtime_error(result.output);
    Json matches = Json::array();
    std::map<std::string, std::string> dirty;
    {
        std::lock_guard guard(mutex_);
        for (const auto &[path, doc] : documents_)
            if (doc.dirty)
                dirty[path] = doc.text;
    }
    auto record = [&](std::string path, size_t line, size_t column, std::string text) {
        if (text.size() > 512)
            text.resize(512);
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
            text.pop_back();
        if (matches.size() < static_cast<size_t>(limit))
            matches.push_back({{"path", path}, {"line", line}, {"column", column}, {"text", text}});
    };
    std::istringstream stream(result.output);
    std::string line;
    bool over = false;
    while (std::getline(stream, line)) {
        auto item = Json::parse(line, nullptr, false);
        if (item.is_discarded() || item.value("type", "") != "match")
            continue;
        const auto &d = item["data"];
        if (!d["path"].contains("text") || !d["lines"].contains("text"))
            continue;
        std::string path = d["path"]["text"];
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.starts_with("./"))
            path.erase(0, 2);
        if (dirty.contains(path))
            continue;
        for (const auto &sub : d["submatches"]) {
            if (matches.size() >= static_cast<size_t>(limit)) {
                over = true;
                break;
            }
            record(path, d["line_number"].get<size_t>(), sub["start"].get<size_t>() + 1, d["lines"]["text"]);
        }
    }
    // Unsaved buffers supersede disk results. Regex stays delegated to ripgrep,
    // using a private stdin-free temporary snapshot only in a later milestone.
    if (!regex)
        for (const auto &[path, text] : dirty) {
            std::istringstream lines(text);
            size_t number = 0;
            while (std::getline(lines, line)) {
                ++number;
                std::string needle = query, haystack = line;
                if (!sensitive) {
                    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
                    std::transform(needle.begin(), needle.end(), needle.begin(), lower);
                    std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
                }
                size_t at = 0;
                while ((at = haystack.find(needle, at)) != std::string::npos) {
                    if (matches.size() >= static_cast<size_t>(limit)) {
                        over = true;
                        break;
                    }
                    record(path, number, at + 1, line);
                    at += std::max<size_t>(needle.size(), 1);
                }
            }
        }
    return {{"matches", matches},
            {"truncated", over || result.truncated},
            {"timedOut", result.timedOut},
            {"unsavedRegexExcluded", regex && !dirty.empty()}};
}
Json Engine::git(const std::string &method, const Json &params) {
    std::lock_guard vcs(vcsMutex_);
    if (method != "vcs.status" && method != "vcs.diff" && method != "vcs.log" && method != "vcs.stage" && method != "vcs.unstage" && method != "vcs.commit") return gitAdvanced(method,params);
    std::vector<std::string> args{"git", "-C", pathUtf8(root_)};
    if (method == "vcs.status")
        args.insert(args.end(),
                    {"status", "--porcelain=v1", "-z", "--branch", "--untracked-files=all", "--", "."});
    else if (method == "vcs.diff") {
        args.insert(args.end(), {"diff", "--no-ext-diff", "--no-color"});
        if (params.value("staged", false))
            args.push_back("--cached");
        args.push_back("--");
        if (params.contains("path"))
            args.push_back(pathUtf8(resolve(params.at("path")).lexically_relative(root_)));
        else
            args.push_back(".");
    } else if (method == "vcs.log")
        args.insert(args.end(), {"log", "-30", "--date=short", "--format=%h %ad %s", "--", "."});
    else if (method == "vcs.stage") {
        args.push_back("add");
        args.push_back("--");
        args.push_back(pathUtf8(resolve(params.at("path")).lexically_relative(root_)));
    } else if (method == "vcs.unstage") {
        auto head = runProcess({"git", "-C", pathUtf8(root_), "rev-parse", "--verify", "HEAD"}, root_);
        if (head.exitCode == 0)
            args.insert(args.end(), {"restore", "--staged", "--"});
        else
            args.insert(args.end(), {"rm", "--cached", "--ignore-unmatch", "--"});
        args.push_back(pathUtf8(resolve(params.at("path")).lexically_relative(root_)));
    } else if (method == "vcs.commit") {
        auto message = params.at("message").get<std::string>();
        if (message.empty())
            throw std::runtime_error("Commit message is empty");
        args.insert(args.end(), {"commit", "-m", message});
    } else
        throw std::runtime_error("Unsupported Git operation");
    auto result = runProcess(args, root_);
    if (method == "vcs.status") {
        auto location = runProcess({"git", "-C", pathUtf8(root_), "rev-parse", "--show-toplevel"}, root_);
        auto repository = location.output;
        while (!repository.empty() && (repository.back() == '\n' || repository.back() == '\r'))
            repository.pop_back();
        Json entries = Json::array();
        std::string branch;
        size_t start = 0;
        while (start < result.output.size()) {
            auto end = result.output.find('\0', start);
            if (end == std::string::npos)
                break;
            auto record = result.output.substr(start, end - start);
            start = end + 1;
            if (record.starts_with("## ")) {
                branch = record.substr(3);
                continue;
            }
            if (record.size() < 4)
                continue;
            auto path = record.substr(3);
            if (location.exitCode == 0)
                path = pathUtf8((utf8Path(repository) / utf8Path(path)).lexically_relative(root_));
            bool renamed = record[0] == 'R' || record[0] == 'C' || record[1] == 'R' || record[1] == 'C';
            if (renamed) {
                auto next = result.output.find('\0', start);
                if (next != std::string::npos)
                    start = next + 1;
            }
            entries.push_back({{"status", record.substr(0, 2)}, {"path", path}});
        }
        return {{"provider", "git"},
                {"repository", result.exitCode == 0},
                {"branch", branch},
                {"files", entries},
                {"message", result.exitCode ? result.output : ""}};
    }
    return {{"exitCode", result.exitCode},
            {"output", result.output},
            {"timedOut", result.timedOut},
            {"truncated", result.truncated}};
}
Json Engine::dispatch(const std::string &method, const Json &params) {
    if (method=="project.inspect") return projects_.inspect(params);
    if (method=="project.cancel") return projects_.cancel(params.value("operationId",""));
    if (method.starts_with("project.")) {
        { std::lock_guard lock(mutex_);for (const auto &[path,document] : documents_) if (document.dirty) throw std::runtime_error("UnsavedDocuments: save "+path+" before executing project tasks"); }
        return projects_.execute(method,params);
    }
    if (method == "msbuild.inspect") return msbuild_.inspect(params);
    if (method == "msbuild.tools") return msbuild_.tools(params.value("refresh",false));
    if (method.starts_with("msbuild.")) {
        auto result = msbuild_.run(method,params);
        if (method == "msbuild.prepare" && result.at("exitCode") == 0 && result.at("project").at("compilationDatabase").at("available").get<bool>())
            language_.setCompilationDatabase(resolve(result.at("project").at("compilationDatabase").at("path")).parent_path(),true);
        return result;
    }
    if (method.starts_with("debug.")) return debugger_.execute(method,params);
    if (method == "terminal.create") return terminals_.create(resolve(params.value("cwd", ".")), Terminals::shell(params.value("profile", "default")), params.value("columns", 100), params.value("rows", 24),params.value("env",Json::object()));
    if (method == "terminal.list") return terminals_.list();
    if (method.starts_with("terminal.")) {
        const auto id = params.at("id").get<std::string>();
        if (method == "terminal.read") {
            const auto snap = terminals_.snapshot(id, params.value("firstRow", 0), params.value("limit", 200));
            Json lines = Json::array(); for (const auto &line : snap.lines) lines.push_back(Terminals::text(line));
            return {{"id", id}, {"firstRow", snap.first}, {"totalRows", snap.total}, {"columns", snap.columns}, {"rows", snap.rows},
                    {"cursor", {{"row", snap.cursorRow}, {"column", snap.cursorColumn}}}, {"running", snap.running},
                    {"alternate", snap.alternate}, {"generation", snap.generation}, {"lines", lines}};
        }
        if (method == "terminal.write") terminals_.write(id, params.at("data"));
        else if (method == "terminal.resize") terminals_.resize(id, params.at("columns"), params.at("rows"));
        else if (method == "terminal.key") terminals_.key(id, params.at("key"), params.value("modifiers", 0));
        else if (method == "terminal.close") terminals_.close(id);
        else if (method == "terminal.clear") terminals_.clear(id);
        else throw std::runtime_error("Unknown terminal operation");
        return {{"id", id}, {"ok", true}};
    }
    if (method == "language.highlight") return highlight(params);
    if (method == "language.symbols" || method == "language.documentSymbols") return symbolQuery(method, params);
    if (method == "language.rename" || method == "language.format") return prepareChange(method, params);
    if (method == "changes.preview" || method == "changes.apply" || method == "changes.undo") return changeOperation(method, params);
    if (method == "sources.read" || method == "sources.close") return sourceOperation(method, params);
    if (method == "unreal.prepare") return prepareUnrealAnalysis(params);
    if (method == "unreal.inspect")
        return unreal_.inspect(params);
    if (method == "unreal.generate" || method == "unreal.database" || method == "unreal.build") {
        auto result = unreal_.run(method, params);
        addSdkRoots(result.at("project"));
        if (result.at("exitCode") == 0 && method == "unreal.generate") {
            auto refreshed = inspectSolution(entry_);
            std::lock_guard lock(mutex_);
            solution_ = std::move(refreshed);
        }
        if (result.at("exitCode") == 0 && method == "unreal.database") {
            const auto &database = result.at("project").at("compilationDatabase");
            if (database.at("available").get<bool>())
                language_.setCompilationDatabase(resolve(database.at("path")).parent_path(), true);
        }
        return result;
    }
    if (method == "toolchains.detect")
        return build_.detect(params.value("refresh", false));
    if (method == "build.inspect")
        return build_.inspect(params);
    if (method == "build.configure" || method == "build.run") {
        auto result = build_.run(method, params);
        if (result.at("exitCode") == 0) {
            const auto &database = result.at("project").at("compilationDatabase");
            language_.setCompilationDatabase(database.at("available").get<bool>()
                                                 ? resolve(database.at("path")).parent_path()
                                                 : std::filesystem::path{},
                                             method == "build.configure");
        }
        return result;
    }
    if (method == "language.configure") {
        const auto backend=params.value("language","csharp");
        auto arguments=params.value("arguments",std::vector<std::string>{});
        if (params.contains("executable")) {
            if (!arguments.empty()) throw std::runtime_error("Use arguments or executable, not both");
            const auto executable=params.at("executable").get<std::string>();
            if (backend=="csharp") { language_.configureManaged(executable,params.value("enabled",true),true); return languageStatus(); }
            if (!executable.empty()) arguments.push_back(executable);
        }
        language_.configureBackend(backend,arguments,params.value("settings",Json::object()),params.value("enabled",true),true);
        return languageStatus();
    }
    if (method == "language.status")
        return languageStatus();
    if (method == "language.completionResolve" || method == "language.completionPrepare") return completionOperation(method,params);
    if (method == "language.codeActions" || method == "language.codeActionPrepare") return codeActionOperation(method,params);
    if (method == "language.definition" || method == "language.references" || method == "language.complete" || method == "language.hover" || method == "language.signature")
        return languageQuery(method, params);
    if (method == "workspace.describe") {
        std::lock_guard lock(mutex_);
        std::string project = "Folder";
        if (!solution_.is_null())
            project = "VisualStudio";
        else if (entry_.extension() == ".vcxproj" || entry_.extension() == ".csproj" || entry_.extension() == ".fsproj" || entry_.extension() == ".vbproj") project = "MSBuild";
        else if (std::filesystem::exists(root_ / "CMakeLists.txt"))
            project = "CMake";
        else if (std::filesystem::exists(root_ / "Cargo.toml"))
            project = "Cargo";
        else if (std::filesystem::exists(root_ / "package.json"))
            project = "Web";
        else if (ProjectTasks::detect(root_)=="java") project = "Java";
        return {{"root", pathUtf8(root_)},
                {"name", pathUtf8(root_.filename())},
                {"projectType", project},
                {"version", CYCODE_VERSION},
                {"solution", solution_}};
    }
    if (method == "workspace.files")
        return files();
    if (method == "search.text")
        return searchText(params);
    if (method == "plugins.list")
        return plugins_.describe();
    if (method == "plugins.invoke")
        return plugins_.invoke(params.at("command"), params.value("input", Json::object()));
    if (method.starts_with("vcs.")) {
        auto provider = params.value("provider", std::string("git"));
        if (provider != "git")
            return plugins_.vcs(provider, method.substr(4), params);
        return git(method, params);
    }
    std::lock_guard guard(mutex_);
    if (method == "documents.create") {
        auto full = resolve(params.at("path").get<std::string>());
        std::filesystem::create_directories(full.parent_path());
#ifdef _WIN32
        HANDLE created =
            CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (created == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create file: it may already exist");
        CloseHandle(created);
#else
        int created = ::open(full.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0666);
        if (created < 0)
            throw std::runtime_error("Cannot create file: it may already exist");
        close(created);
#endif
        auto key = pathUtf8(full.lexically_relative(root_));
        return documentJson(key, openLocked(key));
    }
    if (method == "documents.list") {
        Json docs = Json::array();
        for (const auto &[key, doc] : documents_)
            docs.push_back({{"path", key}, {"revision", doc.revision}, {"dirty", doc.dirty}});
        return {{"documents", docs}};
    }
    if (method == "documents.open" || method == "documents.read") {
        auto path = params.at("path").get<std::string>();
        auto &doc = openLocked(path);
        auto key = pathUtf8(resolve(path).lexically_relative(root_));
        return documentJson(key, doc);
    }
    if (method == "documents.reload") {
        const auto key = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
        auto &doc = openLocked(key);
        if (params.at("expectedRevision") != doc.revision) throw std::runtime_error("RevisionConflict: document changed before reload");
        if (doc.dirty && !params.value("discard",false)) throw std::runtime_error("Save or reconcile modified text before reloading from disk");
        auto previous = documents_.extract(key);
        try { return documentJson(key,openLocked(key)); }
        catch (...) { documents_.erase(key); documents_.insert(std::move(previous)); throw; }
    }
    if (method == "documents.replace" || method == "documents.save") {
        auto path = params.at("path").get<std::string>();
        auto &doc = openLocked(path);
        auto key = pathUtf8(resolve(path).lexically_relative(root_));
        if (!params.contains("expectedRevision") ||
            params.at("expectedRevision").get<std::uint64_t>() != doc.revision)
            throw std::runtime_error("RevisionConflict: document changed; read it again");
        if (method == "documents.replace") {
            auto text = params.at("text").get<std::string>();
            validateText(text);
            text = normalize(text);
            if (text != doc.text) {
                doc.text = std::move(text);
                doc.revision = nextRevision_++;
                doc.dirty = doc.text != doc.saved;
                language_.sync(key, doc.text, doc.revision);
            }
            return documentJson(key, doc);
        }
        auto full = resolve(path);
        if (readFile(full) != doc.disk)
            throw std::runtime_error("DiskConflict: file changed outside CyCode");
        if (!doc.dirty)
            return documentJson(key, doc);
        const auto bytes = encodeEditorText(doc.text, doc.bom, doc.crlf, doc.crOnly);
        writeTextFileAtomic(full, bytes, doc.disk);
        doc.saved = doc.text;
        doc.dirty = false;
        doc.disk = bytes;
        return documentJson(key, doc);
    }
    if (method == "documents.close") {
        auto key = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
        auto found = documents_.find(key);
        if (found != documents_.end() && found->second.dirty && !params.value("discard", false))
            throw std::runtime_error("Unsaved changes");
        documents_.erase(key);
        language_.close(key);
        return {{"closed", true}};
    }
    throw std::runtime_error("Unknown operation: " + method);
}
} // namespace cycode

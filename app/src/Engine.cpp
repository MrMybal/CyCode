#include "cycode/Engine.h"
#include "cycode/CompilerLocation.h"
#include "cycode/Solution.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include "cycode/LanguageInfo.h"
#include "cycode/NavigationLocations.h"
#include "cycode/OccurrenceCatalogue.h"
#include "cycode/SymbolCatalogue.h"
#include "cycode/PluginPackages.h"
#include "cycode/Tools.h"
#include "cycode/UnrealLaunch.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <optional>
#include <sstream>
#include <system_error>
#include <random>
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
Engine::Engine(std::filesystem::path root, std::filesystem::path languagePreferencesDirectory,std::filesystem::path userSnippetsDirectory)
    : monitor_(workspaceRoot(root)),root_(workspaceRoot(root)), entry_(std::filesystem::canonical(root)),recovery_(root_), solution_(inspectSolution(root)),
      build_(root_, [this](const std::string &path) { return resolve(path); }),
      msbuild_(root_,entry_,[this](const std::string &path) { return resolve(path); }),
      unreal_(root_, std::filesystem::canonical(root), solution_,
              [this](const std::string &path) { return resolve(path); }),
      language_(root_), languagePreferences_(root_, languagePreferencesDirectory), userSnippets_(std::move(userSnippetsDirectory)), projects_(root_,entry_,[this](const std::string &path) { return resolve(path); },terminals_),
      tests_(root_,[this](const std::string &path){return resolve(path);}),
      debugger_(root_, [this](const std::string &path) { return resolve(path); }, terminals_,
                [this] { return language_.request("cycode/javaDebugSession", {{"cycodeLanguage", "java"}}); },
                [this] { return language_.epoch("CyCode.java"); },
                [this](std::shared_ptr<std::atomic<bool>> cancelled) {
                    return language_.request("cycode/javaBuildWorkspace", {{"cycodeLanguage", "java"}}, std::move(cancelled));
                },
                [this] {
                    std::lock_guard lock(mutex_);
                    Json snapshot = Json::array();
                    for (const auto &[path, doc] : documents_) {
                        if (doc.dirty || !doc.diskConflict.empty()) throw std::runtime_error("UnsavedDocuments: save or resolve " + path + " before Java reload");
                        snapshot.push_back({path, doc.identity, doc.revision});
                    }
                    return snapshot;
                },
                [this](const Json &parameters) {
                    const auto full = resolve(parameters.at("path"));
                    const auto key = pathUtf8(full.lexically_relative(root_));
                    std::lock_guard lock(mutex_);
                    const auto found = documents_.find(key);
                    if (found == documents_.end() || found->second.identity != parameters.at("expectedDocumentId") ||
                        found->second.revision != parameters.at("expectedRevision"))
                        throw std::runtime_error("RevisionConflict: read the destination document again");
                    const auto &doc = found->second;
                    if (doc.dirty || !doc.diskConflict.empty() || readFile(full) != doc.disk)
                        throw std::runtime_error("UnsavedDocuments: save or resolve the destination source before running to cursor");
                    if (parameters.at("line").get<size_t>() > 1 + static_cast<size_t>(std::count(doc.text.begin(),doc.text.end(),'\n')))
                        throw std::runtime_error("Run-to-cursor line is outside the document");
                },
                [this](const Json &profile, bool launching) {
                    unreal_.validateLaunchProfile(profile);
                    if (launching) {
                        const auto artifacts=unreal_.artifacts(unrealBoundLaunchParameters(profile));
                        if (!artifacts.value("ready",false)||!artifacts.value("complete",false))
                            throw std::runtime_error("Unreal Editor outputs are missing or incompatible; inspect Unreal build outputs before launching");
                    }
                }) {
    if (!std::filesystem::is_directory(root_))
        throw std::runtime_error("Workspace must be a directory");
    addSdkRoots(Json::object());
    resources_=std::make_unique<Resources>(root_,entry_);
    templates_=std::make_unique<ProjectTemplates>(root_,[this]{return plugins_.templateSnapshot();},[this](const std::string &owner,uint64_t revision,const std::function<Json()> &commit){return plugins_.createFromTemplate(owner,revision,commit);});
    plugins_.workspace(root_,[this](const std::string &method,const Json &params) { return dispatch(method,params); });
    language_.setProject(entry_);
    events_.publish("workspace.opened",{{"root",pathUtf8(root_)},{"entry",pathUtf8(entry_)}});
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
    recoveryWorker_=std::jthread([this](std::stop_token stop) {
        while(!stop.stop_requested()) {
            checkpointRecovery();std::unique_lock lock(recoveryWaitMutex_);
            recoveryWake_.wait_for(lock,stop,std::chrono::seconds(1),[] { return false; });
        }
    });
    eventWorker_=std::jthread([this](std::stop_token stop) {
        while(!stop.stop_requested()) {
            try {plugins_.pumpEvents([this](const Json &params){return events_.read(params);},stop);}catch(...){}
            std::unique_lock lock(eventWaitMutex_);
            eventWake_.wait_for(lock,stop,std::chrono::milliseconds(100),[]{return false;});
        }
        eventWorkerFinished_=true;
    });
}
Engine::~Engine() {
    shuttingDown_=true;
    debugger_.requestShutdown();
    eventWorker_.request_stop();eventWake_.notify_all();
    while(!eventWorkerFinished_.load()) {
        // A callback can be entering a reservation while shutdown starts.
        // Keep signalling until the actual callback/owned work has returned.
        buildOperations_.cancel();tests_.cancel();projects_.cancel("");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(eventWorker_.joinable())eventWorker_.join();
    plugins_.shutdown();
    recoveryWorker_.request_stop();recoveryWake_.notify_all();if(recoveryWorker_.joinable())recoveryWorker_.join();checkpointRecovery();
}
std::filesystem::path Engine::resolve(const std::string &relative) const {
    if (relative.starts_with("untitled:"))
        throw std::runtime_error("Untitled documents have no filesystem path; use Save as first");
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
            {"untitled", isUntitledDocument(key)},
            {"displayPath", isUntitledDocument(key) ? untitledDocumentName(key) : key},
            {"text", doc.text},
            {"revision", doc.revision},
            {"id",doc.identity},
            {"dirty", doc.dirty},
            {"diskConflict",doc.diskConflict},
            {"lineEnding", doc.crlf     ? "CRLF"
                           : doc.crOnly ? "CR"
                                        : "LF"},
            {"encoding", doc.bom ? "UTF-8 BOM" : "UTF-8"}};
}
void Engine::documentEvent(const std::string &type,const std::string &path,const Document &doc,const std::string &transaction) {
    if(type!="document.opened")++hierarchyGeneration_;
    events_.publish(type,{{"path",path},{"id",doc.identity},{"revision",doc.revision},{"dirty",doc.dirty},{"diskConflict",doc.diskConflict},{"missing",!doc.diskExisted},{"transaction",transaction}});
}
Engine::Document &Engine::openLocked(const std::string &path, bool publish) {
    if (path.starts_with("untitled:")) {
        const auto key = documentKey(path);
        const auto found = untitled_.find(key);
        if (found == untitled_.end()) throw std::runtime_error("Unknown untitled document; create or recover it first");
        return found->second;
    }
    auto full = resolve(path);
    auto key = pathUtf8(full.lexically_relative(root_));
    auto found = documents_.find(key);
    if (found != documents_.end())
        return found->second;
    auto raw = readFile(full);
    Document doc;
    doc.identity=nextDocumentIdentity_++;
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
    language_.sync(key, opened.text, opened.revision, opened.identity);
    if(publish)documentEvent("document.opened",key,opened);
    return opened;
}
void Engine::refreshPluginServices() {
    if(plugins_.revision()<=languagePluginsRevision_.load())return;
    // Never acquire the plugin lock under Engine/registry locks: callbacks may
    // already own it. Snapshot first, then publish and resync live documents.
    const auto snapshot=plugins_.serviceSnapshot();
    std::lock_guard publication(languagePluginsMutex_);
    const auto revision=snapshot.at("revision").get<uint64_t>();
    if(revision<=languagePluginsRevision_)return;
    std::lock_guard documents(mutex_);
    language_.replaceExtensions(snapshot.at("servers"));
    debugger_.replaceExtensions(snapshot.at("debugAdapters"));
    for(const auto &[path,doc]:documents_)language_.sync(path,doc.text,doc.revision,doc.identity);
    languagePluginsRevision_=revision;
}
Json Engine::prepareUnrealAnalysis(const Json &params, const std::atomic<bool> *cancellation) {
    auto result = unreal_.prepare(params, cancellation);
    if (result.at("project").value("available",false)) addSdkRoots(result.at("project"));
    if (result.at("project").value("available",false) && !result.contains("inspectionError") && result.at("exitCode") == 0 && !result.value("cancelled", false) && !result.value("truncated", false) && !(cancellation && cancellation->load())) {
        const auto &db = result.at("project").at("compilationDatabase");
        if (db.at("available").get<bool>()) language_.setCompilationDatabase(resolve(db.at("path")).parent_path(), true);
    }
    return result;
}
Json Engine::languageQuery(const std::string &method, const Json &params) {
    const auto supplied = params.at("path").get<std::string>();
    const auto path = supplied.starts_with("sdk://") ? supplied : pathUtf8(resolve(supplied).lexically_relative(root_));
    if (!language_.accepts(path))
        throw std::runtime_error("No language backend for this file type");
    std::future<Json> reply;
    std::uint64_t revision, identity, generation, epoch;
    const auto navigation = locationRequest(method);
    const bool occurrences=method=="language.documentHighlights";
    const bool inspection=navigation || occurrences;
    std::string backend,sourceUri;
    const auto &position = params.at("position");
    {
        std::lock_guard lock(mutex_);
        auto [document, full] = semanticDocumentLocked(path);
        auto &doc = *document;
        revision = doc.revision; identity = doc.identity;
        generation = nextRevision_;
        backend = language_.backend(pathUtf8(full));
        epoch = inspection ? language_.backendEpoch(backend) : language_.epoch(path);
        if ((params.contains("expectedDocumentId") && params.at("expectedDocumentId") != identity) ||
            (inspection && params.contains("expectedEpoch") && params.at("expectedEpoch") != epoch))
            throw std::runtime_error("RevisionConflict: document identity or language configuration changed");
        if (!params.at("expectedRevision").is_number_integer() || params.at("expectedRevision") != revision)
            throw std::runtime_error("RevisionConflict: document changed; read it again");
        if (inspection && (!position.at("line").is_number_integer() || !position.at("character").is_number_integer()))
            throw std::runtime_error("Navigation positions must contain integer UTF-16 coordinates");
        (void)utf16Offset(doc.text,position);
        sourceUri=LanguageClient::uri(full);
        Json request = {{"textDocument", {{"uri", sourceUri}}},
                        {"position", position}};
        if (method == "language.references")
            request["context"] = {{"includeDeclaration", params.value("includeDeclaration", true)}};
        if (method == "language.signature") request["context"] = {{"triggerKind",1},{"isRetrigger",false}};
        // Engine's mutex orders document changes and requests; the service owns the transport.
        reply = language_.request(occurrences ? "textDocument/documentHighlight" : navigation ? navigation
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
    if (!current || current->revision != revision || current->identity != identity ||
        (!inspection && generation != nextRevision_) ||
        (inspection ? language_.backendEpoch(backend) : language_.epoch(path)) != epoch)
        throw std::runtime_error("RevisionConflict: document changed during language request");
    if (method == "language.complete")
        return issueCompletion(path,revision,generation,epoch,position,result);
    if (method=="language.hover" || method=="language.signature") {
        auto information=method=="language.hover" ? hoverInformation(result,current->text) : signatureInformation(result);
        information["path"]=path;information["revision"]=revision;information["positionEncoding"]="utf-16";return information;
    }
    if (occurrences) {
        auto report=collectOccurrences(result,static_cast<size_t>(std::clamp(params.value("limit",1000),1,4096)),current->text);
        report.update({{"path",path},{"revision",revision},{"documentId",identity},{"backend",backend},{"epoch",epoch},
            {"position",position},{"positionEncoding","utf-16"}});
        return report;
    }
    std::optional<SymbolTextBounds> targetBounds;
    auto report=collectLocations(result,static_cast<size_t>(std::clamp(params.value("limit",1000),1,5000)),current->text,
        [&](const std::string &uri,const Json &range,const Json &extent) {
            if (LanguageClient::uriKey(uri)==LanguageClient::uriKey(sourceUri)) {
                if (!targetBounds) targetBounds.emplace(current->text);
                if (!targetBounds->accepts(range) || !targetBounds->accepts(extent))
                    throw std::runtime_error("Navigation target is outside the live source buffer");
            }
            return sourceLocationLocked(uri,range,params.value("includeExternal",false));
        });
    report.update({{"path",path},{"revision",revision},{"documentId",identity},{"backend",backend},{"epoch",epoch},
        {"position",position},{"method",method},{"positionEncoding","utf-16"}});
    return report;
}
Json Engine::files(const Json &params) {
    std::lock_guard lock(filesMutex_);
    const auto generation=monitor_.snapshot().indexRevision;
    if(filesCache_.is_null()||generation!=filesIndexRevision_||params.value("refresh",false)) {
    auto result = runProcess({"rg", "--files", "--hidden", "--no-require-git", "-0", "-g", "!.git", "-g",
                              "!runtime", "-g", "!data", "-g", "!.cycode"},
                             root_,std::chrono::seconds(30),32*1024*1024);
    Json paths = Json::array();
    if (result.exitCode != 0 && result.exitCode != 1 && !result.truncated && !result.timedOut)
        throw std::runtime_error(result.output);
    std::size_t start = 0;
    while (start < result.output.size() && paths.size() < 200000) {
        auto end = result.output.find('\0', start);
        if (end == std::string::npos)
            break;
        auto path = result.output.substr(start, end - start);
        std::replace(path.begin(), path.end(), '\\', '/');
        paths.push_back(path);
        start = end + 1;
    }
    std::sort(paths.begin(), paths.end());
    RelatedFileIndex relatedIndex;relatedIndex.rebuild(paths);
    filesIndexRevision_=generation;
    filesCache_={{"files",std::move(paths)},{"truncated",result.truncated||result.timedOut||start<result.output.size()},{"indexRevision",generation}};
    relatedFileIndex_=std::move(relatedIndex);++filesCacheGeneration_;
    }
    const auto offset=params.value("offset",size_t(0)),limit=params.value("limit",size_t(20000));
    if(limit<1||limit>20000||offset>200000)throw std::runtime_error("Invalid file index page");
    const auto &all=filesCache_.at("files");Json page=Json::array();size_t bytes=0,end=std::min(offset,all.size());
    for(;end<all.size()&&page.size()<limit;++end) {
        const auto &path=all.at(end);bytes+=path.get_ref<const std::string &>().size()*6+4;
        if(bytes>8*1024*1024&&!page.empty())break;
        page.push_back(path);
    }
    return {{"files",page},{"truncated",filesCache_.at("truncated").get<bool>()||end<all.size()},
        {"nextOffset",end<all.size()?Json(end):Json(nullptr)},{"total",all.size()},{"indexRevision",filesCache_.at("indexRevision")},
        {"generation",filesCacheGeneration_}};
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
        for (const auto &[path, doc] : untitled_) dirty[path] = doc.text;
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
    // Unsaved buffers supersede disk results. Keep regex semantics/cancellation
    // in ripgrep by searching host-owned numeric snapshots, never project files.
    if(regex&&!dirty.empty()) {
        struct Snapshots {std::filesystem::path folder;~Snapshots(){if(!folder.empty()){std::error_code ec;std::filesystem::remove_all(folder,ec);}}} snapshots;
        std::random_device random;const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        for(int attempt=0;attempt<16;++attempt){const auto candidate=parent/("cycode-search-"+std::to_string(random())+"-"+std::to_string(random()));if(std::filesystem::create_directory(candidate)){snapshots.folder=candidate;break;}}
        if(snapshots.folder.empty())throw std::runtime_error("Cannot create private search snapshots");
#ifndef _WIN32
        std::filesystem::permissions(snapshots.folder,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
#endif
        std::map<std::string,std::string> paths;size_t bytes=0;
        for(const auto &[path,text]:dirty){if(cancellation&&cancellation->load())return {{"matches",Json::array()},{"truncated",false},{"cancelled",true}};
            bytes+=text.size();if(paths.size()>=128||bytes>64*1024*1024)throw std::runtime_error("Unsaved regex search exceeds 128 buffers / 64 MiB");
            const auto name=std::to_string(paths.size())+".txt";std::ofstream out(snapshots.folder/name,std::ios::binary);out.write(text.data(),std::streamsize(text.size()));if(!out)throw std::runtime_error("Cannot write search snapshot");paths[name]=path;
        }
        std::vector<std::string> arguments={"rg","--json","--no-ignore","--hidden","--max-count",std::to_string(limit)};if(!sensitive)arguments.push_back("--ignore-case");arguments.insert(arguments.end(),{"--",query,"."});
        const auto live=runProcess(arguments,snapshots.folder,std::chrono::seconds(15),8*1024*1024,cancellation);
        if(live.cancelled)return {{"matches",Json::array()},{"truncated",false},{"cancelled",true}};
        if(live.exitCode>1&&!live.truncated&&!live.timedOut)throw std::runtime_error(live.output);
        result.truncated=result.truncated||live.truncated;result.timedOut=result.timedOut||live.timedOut;
        std::istringstream rows(live.output);std::string row;
        while(std::getline(rows,row)){const auto item=Json::parse(row,nullptr,false);if(item.is_discarded()||item.value("type","")!="match")continue;
            const auto &data=item.at("data");if(!data.at("path").contains("text")||!data.at("lines").contains("text"))continue;
            const auto file=pathUtf8(utf8Path(data.at("path").at("text").get<std::string>()).filename());if(!paths.contains(file))continue;
            for(const auto &sub:data.at("submatches")){if(matches.size()>=size_t(limit)){over=true;break;}record(paths.at(file),data.at("line_number"),sub.at("start").get<size_t>()+1,data.at("lines").at("text"));}
        }
    }
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
            {"unsavedRegexExcluded", false}};
}
Json Engine::git(const std::string &method, const Json &params) {
    std::lock_guard vcs(vcsMutex_);
    if(method.starts_with("vcs.replay."))return gitReplay(method,params);
    if(method.starts_with("vcs.merge."))return gitMerge(method,params);
    if(method.starts_with("vcs.checkout."))return gitCheckout(method,params);
    if(method.starts_with("vcs.conflict."))return gitConflicts(method,params);
    if(method=="vcs.worktrees"||method.starts_with("vcs.worktree."))return gitWorktrees(method,params);
    if(method=="vcs.submodules"||method.starts_with("vcs.submodule."))return gitSubmodules(method,params);
    if(method=="vcs.hunks"||method=="vcs.applyHunks")return gitHunks(method,params);
    if (method != "vcs.status" && method != "vcs.diff" && method != "vcs.log" && method != "vcs.stage" && method != "vcs.unstage" && method != "vcs.commit") return gitAdvanced(method,params);
    const auto selected=gitSelection(params);
    const auto &gitRoot=selected.directory;
    std::vector<std::string> args{"git", "--literal-pathspecs", "-C", pathUtf8(gitRoot)};
    if (method == "vcs.status")
        args.insert(args.end(),
                    {"status", "--porcelain=v1", "-z", "--branch", "--untracked-files=all", "--", "."});
    else if (method == "vcs.diff") {
        args.insert(args.end(), {"diff", "--no-ext-diff", "--no-color"});
        if (params.value("staged", false))
            args.push_back("--cached");
        args.push_back("--");
        if (params.contains("path"))
            args.push_back(gitFileArgument(root_,selected,resolve(params.at("path")),true));
        else
            args.push_back(".");
    } else if (method == "vcs.log")
        args.insert(args.end(), {"log", "-30", "--date=short", "--format=%h %ad %s", "--", "."});
    else if (method == "vcs.stage") {
        args.push_back("add");
        args.push_back("--");
        args.push_back(gitFileArgument(root_,selected,resolve(params.at("path")),true));
    } else if (method == "vcs.unstage") {
        auto head = runProcess({"git", "-C", pathUtf8(gitRoot), "rev-parse", "--verify", "HEAD"}, gitRoot);
        if (head.exitCode == 0)
            args.insert(args.end(), {"restore", "--staged", "--"});
        else
            args.insert(args.end(), {"rm", "--cached", "--ignore-unmatch", "--"});
        args.push_back(gitFileArgument(root_,selected,resolve(params.at("path")),true));
    } else if (method == "vcs.commit") {
        const auto owner=runProcess({"git","-C",pathUtf8(gitRoot),"rev-parse","--show-toplevel"},gitRoot);
        auto top=owner.output;while(!top.empty()&&(top.back()=='\n'||top.back()=='\r'))top.pop_back();
        if(owner.exitCode||!std::filesystem::equivalent(utf8Path(top),gitRoot))throw std::runtime_error("Select the repository root before committing staged changes");
        auto message = params.at("message").get<std::string>();
        if (message.empty())
            throw std::runtime_error("Commit message is empty");
        args.insert(args.end(), {"commit", "-m", message});
    } else
        throw std::runtime_error("Unsupported Git operation");
    auto result = runProcess(args, gitRoot);
    if (method == "vcs.status") {
        auto location = runProcess({"git", "-C", pathUtf8(gitRoot), "rev-parse", "--show-toplevel"}, gitRoot);
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
    if(shuttingDown_ && method!="build.cancel" && method!="tests.cancel" && method!="project.cancel")throw std::runtime_error("Workspace is closing");
    if (method == "documents.createUntitled") return createUntitled(params);
    if (method == "documents.saveAs") return saveDocumentAs(params);
    // Cancellation must never wait for plugin/adapter/document locks: a plugin
    // callback can itself be waiting for the build process being cancelled.
    if (method == "build.cancel") return buildOperations_.cancel(params);
    if (method == "events.read") return events_.read(params);
    if (method == "build.status") return buildOperations_.status();
    if (method == "build.output") return buildOperations_.output(params);
    if (method == "build.location") return compilerLocation(params, root_, [this](const std::string &path) { return resolve(path); });
    if (method == "tests.cancel") return tests_.cancel(params);
    if (method == "project.cancel") return projects_.cancel(params.value("operationId", ""));
    if (method == "project.status") return projects_.status();
    if (method == "project.output") return projects_.output(params);
    refreshPluginServices();
    if (BuildOperations::supports(method)) {
        std::string operation;
        try {
            auto result=buildOperations_.execute(method, params, [&](const std::atomic<bool> *cancellation) {
                operation=buildOperations_.status().at("operationId");
                events_.publish("build.started",{{"operationId",operation},{"method",method}});
                return buildOperation(method, params, cancellation);
            });
            events_.publish("build.finished",{{"operationId",operation},{"method",method},{"exitCode",result.value("exitCode",-1)},{"cancelled",result.value("cancelled",false)},{"timedOut",result.value("timedOut",false)},{"truncated",result.value("truncated",false)}});
            return result;
        } catch(const std::exception &e) {
            if(!operation.empty())events_.publish("build.finished",{{"operationId",operation},{"method",method},{"error",e.what()}});
            throw;
        }
    }
    if(method=="tests.discover")return tests_.discover(params);
    if(method=="tests.run"){
        {std::lock_guard lock(mutex_);for(const auto &[path,doc]:documents_)if(doc.dirty)throw std::runtime_error("Save modified buffers before running project tests");}
        return tests_.run(params);
    }
    if(method=="templates.list")return templates_->list();
    if(method.starts_with("packages.")) {
        PluginPackages manager(installationRoot());
        if(method=="packages.list")return manager.catalog();
        if(method=="packages.inspect")return manager.inspect(resolve(params.at("source")));
        if(method=="packages.install")return manager.install(resolve(params.at("source")),params.at("expectedRevision"),params.at("expectedTrustRevision"),params.at("expectedPackage"));
        if(method=="packages.remove")return manager.remove(params.at("id"),params.at("expectedRevision"));
        if(method=="packages.rollback")return manager.rollback(params.at("id"),params.at("digest"),params.at("expectedRevision"),params.at("expectedTrustRevision"));
        if(method=="packages.clean")return manager.clean(params.at("id"),params.at("digest"),params.at("expectedRevision"));
        throw std::runtime_error("Package operation is not exported; publisher trust is managed explicitly by the desktop");
    }
    if(method=="templates.preview")return templates_->preview(params);
    if(method=="templates.create")return templates_->create(params.at("id"));
    if(method.starts_with("resources."))return resourceOperation(method,params);
    if(method.starts_with("recovery."))return recoveryOperation(method,params);
    if(method=="workspace.poll")return pollWorkspace();
    if (method=="project.inspect") return projects_.inspect(params);
    if (method.starts_with("project.")) {
        { std::lock_guard lock(mutex_);for (const auto &[path,document] : documents_) if (document.dirty) throw std::runtime_error("UnsavedDocuments: save "+path+" before executing project tasks"); }
        return projects_.execute(method,params);
    }
    if (method == "msbuild.inspect") return msbuild_.inspect(params);
    if (method == "msbuild.tools") return msbuild_.tools(params.value("refresh",false));
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
    if (method == "language.hierarchyPrepare" || method == "language.hierarchyExpand" || method == "language.hierarchyStatus") return hierarchyOperation(method,params);
    if (method == "language.symbols" || method == "language.documentSymbols") return symbolQuery(method, params);
    if (method == "language.symbolResolve") return resolveSymbol(params);
    if (method == "language.rename" || method == "language.format") return prepareChange(method, params);
    if (method == "changes.preview" || method == "changes.apply" || method == "changes.undo") return changeOperation(method, params);
    if (method == "sources.read" || method == "sources.close") return sourceOperation(method, params);
    if (method == "unreal.inspect")
        return unreal_.inspect(params);
    if (method == "unreal.artifacts")
        return unreal_.artifacts(params);
    if (method == "unreal.automationPlan")
        return unreal_.automationPlan(params);
    if (method == "unreal.launchProfile") {
        auto result = unreal_.launchProfile(params);
        result["expectedProfile"] = debugger_.profile();
        return result;
    }
    if (method == "toolchains.detect")
        return build_.detect(params.value("refresh", false));
    if (method == "build.inspect")
        return build_.inspect(params);
    if (method == "build.presets")
        return build_.presets(params);
    if (method == "build.presetPlan")
        return build_.presetPlan(params);
    if (method == "language.configure") {
        return configureLanguageBackend(params);
    }
    if (method == "language.preferences")
        return languagePreferences_.snapshot();
    if (method == "language.preferencesForget") {
        std::lock_guard lock(languageConfigurationMutex_);
        languagePreferences_.update(params.at("language"), std::nullopt, LanguagePreferences::revisionValue(params.at("expectedPreferencesRevision")));
        return languagePreferences_.snapshot(false);
    }
    if (method == "language.status")
        return languageStatus();
    if (method == "snippets.complete") return snippetCompletions(params);
    if (method == "snippets.userRead") return userSnippets_.snapshot();
    if (method == "workspace.counterparts") return relatedFiles(params);
    if (method == "workspace.include") return relatedFiles(params,true);
    if (method == "snippets.userDraft") return userSnippetDraft(params);
    if (method == "snippets.userSave") return userSnippets_.save(params.at("text"),params.at("expectedRevision"));
    if (method == "language.completionResolve" || method == "language.completionPrepare") return completionOperation(method,params);
    if (method == "language.codeActions" || method == "language.codeActionPrepare") return codeActionOperation(method,params);
    if (locationRequest(method) || method == "language.documentHighlights" || method == "language.complete" || method == "language.hover" || method == "language.signature")
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
        else if (ProjectTasks::detect(root_)=="maven") project = "Maven";
        else if (ProjectTasks::detect(root_)=="gradle") project = "Gradle";
        else if (ProjectTasks::detect(root_)=="shader") project = "Shader";
        return {{"root", pathUtf8(root_)},
                {"name", pathUtf8(root_.filename())},
                {"projectType", project},
                {"version", CYCODE_VERSION},
                {"solution", solution_}};
    }
    if (method == "workspace.files")
        return files(params);
    if (method == "search.text")
        return searchText(params);
    if (method == "plugins.list")
        return plugins_.describe();
    if (method == "plugins.configure") { auto result=plugins_.configure(params);refreshPluginServices();return result; }
    if (method == "plugins.project") return plugins_.project(params.at("provider"),params.at("action"),params.value("input",Json::object()));
    if (method == "plugins.invoke")
        return plugins_.invoke(params.at("command"), params.value("input", Json::object()));
    if (method.starts_with("vcs.")) {
        return versionControl(method,params);
    }
    std::unique_lock guard(mutex_);
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
            docs.push_back({{"path", key}, {"id",doc.identity},{"revision", doc.revision}, {"dirty", doc.dirty},{"diskConflict",doc.diskConflict}});
        for (const auto &[key, doc] : untitled_)
            docs.push_back({{"path",key},{"id",doc.identity},{"revision",doc.revision},{"dirty",true},{"untitled",true},{"displayPath",untitledDocumentName(key)},{"diskConflict",""}});
        return {{"documents", docs}};
    }
    if (method == "documents.open" || method == "documents.read") {
        auto path = params.at("path").get<std::string>();
        auto &doc = openLocked(path);
        auto key = documentKey(path);
        return documentJson(key, doc);
    }
    if (method == "documents.reload") {
        const auto key = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
        auto &doc = openLocked(key);
        if (params.at("expectedRevision") != doc.revision) throw std::runtime_error("RevisionConflict: document changed before reload");
        if (doc.dirty && !params.value("discard",false)) throw std::runtime_error("Save or reconcile modified text before reloading from disk");
        auto previous = documents_.extract(key);
        Json result;
        try { auto &reloaded=openLocked(key,false);reloaded.identity=previous.mapped().identity;language_.sync(key,reloaded.text,reloaded.revision,reloaded.identity);result=documentJson(key,reloaded);documentEvent("document.reloaded",key,reloaded); }
        catch (...) { documents_.erase(key); documents_.insert(std::move(previous)); throw; }
        guard.unlock();retireRecovery(key);return result;
    }
    if (method == "documents.replace" || method == "documents.save") {
        auto path = params.at("path").get<std::string>();
        auto &doc = openLocked(path);
        auto key = documentKey(path);
        if (!params.contains("expectedRevision") ||
            params.at("expectedRevision").get<std::uint64_t>() != doc.revision)
            throw std::runtime_error("RevisionConflict: document changed; read it again");
        if (method == "documents.replace") {
            auto text = params.at("text").get<std::string>();
            validateText(text);
            text = normalize(text);
            if (isUntitledDocument(key)) checkUntitledQuota(key, text.size());
            if (text != doc.text) {
                doc.text = std::move(text);
                doc.revision = nextRevision_++;
                doc.dirty = isUntitledDocument(key) || doc.text != doc.saved;
                if (!isUntitledDocument(key)) language_.sync(key, doc.text, doc.revision, doc.identity);
                documentEvent("document.changed",key,doc);
            }
            return documentJson(key, doc);
        }
        if (isUntitledDocument(key)) throw std::runtime_error("Use Save as to give this document a workspace file");
        auto full = resolve(path);
        if ((doc.diskExisted&&readFile(full)!=doc.disk)||(!doc.diskExisted&&std::filesystem::exists(full)))
            throw std::runtime_error("DiskConflict: file changed outside CyCode");
        if (!doc.dirty&&doc.diskExisted) { auto result=documentJson(key,doc);guard.unlock();retireRecovery(key);return result; }
        const auto bytes = encodeEditorText(doc.text, doc.bom, doc.crlf, doc.crOnly);
        if(!doc.diskExisted)std::filesystem::create_directories(full.parent_path());
        writeTextFileAtomic(full, bytes, doc.diskExisted?std::optional<std::string>(doc.disk):std::nullopt);
        doc.saved = doc.text;
        doc.dirty = false;
        doc.disk = bytes;
        doc.diskExisted=true;doc.diskConflict.clear();documentEvent("document.saved",key,doc);auto result=documentJson(key, doc);guard.unlock();retireRecovery(key);return result;
    }
    if (method == "documents.close") {
        auto key = documentKey(params.at("path"));
        if (isUntitledDocument(key) && !params.contains("expectedRevision"))
            throw std::runtime_error("Closing an untitled document requires its expected revision");
        auto &owner = isUntitledDocument(key) ? untitled_ : documents_;
        auto found = owner.find(key);
        if (params.contains("expectedRevision") && (found == owner.end() || params.at("expectedRevision") != found->second.revision))
            throw std::runtime_error("RevisionConflict: document changed before closing");
        if (found != owner.end() && found->second.dirty && !params.value("discard", false))
            throw std::runtime_error("Unsaved changes");
        if(found!=owner.end())documentEvent("document.closed",key,found->second);
        owner.erase(key);
        if (!isUntitledDocument(key)) language_.close(key);
        guard.unlock();retireRecovery(key);
        return {{"closed", true}};
    }
    throw std::runtime_error("Unknown operation: " + method);
}
} // namespace cycode

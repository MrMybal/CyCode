#include "cycode/Engine.h"
#include <cstring>
#include "cycode/AtomicFile.h"
#include "cycode/CompilerLocation.h"
#include "cycode/Error.h"
#include "cycode/GitSafety.h"
#include "cycode/LanguageInfo.h"
#include "cycode/NativeTrace.h"
#include "cycode/NavigationLocations.h"
#include "cycode/OccurrenceCatalogue.h"
#include "cycode/PluginPackages.h"
#include "cycode/ProcessText.h"
#include "cycode/Solution.h"
#include "cycode/SymbolCatalogue.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include "cycode/UnrealLaunch.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace cycode {
static constexpr std::size_t maxFile = maxTextFileBytes;
static std::string readFile(const std::filesystem::path &path) { return readTextFileBytes(path); }
static void validateText(const std::string &text) { validateEditorText(text); }
static std::string normalize(std::string text) { return normalizeEditorText(text); }
Engine::Engine(std::filesystem::path root, std::filesystem::path languagePreferencesDirectory,
               std::filesystem::path userSnippetsDirectory)
    : monitor_(workspaceRoot(root)), fileIndex_(workspaceRoot(root)), codeIndex_(workspaceRoot(root), monitor_),
      root_(workspaceRoot(root)),
      entry_(std::filesystem::canonical(root)), recovery_(root_), solution_(inspectSolution(root)),
      build_(root_, [this](const std::string &path) { return resolve(path); }),
      msbuild_(root_, entry_, [this](const std::string &path) { return resolve(path); }),
      unreal_(root_, std::filesystem::canonical(root), solution_,
              [this](const std::string &path) { return resolve(path); }),
      language_(root_), languagePreferences_(root_, languagePreferencesDirectory),
      userSnippets_(std::move(userSnippetsDirectory)),
      projects_(
          root_, entry_, [this](const std::string &path) { return resolve(path); }, terminals_),
      tests_(root_, [this](const std::string &path) { return resolve(path); }),
      debugger_(
          root_, [this](const std::string &path) { return resolve(path); }, terminals_,
          [this] { return language_.request("cycode/javaDebugSession", {{"cycodeLanguage", "java"}}); },
          [this] { return language_.epoch("CyCode.java"); },
          [this](std::shared_ptr<std::atomic<bool>> cancelled) {
              return language_.request("cycode/javaBuildWorkspace", {{"cycodeLanguage", "java"}},
                                       std::move(cancelled));
          },
          [this] {
              std::lock_guard lock(mutex_);
              Json snapshot = Json::array();
              for (const auto &[path, doc] : documents_) {
                  if (doc.dirty || !doc.diskConflict.empty())
                      throw std::runtime_error("UnsavedDocuments: save or resolve " + path +
                                               " before Java reload");
                  snapshot.push_back({path, doc.identity, doc.revision});
              }
              return snapshot;
          },
          [this](const Json &parameters) {
              const auto full = resolve(parameters.at("path"));
              const auto key = pathUtf8(full.lexically_relative(root_));
              std::lock_guard lock(mutex_);
              const auto found = documents_.find(key);
              if (found == documents_.end() ||
                  found->second.identity != parameters.at("expectedDocumentId") ||
                  found->second.revision != parameters.at("expectedRevision"))
                  throw NativeError(ErrorCode::RevisionConflict,
                                    "RevisionConflict: read the destination document again");
              const auto &doc = found->second;
              if (doc.dirty || !doc.diskConflict.empty() || readFile(full) != doc.disk)
                  throw std::runtime_error(
                      "UnsavedDocuments: save or resolve the destination source before running to cursor");
              if (parameters.at("line").get<size_t>() >
                  1 + static_cast<size_t>(std::count(doc.text.begin(), doc.text.end(), '\n')))
                  throw std::runtime_error("Run-to-cursor line is outside the document");
          },
          [this](const Json &profile, bool launching) {
              unreal_.validateLaunchProfile(profile);
              if (launching) {
                  const auto artifacts = unreal_.artifacts(unrealBoundLaunchParameters(profile));
                  if (!artifacts.value("ready", false) || !artifacts.value("complete", false))
                      throw std::runtime_error("Unreal Editor outputs are missing or incompatible; inspect "
                                               "Unreal build outputs before launching");
              }
          }) {
    if (!std::filesystem::is_directory(root_))
        throw std::runtime_error("Workspace must be a directory");
    addSdkRoots(Json::object());
    resources_ = std::make_unique<Resources>(root_, entry_);
    templates_ = std::make_unique<ProjectTemplates>(
        root_, [this] { return plugins_.templateSnapshot(); },
        [this](const std::string &owner, uint64_t revision, const std::function<Json()> &commit) {
            return plugins_.createFromTemplate(owner, revision, commit);
        });
    plugins_.workspace(
        root_, [this](const std::string &method, const Json &params) { return dispatch(method, params); });
    language_.setProject(entry_);
    events_.publish("workspace.opened", {{"root", pathUtf8(root_)}, {"entry", pathUtf8(entry_)}});
    try {
        language_.setCompilationDatabase(build_.activeDatabaseDirectory());
        const auto msbuild = msbuild_.inspect();
        if (msbuild.at("compilationDatabase").at("available").get<bool>())
            language_.setCompilationDatabase(
                resolve(msbuild.at("compilationDatabase").at("path")).parent_path());
        auto unreal = unreal_.inspect();
        addSdkRoots(unreal);
        if (unreal.value("available", false) && unreal.at("compilationDatabase").value("available", false))
            language_.setCompilationDatabase(
                resolve(unreal.at("compilationDatabase").at("path")).parent_path());
    } catch (
        const std::exception &) { /* Build inspection reports invalid profiles without preventing editing. */
    }
    recoveryWorker_ = std::jthread([this](std::stop_token stop) {
        while (!stop.stop_requested()) {
            checkpointRecovery();
            std::unique_lock lock(recoveryWaitMutex_);
            recoveryWake_.wait_for(lock, stop, std::chrono::seconds(1), [] { return false; });
        }
    });
    eventWorker_ = std::jthread([this](std::stop_token stop) {
        while (!stop.stop_requested()) {
            try {
                plugins_.pumpEvents([this](const Json &params) { return events_.read(params); }, stop);
            } catch (...) {
            }
            std::unique_lock lock(eventWaitMutex_);
            eventWake_.wait_for(lock, stop, std::chrono::milliseconds(100), [] { return false; });
        }
        eventWorkerFinished_ = true;
    });
    // Reload a persisted project index now (load + stat reconciliation, seconds), so the
    // first search of a new session does not fall back to a scan. A first build stays lazy.
    codeIndex_.warmStart();
}
Engine::~Engine() {
    NativeTraceScope trace(NativeTraceEvent::EngineShutdown);
    shuttingDown_ = true;
    fileIndex_.stop();
    codeIndex_.stop();
    debugger_.requestShutdown();
    eventWorker_.request_stop();
    eventWake_.notify_all();
    while (!eventWorkerFinished_.load()) {
        // A callback can be entering a reservation while shutdown starts.
        // Keep signalling until the actual callback/owned work has returned.
        buildOperations_.cancel();
        tests_.cancel();
        projects_.cancel("");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (eventWorker_.joinable())
        eventWorker_.join();
    plugins_.shutdown();
    recoveryWorker_.request_stop();
    recoveryWake_.notify_all();
    if (recoveryWorker_.joinable())
        recoveryWorker_.join();
    checkpointRecovery();
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
            {"id", doc.identity},
            {"dirty", doc.dirty},
            {"diskConflict", doc.diskConflict},
            {"lineEnding", doc.crlf     ? "CRLF"
                           : doc.crOnly ? "CR"
                                        : "LF"},
            {"encoding", doc.bom ? "UTF-8 BOM" : "UTF-8"}};
}
void Engine::documentEvent(const std::string &type, const std::string &path, const Document &doc,
                           const std::string &transaction) {
    if (type != "document.opened")
        ++hierarchyGeneration_;
    events_.publish(type, {{"path", path},
                           {"id", doc.identity},
                           {"revision", doc.revision},
                           {"dirty", doc.dirty},
                           {"diskConflict", doc.diskConflict},
                           {"missing", !doc.diskExisted},
                           {"transaction", transaction}});
}
Engine::Document &Engine::openLocked(const std::string &path, bool publish) {
    if (path.starts_with("untitled:")) {
        const auto key = documentKey(path);
        const auto found = untitled_.find(key);
        if (found == untitled_.end())
            throw std::runtime_error("Unknown untitled document; create or recover it first");
        return found->second;
    }
    auto full = resolve(path);
    auto key = pathUtf8(full.lexically_relative(root_));
    if (pagedDocuments_.contains(key) || pendingPagedDocuments_.contains(key))
        throw NativeError(ErrorCode::UnsupportedOperation,
                          "This document uses paged storage; use documents.readPage and prepareByteEdit");
    auto found = documents_.find(key);
    if (found != documents_.end()) {
        if (publish)
            analysisTemporaryDocuments_.erase(key);
        return found->second;
    }
    const auto raw = readFile(full);
    // Preserve the project's UTF-8-only policy. Standalone text sessions may
    // explicitly select other encodings through the same native buffer service.
    Document doc = text::decodeBuffer(raw, raw.starts_with("\xef\xbb\xbf") ? "utf-8-bom" : "utf-8");
    doc.identity = nextDocumentIdentity_++;
    doc.revision = nextRevision_++;
    auto &opened = documents_.emplace(key, std::move(doc)).first->second;
    language_.sync(key, opened.text, opened.revision, opened.identity);
    if (publish)
        documentEvent("document.opened", key, opened);
    return opened;
}
void Engine::refreshPluginServices() {
    if (plugins_.revision() <= languagePluginsRevision_.load())
        return;
    // Never acquire the plugin lock under Engine/registry locks: callbacks may
    // already own it. Snapshot first, then publish and resync live documents.
    const auto snapshot = plugins_.serviceSnapshot();
    std::lock_guard publication(languagePluginsMutex_);
    const auto revision = snapshot.at("revision").get<uint64_t>();
    if (revision <= languagePluginsRevision_)
        return;
    std::lock_guard documents(mutex_);
    language_.replaceExtensions(snapshot.at("servers"));
    debugger_.replaceExtensions(snapshot.at("debugAdapters"));
    for (const auto &[path, doc] : documents_)
        language_.sync(path, doc.text, doc.revision, doc.identity);
    languagePluginsRevision_ = revision;
}
Json Engine::prepareUnrealAnalysis(const Json &params, const std::atomic<bool> *cancellation) {
    auto result = unreal_.prepare(params, cancellation);
    if (result.at("project").value("available", false))
        addSdkRoots(result.at("project"));
    if (result.at("project").value("available", false) && !result.contains("inspectionError") &&
        result.at("exitCode") == 0 && !result.value("cancelled", false) &&
        !result.value("truncated", false) && !(cancellation && cancellation->load())) {
        const auto &db = result.at("project").at("compilationDatabase");
        if (db.at("available").get<bool>())
            language_.setCompilationDatabase(resolve(db.at("path")).parent_path(), true);
    }
    return result;
}
namespace {
std::string navigationCompilerContext(LanguageService &language, const std::string &backend) {
    const auto configuration = language.configuration(backend);
    auto result = configuration.dump();
    auto directory = language.compilationDatabase(backend);
    if (configuration.contains("arguments")) {
        const auto &args = configuration.at("arguments");
        for (size_t i = 0; i < args.size(); ++i) {
            const auto arg = args[i].get<std::string>();
            const auto prefix = arg.starts_with("--compile-commands-dir=")  ? 23
                                : arg.starts_with("-compile-commands-dir=") ? 22
                                                                            : 0;
            if (prefix)
                directory = utf8Path(arg.substr(prefix));
            else if ((arg == "--compile-commands-dir" || arg == "-compile-commands-dir") &&
                     i + 1 < args.size())
                directory = utf8Path(args[++i].get<std::string>());
        }
    }
    if (!directory.empty()) {
        std::error_code error;
        const auto file = std::filesystem::weakly_canonical(directory / "compile_commands.json", error);
        result += pathUtf8(file);
        const auto stamp = std::filesystem::last_write_time(file, error);
        if (error)
            result += "missing";
        else {
            result += std::to_string(stamp.time_since_epoch().count());
            result += ':';
            result += std::to_string(std::filesystem::file_size(file, error));
        }
    }
    return result;
}
} // namespace
Json Engine::languageQuery(const std::string &method, const Json &params) {
    const auto supplied = params.at("path").get<std::string>();
    const auto path =
        supplied.starts_with("sdk://") ? supplied : pathUtf8(resolve(supplied).lexically_relative(root_));
    if (!language_.accepts(path))
        throw std::runtime_error("No language backend for this file type");
    std::future<Json> reply;
    bool refreshContext = false;
    Json request;
    std::uint64_t revision, identity, generation, epoch;
    const auto navigation = locationRequest(method);
    const bool occurrences = method == "language.documentHighlights";
    const bool inspection = navigation || occurrences;
    std::string backend, sourceUri, cacheKey, compilerContext;
    uint64_t cacheWorkspaceRevision = 0;
    const bool cacheable = (method == "language.definition" || method == "language.declaration" ||
                            method == "language.typeDefinition") &&
                           !params.value("includeExternal", false) && !path.starts_with("sdk://");
    const auto &position = params.at("position");
    {
        std::lock_guard lock(mutex_);
        auto [document, full] = semanticDocumentLocked(path);
        auto &doc = *document;
        revision = doc.revision;
        identity = doc.identity;
        generation = documentGenerationLocked();
        backend = language_.backend(pathUtf8(full));
        epoch = inspection ? language_.backendEpoch(backend) : language_.epoch(path);
        if ((params.contains("expectedDocumentId") && params.at("expectedDocumentId") != identity) ||
            (inspection && params.contains("expectedEpoch") && params.at("expectedEpoch") != epoch))
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: document identity or language configuration changed");
        if (!params.at("expectedRevision").is_number_integer() || params.at("expectedRevision") != revision)
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: document changed; read it again");
        if (inspection &&
            (!position.at("line").is_number_integer() || !position.at("character").is_number_integer()))
            throw std::runtime_error("Navigation positions must contain integer UTF-16 coordinates");
        (void)utf16Offset(doc.text, position);
        sourceUri = LanguageClient::uri(full);
        if (cacheable) {
            cacheWorkspaceRevision = monitor_.snapshot().revision;
            compilerContext = navigationCompilerContext(language_, backend);
            auto identityParams = params;
            identityParams.erase("refresh");
            cacheKey = method + ":" + path + ":" + std::to_string(identity) + ":" + identityParams.dump();
            const auto found = navigationCache_.find(cacheKey);
            if (found != navigationCache_.end() && !params.value("refresh", false)) {
                const auto &entry = found->second;
                bool valid = entry.generation == generation &&
                             entry.workspaceRevision == cacheWorkspaceRevision && entry.epoch == epoch &&
                             entry.compilerContext == compilerContext &&
                             std::chrono::steady_clock::now() - entry.created < std::chrono::minutes(5);
                for (const auto &[file, stamp] : entry.targets) {
                    std::error_code error;
                    if (std::filesystem::last_write_time(file, error) != stamp.first || error ||
                        std::filesystem::file_size(file, error) != stamp.second || error)
                        valid = false;
                }
                if (valid) {
                    auto hit = entry.result;
                    hit["cacheHit"] = true;
                    return hit;
                }
            }
        }
        if (backend == "cpp") {
            const auto previous = navigationContexts_.find(path);
            refreshContext = previous != navigationContexts_.end() && previous->second.revision == revision &&
                             previous->second.generation != generation;
        }
        request = {{"textDocument", {{"uri", sourceUri}}}, {"position", position}};
        if (method == "language.references")
            request["context"] = {{"includeDeclaration", params.value("includeDeclaration", true)}};
        if (method == "language.signature")
            request["context"] = {{"triggerKind", 1}, {"isRetrigger", false}};
    }
    {
        std::lock_guard lock(mutex_);
        auto [document, full] = semanticDocumentLocked(path);
        if (document->revision != revision || document->identity != identity ||
            generation != documentGenerationLocked() || language_.backendEpoch(backend) != epoch)
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: navigation dependency context changed");
        if (backend == "cpp") {
            if (refreshContext)
                language_.sync(path, document->text, revision, identity, true, true);
            if (navigationContexts_.size() >= 128)
                navigationContexts_.clear();
            navigationContexts_[path] = {revision, generation, epoch};
        }
        // Engine's mutex orders document changes and requests; the service owns the transport.
        reply = language_.request(occurrences                      ? "textDocument/documentHighlight"
                                  : navigation                     ? navigation
                                  : method == "language.hover"     ? "textDocument/hover"
                                  : method == "language.signature" ? "textDocument/signatureHelp"
                                                                   : "textDocument/completion",
                                  std::move(request));
    }
    auto result = reply.get(); // Backend deadline includes initialization (15s C++, 60s C#).
    Json astSymbols;
    bool astReconciled = false;
    if (backend == "cpp" && (method == "language.definition" || method == "language.declaration")) {
        std::future<Json> ast;
        {
            std::lock_guard lock(mutex_);
            bool liveTarget = false;
            const auto rows = result.is_array() ? result : Json::array({result});
            if (rows.size() <= 32)
                for (const auto &row : rows) {
                    try {
                        const auto uri =
                            row.at(row.contains("targetUri") ? "targetUri" : "uri").get<std::string>();
                        const auto name = pathUtf8(
                            resolve(pathUtf8(LanguageClient::fromUri(uri))).lexically_relative(root_));
                        if (name != path && documents_.contains(name) && LanguageService::supportsCpp(name)) {
                            liveTarget = true;
                            break;
                        }
                    } catch (const std::exception &) {
                    }
                }
            if (liveTarget) {
                if (generation != documentGenerationLocked())
                    throw NativeError(ErrorCode::RevisionConflict, "Navigation dependency changed");
                ast = language_.request("textDocument/symbolInfo",
                                        {{"textDocument", {{"uri", sourceUri}}}, {"position", position}});
            }
        }
        if (ast.valid())
            astSymbols = ast.get();
    }
    if (!astSymbols.is_null()) {
        auto reconciled = reconcileNavigationTargets(result, astSymbols, generation, epoch);
        astReconciled = reconciled != result;
        result = std::move(reconciled);
    }
    std::lock_guard lock(mutex_);
    const auto current = path.starts_with("sdk://")
                             ? (externalSources_.contains(path) && externalSources_.at(path).loaded
                                    ? &externalSources_.at(path).document
                                    : nullptr)
                             : (documents_.contains(path) ? &documents_.at(path) : nullptr);
    if (!current || current->revision != revision || current->identity != identity ||
        (!inspection && generation != documentGenerationLocked()) ||
        (inspection ? language_.backendEpoch(backend) : language_.epoch(path)) != epoch)
        throw NativeError(ErrorCode::RevisionConflict,
                          "RevisionConflict: document changed during language request");
    if (method == "language.complete")
        return issueCompletion(path, revision, generation, epoch, position, result);
    if (method == "language.hover" || method == "language.signature") {
        auto information = method == "language.hover" ? hoverInformation(result, current->text)
                                                      : signatureInformation(result);
        information["path"] = path;
        information["revision"] = revision;
        information["positionEncoding"] = "utf-16";
        return information;
    }
    if (occurrences) {
        auto report = collectOccurrences(
            result, static_cast<size_t>(std::clamp(params.value("limit", 1000), 1, 4096)), current->text);
        report.update({{"path", path},
                       {"revision", revision},
                       {"documentId", identity},
                       {"backend", backend},
                       {"epoch", epoch},
                       {"position", position},
                       {"positionEncoding", "utf-16"}});
        return report;
    }
    std::optional<SymbolTextBounds> targetBounds;
    auto report = collectLocations(
        result, static_cast<size_t>(std::clamp(params.value("limit", 1000), 1, 5000)), current->text,
        [&](const std::string &uri, const Json &range, const Json &extent) {
            if (LanguageClient::uriKey(uri) == LanguageClient::uriKey(sourceUri)) {
                if (!targetBounds)
                    targetBounds.emplace(current->text);
                if (!targetBounds->accepts(range) || !targetBounds->accepts(extent))
                    throw std::runtime_error("Navigation target is outside the live source buffer");
            }
            return sourceLocationLocked(uri, range, params.value("includeExternal", false));
        });
    report.update({{"path", path},
                   {"revision", revision},
                   {"documentId", identity},
                   {"backend", backend},
                   {"epoch", epoch},
                   {"position", position},
                   {"method", method},
                   {"positionEncoding", "utf-16"}});
    if (astReconciled)
        report["astReconciled"] = true;
    if (cacheable) {
        report["cacheHit"] = false;
        if (generation == documentGenerationLocked() &&
            cacheWorkspaceRevision == monitor_.snapshot().revision && !report.at("truncated").get<bool>() &&
            report.at("invalid") == 0 && report.at("outsideWorkspace") == 0 &&
            !report.at("locations").empty() && report.at("locations").size() <= 32) {
            NavigationCacheEntry entry;
            entry.result = report;
            entry.generation = generation;
            entry.workspaceRevision = cacheWorkspaceRevision;
            entry.epoch = epoch;
            entry.compilerContext = compilerContext;
            bool valid = true;
            for (const auto &location : report.at("locations")) {
                const auto name = location.at("path").get<std::string>();
                if (name.starts_with("sdk://")) {
                    valid = false;
                    break;
                }
                const auto file = resolve(name);
                std::error_code error;
                const auto stamp = std::filesystem::last_write_time(file, error);
                if (error) {
                    valid = false;
                    break;
                }
                const auto bytes = std::filesystem::file_size(file, error);
                if (error) {
                    valid = false;
                    break;
                }
                entry.targets.push_back({file, {stamp, bytes}});
            }
            const auto bytes = report.dump().size();
            if (valid && bytes <= 1024 * 1024) {
                if (navigationCache_.size() >= 64 || navigationCacheBytes_ + bytes > 4 * 1024 * 1024) {
                    navigationCache_.clear();
                    navigationCacheBytes_ = 0;
                }
                if (navigationCache_.contains(cacheKey))
                    navigationCacheBytes_ -= navigationCache_.at(cacheKey).result.dump().size();
                navigationCacheBytes_ += bytes;
                navigationCache_.insert_or_assign(cacheKey, std::move(entry));
            }
        }
    }
    return report;
}
Json Engine::files(const Json &params) {
    const auto offsetValue = params.value("offset", Json(0)), limitValue = params.value("limit", Json(20000));
    if (!offsetValue.is_number_integer() || offsetValue.get<int64_t>() < 0 ||
        !limitValue.is_number_integer() || limitValue.get<int64_t>() < 1 || limitValue.get<int64_t>() > 20000)
        throw std::runtime_error("Invalid file index page");
    const auto offset = offsetValue.get<size_t>(), limit = limitValue.get<size_t>();
    std::lock_guard lock(filesMutex_);
    const auto revision = monitor_.snapshot().indexRevision;
    filesSnapshot_ = fileIndex_.snapshot(revision, params.value("refresh", false));
    const auto &snapshot = *filesSnapshot_;
    filesIndexRevision_ = snapshot.revision;
    filesCacheGeneration_ = snapshot.generation;
    const auto &all = snapshot.files;
    Json page = Json::array();
    size_t bytes = 0, end = std::min(offset, all.size());
    for (; end < all.size() && page.size() < limit; ++end) {
        const auto &path = all.at(end);
        bytes += path.get_ref<const std::string &>().size() * 6 + 4;
        if (bytes > 8 * 1024 * 1024 && !page.empty())
            break;
        page.push_back(path);
    }
    return {{"files", page},
            {"truncated", !snapshot.complete || end < all.size()},
            {"nextOffset", end < all.size() ? Json(end) : Json(nullptr)},
            {"total", all.size()},
            {"indexRevision", snapshot.revision},
            {"generation", snapshot.generation},
            {"indexing", snapshot.indexing},
            {"complete", snapshot.complete},
            {"indexError", snapshot.error}};
}
namespace {
// While an interactive ripgrep scan runs, a background index build pauses between files.
struct ForegroundScan {
    explicit ForegroundScan(CodeIndex &index) : index(index) { index.foregroundBegin(); }
    ~ForegroundScan() { index.foregroundEnd(); }
    ForegroundScan(const ForegroundScan &) = delete;
    ForegroundScan &operator=(const ForegroundScan &) = delete;
    CodeIndex &index;
};
size_t searchByteBudget(const Json &params) {
    return size_t(std::clamp<int64_t>(params.value("maxBytes", int64_t(0)), 0, int64_t(64) << 20));
}
// Scan results have no cursor: keep the leading rows within the byte budget (at least one) and
// report the cut, so a client never receives a response silently chopped in the middle.
bool keepWithinBudget(Json &rows, size_t maxBytes, size_t &used) {
    if (!maxBytes || !rows.is_array()) return false;
    Json kept = Json::array();
    bool cut = false;
    for (auto &row : rows) {
        const auto size = row.dump().size() + 1;
        if (!kept.empty() && used + size > maxBytes) {
            cut = true;
            break;
        }
        used += size;
        kept.push_back(std::move(row));
    }
    rows = std::move(kept);
    return cut;
}
} // namespace

Json Engine::searchText(const Json &params, const std::atomic<bool> *cancellation) {
    if (params.contains("queries")) {
        // Several searches in one call (same filters/limits), one result block per query.
        const auto &queries = params.at("queries");
        if (!queries.is_array() || queries.empty() || queries.size() > 10 || params.contains("query"))
            throw NativeError(ErrorCode::InvalidParams, "queries must list 1 to 10 strings (instead of query)");
        Json results = Json::array();
        bool truncated = false, allIndexed = true;
        // While the index is not usable, run ONE multi-pattern ripgrep for every query instead of
        // one full scan per query; lines are attributed per query with the index matcher.
        const auto engine = params.value("engine", std::string("auto"));
        std::vector<std::string> list;
        for (const auto &query : queries) {
            if (!query.is_string() || query.get_ref<const std::string &>().empty())
                throw NativeError(ErrorCode::InvalidParams, "queries entries must be non-empty strings");
            list.push_back(query.get<std::string>());
        }
        bool unsaved = false;
        {
            std::lock_guard guard(mutex_);
            for (const auto &[path, doc] : documents_)
                unsaved |= doc.dirty;
            unsaved |= !untitled_.empty();
            for (const auto &[path, doc] : pagedDocuments_)
                unsaved |= doc.knownDirty;
        }
        if (engine == "scan" || (engine == "auto" && !unsaved && !waitForIndex(params, cancellation))) {
            const bool sensitive = params.value("caseSensitive", false), regex = params.value("regex", false);
            const auto limit = size_t(std::clamp(params.value("limit", 500), 1, 5000));
            std::vector<std::string> args{"rg", "--json", "--hidden", "--no-require-git", "-g", "!.git", "-g",
                                          "!/runtime", "-g", "!/data", "-g", "!.cycode"};
            if (!sensitive)
                args.push_back("--ignore-case");
            if (!regex)
                args.push_back("--fixed-strings");
            if (params.contains("extensions"))
                for (const auto &extension : params.at("extensions")) {
                    auto value = extension.get<std::string>();
                    if (value.find_first_of("*?[]{}!/\\") != std::string::npos || value.size() > 32)
                        throw NativeError(ErrorCode::InvalidParams, "extensions must be plain file extensions");
                    if (!value.starts_with("."))
                        value.insert(value.begin(), '.');
                    args.insert(args.end(), {"--iglob", "*" + value});
                }
            for (const auto &query : list)
                args.insert(args.end(), {"-e", query});
            args.push_back("--");
            if (params.contains("paths") && !params.at("paths").empty()) {
                for (const auto &prefix : params.at("paths"))
                    args.push_back(pathUtf8(resolve(prefix.get<std::string>()).lexically_relative(root_)));
            } else
                args.push_back(".");
            const ForegroundScan foreground(codeIndex_);
            const auto scan = runProcess(args, root_, std::chrono::seconds(60), 32 * 1024 * 1024, cancellation);
            if (scan.cancelled)
                return {{"results", Json::array()}, {"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
            if (scan.exitCode > 1 && !scan.truncated && !scan.timedOut)
                throw std::runtime_error(scan.output);
            std::vector<std::tuple<std::string, uint32_t, std::string>> lines;
            std::istringstream stream(scan.output);
            std::string row;
            while (std::getline(stream, row)) {
                const auto item = Json::parse(row, nullptr, false);
                if (item.is_discarded() || item.value("type", "") != "match")
                    continue;
                const auto &d = item.at("data");
                if (!d.at("path").contains("text") || !d.at("lines").contains("text"))
                    continue;
                auto path = d.at("path").at("text").get<std::string>();
                std::replace(path.begin(), path.end(), '\\', '/');
                if (path.starts_with("./"))
                    path.erase(0, 2);
                lines.emplace_back(std::move(path), d.at("line_number").get<uint32_t>(),
                                   d.at("lines").at("text").get<std::string>());
            }
            auto attributed = attributeScanLines(list, regex, sensitive, lines, limit);
            const auto status = codeIndex_.status();
            Json brief = {{"phase", status.at("phase")}, {"ready", status.at("ready")}};
            if (status.contains("progress"))
                brief["progress"] = status.at("progress");
            const auto maxBytes = searchByteBudget(params);
            size_t used = 0;
            for (auto &block : attributed) {
                if (keepWithinBudget(block["matches"], maxBytes, used)) {
                    block["truncated"] = true;
                    block["budgetReached"] = true;
                }
                block["engine"] = "scan";
                block["fallback"] = engine == "scan" ? "requested" : "index-not-ready";
                block["coverage"] = {{"complete", !scan.truncated && !scan.timedOut && !block.at("truncated").get<bool>()},
                                     {"filteredByIndex", false}, {"combinedScan", true}};
                block["nextCursor"] = nullptr;
                truncated |= block.at("truncated").get<bool>();
            }
            Json result = {{"results", attributed}, {"matches", Json::array()}, {"truncated", truncated || scan.truncated},
                           {"timedOut", scan.timedOut}, {"engine", "scan"}, {"index", brief}};
            if (maxBytes)
                result["budget"] = {{"maxBytes", maxBytes}, {"usedBytes", used}};
            if (engine != "scan")
                result["fallback"] = "index-not-ready";
            return result;
        }
        // One byte budget for the whole batch: half of it is reserved in equal shares, so a broad
        // first query cannot starve the others, and each query may use what is left minus the
        // shares still reserved for the queries after it. The batch stays within maxBytes, apart
        // from the one row a page always returns.
        const auto maxBytes = searchByteBudget(params);
        const size_t share = maxBytes / (2 * queries.size());
        size_t used = 0, index = 0;
        for (const auto &query : queries) {
            auto single = params;
            single.erase("queries");
            single["query"] = query;
            const size_t reserved = share * (queries.size() - ++index);
            if (maxBytes)
                single["maxBytes"] = std::max<size_t>({maxBytes - std::min(used + reserved, maxBytes), share, 1});
            auto result = searchText(single, cancellation);
            if (result.contains("budget"))
                used += result.at("budget").value("usedBytes", size_t(0));
            truncated |= result.value("truncated", false);
            allIndexed &= result.value("engine", std::string()) == "index";
            Json block = {{"query", query}, {"engine", result.value("engine", std::string())},
                          {"truncated", result.value("truncated", false)}, {"matches", result.at("matches")},
                          {"nextCursor", result.value("nextCursor", Json())}};
            for (const auto *key : {"files", "coverage", "fallback"})
                if (result.contains(key))
                    block[key] = result.at(key);
            if (result.contains("budget") && result.at("budget").value("reached", false))
                block["budgetReached"] = true;
            results.push_back(std::move(block));
        }
        Json batch = {{"results", results}, {"matches", Json::array()}, {"truncated", truncated},
                      {"engine", allIndexed ? "index" : "scan"}};
        if (maxBytes)
            batch["budget"] = {{"maxBytes", maxBytes}, {"usedBytes", used}};
        return batch;
    }
    if (!params.contains("query"))
        throw NativeError(ErrorCode::InvalidParams, "query or queries is required");
    const auto engine = params.value("engine", std::string("auto"));
    if (engine != "auto" && engine != "index" && engine != "scan")
        throw NativeError(ErrorCode::InvalidParams, "engine must be auto, index or scan");
    if (params.at("query").get<std::string>().empty())
        return {{"matches", Json::array()}, {"truncated", false}, {"engine", "none"}, {"nextCursor", nullptr}};
    std::string fallback;
    if (engine != "scan") {
        if (codeIndex_.disabled())
            fallback = "index-disabled";
        else {
            std::map<std::string, std::string> unsaved;
            bool pagedDirty = false;
            {
                std::lock_guard guard(mutex_);
                for (const auto &[path, doc] : documents_)
                    if (doc.dirty)
                        unsaved[path] = doc.text;
                for (const auto &[path, doc] : untitled_)
                    unsaved[path] = doc.text;
                for (const auto &[path, doc] : pagedDocuments_)
                    pagedDirty |= doc.knownDirty;
            }
            if (pagedDirty)
                fallback = "unsaved-paged-documents";
            else {
                const bool usable = engine == "index"
                                        ? codeIndex_.ready(std::chrono::milliseconds(std::clamp<int64_t>(
                                                               params.value("waitMs", int64_t(600000)), 0, 3600000)),
                                                           cancellation)
                                        : waitForIndex(params, cancellation);
                if (usable)
                    return codeIndex_.searchText(params, unsaved, cancellation);
                fallback = "index-not-ready";
            }
        }
        if (engine == "index")
            throw NativeError(ErrorCode::BackendUnavailable, "Project index unavailable: " + fallback,
                              {{"index", codeIndex_.status()}});
    }
    if (params.contains("cursor") && !params.at("cursor").is_null())
        throw NativeError(ErrorCode::InvalidParams, "cursor pagination requires the project index (engine index/auto)");
    auto result = searchTextScan(params, cancellation);
    result["engine"] = "scan";
    if (!fallback.empty())
        result["fallback"] = fallback;
    if (engine != "scan") {
        const auto status = codeIndex_.status();
        Json brief = {{"phase", status.at("phase")}, {"ready", status.at("ready")}};
        if (status.contains("progress"))
            brief["progress"] = status.at("progress");
        if (status.contains("disabledReason"))
            brief["disabledReason"] = status.at("disabledReason");
        if (!status.at("ready").get<bool>() && !status.contains("disabledReason"))
            brief["advice"] = "The project index is being prepared (see progress.etaSeconds). This result is a full "
                              "ripgrep scan; for many searches, wait with index_prepare or retry search_text later "
                              "rather than scanning with shell tools.";
        result["index"] = std::move(brief);
    }
    result["coverage"] = {{"complete", !result.value("truncated", false) && !result.value("timedOut", false) &&
                                           !result.value("cancelled", false)},
                          {"filteredByIndex", false}};
    result["nextCursor"] = nullptr;
    if (params.value("output", std::string("matches")) == "files") {
        std::map<std::string, Json> files;
        for (const auto &match : result.at("matches")) {
            auto &entry = files[match.at("path").get<std::string>()];
            if (entry.is_null())
                entry = {{"path", match.at("path")}, {"matches", 0}, {"firstLine", match.at("line")},
                         {"moreInFile", false}, {"symbols", Json::array()}};
            entry["matches"] = entry["matches"].get<int>() + 1;
        }
        result["files"] = Json::array();
        for (auto &[path, entry] : files)
            result["files"].push_back(std::move(entry));
        result["matches"] = Json::array();
    }
    if (const auto maxBytes = searchByteBudget(params)) {
        size_t used = 0;
        const bool cut = keepWithinBudget(result[result.contains("files") ? "files" : "matches"], maxBytes, used);
        if (cut) {
            result["truncated"] = true;
            result["coverage"]["complete"] = false;
        }
        result["budget"] = {{"maxBytes", maxBytes}, {"usedBytes", used}, {"reached", cut}};
    }
    return result;
}
// engine=auto: an explicit waitMs wins. Otherwise wait 30 s for the reload of a persisted
// generation. A first build is waited for (up to 90 s, when it announces at most 2 minutes
// left) only in an agent service that started it at launch (CYCODE_INDEX_AT_START=1): an
// interactive host such as the IDE starts the build in the background and searches with
// ripgrep meanwhile, as before the index existed.
bool Engine::waitForIndex(const Json &params, const std::atomic<bool> *cancellation) {
    if (params.contains("waitMs"))
        return codeIndex_.ready(
            std::chrono::milliseconds(std::clamp<int64_t>(params.value("waitMs", int64_t(0)), 0, 3600000)), cancellation);
    if (codeIndex_.persisted())
        return codeIndex_.ready(std::chrono::seconds(30), cancellation);
    if (!CodeIndex::startsAtLaunch())
        return codeIndex_.ready(std::chrono::milliseconds(0), cancellation);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
    while (!codeIndex_.ready(std::chrono::seconds(1), cancellation)) {
        if (cancellation && cancellation->load())
            return false;
        const auto status = codeIndex_.status();
        const auto phase = status.value("phase", std::string());
        if (phase == "failed" || phase == "disabled" || std::chrono::steady_clock::now() >= deadline)
            return false;
        if (status.contains("progress") && status.at("progress").contains("etaSeconds") &&
            status.at("progress").at("etaSeconds").get<double>() > 120)
            return false;
    }
    return true;
}
Json Engine::indexOperation(const std::string &method, const Json &params, const std::atomic<bool> *cancellation) {
    if (method == "index.status")
        return codeIndex_.status();
    if (method == "index.prepare")
        return codeIndex_.prepare(params, cancellation);
    if (method == "index.outline" || method == "index.read") {
        // One workspace file: the live buffer when it is open, otherwise disk.
        struct Loaded {
            std::string key, text;
            Json source;
            uint64_t revision = 0;
        };
        auto load = [&](const std::string &path) {
            Loaded doc;
            const auto file = resolve(path);
            doc.key = pathUtf8(file.lexically_relative(root_));
            doc.source = "disk";
            {
                std::lock_guard guard(mutex_);
                if (const auto found = documents_.find(doc.key); found != documents_.end()) {
                    doc.text = found->second.text;
                    doc.revision = found->second.revision;
                    doc.source = found->second.dirty ? "unsaved-buffer" : "open-document";
                }
            }
            if (doc.source == "disk") {
                std::error_code ec;
                if (!std::filesystem::is_regular_file(file, ec))
                    throw NativeError(ErrorCode::FileSystemError, "Not a workspace file: " + path);
                const auto size = std::filesystem::file_size(file, ec);
                if (ec || size > 16 * 1024 * 1024)
                    throw NativeError(ErrorCode::CapacityExceeded, "Reads are limited to files of 16 MiB");
                std::ifstream input(file, std::ios::binary);
                doc.text.assign(std::istreambuf_iterator<char>(input), {});
            }
            return doc;
        };
        auto annotate = [](Json result, const Loaded &doc) {
            result["textSource"] = doc.source;
            if (doc.revision)
                result["revision"] = doc.revision;
            return result;
        };
        if (method == "index.outline") {
            const auto doc = load(params.at("path").get<std::string>());
            return annotate(outlineText(doc.key, doc.text, size_t(std::clamp(params.value("limit", 2000), 1, 20000))), doc);
        }
        // index.read: one range, several ranges, or the body of a declaration found by the index.
        size_t budget = 1500; // numbered lines returned by one call
        auto readRange = [&](const std::string &path, int64_t line, int64_t count) {
            const auto doc = load(path);
            const auto take = std::min<size_t>(size_t(std::clamp<int64_t>(count, 1, 600)), budget);
            if (!take)
                throw NativeError(ErrorCode::CapacityExceeded, "index.read returns at most 1500 lines per call");
            auto result = readNumberedLines(doc.key, doc.text, size_t(std::max<int64_t>(1, line)), take);
            budget -= std::min<size_t>(budget, result.at("endLine").get<size_t>() + 1 - result.at("startLine").get<size_t>());
            return annotate(std::move(result), doc);
        };
        if (params.contains("symbol")) {
            const auto query = params.at("symbol").get<std::string>();
            if (query.empty() || query.size() > 256)
                throw NativeError(ErrorCode::InvalidParams, "symbol must be a name or Scope::Name of at most 256 bytes");
            const auto wait = std::chrono::milliseconds(std::clamp<int64_t>(params.value("waitMs", int64_t(600000)), 0, 3600000));
            if (codeIndex_.disabled() || !codeIndex_.ready(wait, cancellation))
                throw NativeError(ErrorCode::BackendUnavailable, "Project index is not ready; retry or call index.prepare",
                                  {{"index", codeIndex_.status()}});
            Json lookup = {{"query", query}, {"limit", 20}};
            if (params.contains("path"))
                lookup["paths"] = Json::array({params.at("path")});
            const auto found = codeIndex_.symbols(lookup, cancellation).at("symbols");
            auto name = query;
            for (const auto *separator : {"::", "."})
                if (const auto at = name.rfind(separator); at != std::string::npos)
                    name = name.substr(at + std::strlen(separator));
            // Prefer a definition with a body whose name is exactly the requested one.
            const Json *best = nullptr;
            int bestRank = -1;
            for (const auto &row : found) {
                const bool exact = row.at("name").get<std::string>() == name;
                const bool body = row.at("endLine").get<int64_t>() > row.at("line").get<int64_t>();
                const int rank = (exact ? 4 : 0) + (body ? 2 : 0) + (row.value("declaration", false) ? 0 : 1);
                if (rank > bestRank) {
                    best = &row;
                    bestRank = rank;
                }
            }
            if (!best)
                throw NativeError(ErrorCode::InvalidParams, "No indexed declaration matches " + query);
            const auto before = std::clamp<int64_t>(params.value("before", 3), 0, 50);
            const auto first = std::max<int64_t>(1, best->at("line").get<int64_t>() - before);
            const auto last = best->at("endLine").get<int64_t>();
            auto result = readRange(best->at("path").get<std::string>(), first,
                                    std::clamp<int64_t>(last - first + 1, 1, params.value("count", int64_t(600))));
            result["symbol"] = *best;
            Json others = Json::array();
            for (const auto &row : found)
                if (&row != best && others.size() < 5)
                    others.push_back({{"name", row.at("name")}, {"kind", row.at("kind")}, {"path", row.at("path")},
                                      {"line", row.at("line")}});
            result["otherCandidates"] = others;
            return result;
        }
        if (params.contains("ranges")) {
            const auto &ranges = params.at("ranges");
            if (!ranges.is_array() || ranges.empty() || ranges.size() > 20)
                throw NativeError(ErrorCode::InvalidParams, "ranges must list 1 to 20 {path, line, count} objects");
            // One unreadable range is reported in place; it does not discard the other reads.
            Json reads = Json::array();
            size_t failed = 0;
            for (const auto &range : ranges) {
                try {
                    reads.push_back(readRange(range.at("path").get<std::string>(), range.value("line", int64_t(1)),
                                              range.value("count", int64_t(120))));
                } catch (const std::exception &error) {
                    ++failed;
                    reads.push_back({{"path", range.value("path", std::string())}, {"error", error.what()}});
                }
            }
            if (failed == ranges.size())
                throw NativeError(ErrorCode::InvalidParams, "No requested range could be read",
                                  {{"reads", reads}});
            return {{"reads", reads}, {"remainingLineBudget", budget}, {"failedRanges", failed}};
        }
        return readRange(params.at("path").get<std::string>(), params.value("line", int64_t(1)),
                         params.value("count", int64_t(120)));
    }
    if (method == "index.symbols") {
        const auto wait = std::chrono::milliseconds(
            std::clamp<int64_t>(params.value("waitMs", int64_t(600000)), 0, 3600000));
        if (codeIndex_.disabled())
            throw NativeError(ErrorCode::BackendUnavailable, "Project index disabled", {{"index", codeIndex_.status()}});
        if (!codeIndex_.ready(wait, cancellation))
            throw NativeError(ErrorCode::BackendUnavailable,
                              "Project index is not ready; call index.prepare or retry with a larger waitMs",
                              {{"index", codeIndex_.status()}});
        return codeIndex_.symbols(params, cancellation);
    }
    throw NativeError(ErrorCode::UnsupportedOperation, "Unknown index operation");
}
Json Engine::searchTextScan(const Json &params, const std::atomic<bool> *cancellation) {
    auto query = params.at("query").get<std::string>();
    if (query.empty())
        return {{"matches", Json::array()}, {"truncated", false}};
    auto limit = std::clamp(params.value("limit", 500), 1, 5000);
    bool sensitive = params.value("caseSensitive", false);
    bool regex = params.value("regex", false);
    // runtime/ and data/ are excluded only at the workspace root; nested folders with
    // those names are ordinary project content.
    std::vector<std::string> args{"rg", "--json", "--hidden", "--no-require-git",
                                  "-g", "!.git",  "-g",       "!/runtime",
                                  "-g", "!/data", "-g",       "!.cycode"};
    if (!sensitive)
        args.push_back("--ignore-case");
    if (!regex)
        args.push_back("--fixed-strings");
    if (params.contains("maxPerFile"))
        args.insert(args.end(), {"--max-count", std::to_string(std::clamp(params.value("maxPerFile", 1000), 1, 5000))});
    if (params.contains("extensions"))
        for (const auto &extension : params.at("extensions")) {
            auto value = extension.get<std::string>();
            if (value.find_first_of("*?[]{}!/\\") != std::string::npos || value.size() > 32)
                throw NativeError(ErrorCode::InvalidParams, "extensions must be plain file extensions");
            if (!value.starts_with("."))
                value.insert(value.begin(), '.');
            args.insert(args.end(), {"--iglob", "*" + value});
        }
    args.insert(args.end(), {"--", query});
    if (params.contains("paths") && !params.at("paths").empty()) {
        for (const auto &prefix : params.at("paths"))
            args.push_back(pathUtf8(resolve(prefix.get<std::string>()).lexically_relative(root_)));
    } else
        args.push_back(".");
    const ForegroundScan foreground(codeIndex_);
    auto result = runProcess(args, root_, std::chrono::seconds(15), 8 * 1024 * 1024, cancellation);
    if (result.cancelled)
        return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
    if (result.exitCode > 1 && !result.truncated && !result.timedOut)
        throw std::runtime_error(result.output);
    Json matches = Json::array();
    std::map<std::string, std::string> dirty;
    struct PagedSearch {
        std::shared_ptr<LargeText> text;
        LargeTextSnapshot snapshot;
        uint64_t identity, revision;
    };
    std::map<std::string, PagedSearch> paged;
    {
        std::lock_guard guard(mutex_);
        for (const auto &[path, doc] : documents_)
            if (doc.dirty)
                dirty[path] = doc.text;
        for (const auto &[path, doc] : untitled_)
            dirty[path] = doc.text;
        for (const auto &[path, doc] : pagedDocuments_)
            if (doc.knownDirty) {
                if (doc.busy)
                    throw NativeError(ErrorCode::BackendUnavailable,
                                      "Paged document has active I/O; retry search");
                paged.emplace(path, PagedSearch{doc.text, doc.text->snapshot(), doc.identity, doc.revision});
            }
    }
    auto record = [&](const std::string &path, size_t line, size_t column, std::string_view text) {
        if (text.size() > 512) {
            size_t end = 512;
            while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                --end;
            text = text.substr(0, end); // Allocate only the scalar-safe bounded preview.
        }
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
            text.remove_suffix(1);
        if (matches.size() < static_cast<size_t>(limit))
            matches.push_back(
                {{"path", path}, {"line", line}, {"column", column}, {"text", std::string(text)}});
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
        if (dirty.contains(path) || paged.contains(path))
            continue;
        for (const auto &sub : d["submatches"]) {
            if (matches.size() >= static_cast<size_t>(limit)) {
                over = true;
                break;
            }
            record(path, d["line_number"].get<size_t>(), sub["start"].get<size_t>() + 1,
                   d["lines"]["text"].get_ref<const std::string &>());
        }
    }
    // Ripgrep supplies the same Unicode simple-case folding as disk searches.
    // Keep the in-memory fast path for byte-exact and all-ASCII literal searches.
    const auto nonAscii = [](const std::string &value) {
        return std::any_of(value.begin(), value.end(), [](unsigned char c) { return c >= 128; });
    };
    const bool unicodeCaseless =
        !sensitive && (nonAscii(query) || std::any_of(dirty.begin(), dirty.end(), [&](const auto &entry) {
                           return nonAscii(entry.second);
                       }));
    const bool snapshotSearch = regex || unicodeCaseless;
    // Unsaved buffers supersede disk results. Search host-owned numeric snapshots,
    // never project files, through the existing bounded/cancellable process owner.
    if ((snapshotSearch && !dirty.empty()) || !paged.empty()) {
        struct Snapshots {
            std::filesystem::path folder, parent;
            std::vector<std::string> owned;
            ~Snapshots() {
                if (folder.empty())
                    return;
                std::error_code error;
                if (std::filesystem::is_symlink(folder, error) ||
                    std::filesystem::canonical(folder, error) != folder || folder.parent_path() != parent ||
                    error)
                    return;
                for (const auto &name : owned)
                    std::filesystem::remove(folder / name, error);
                // Unknown entries prevent removal; no recursive cleanup or link traversal.
                std::filesystem::remove(folder, error);
            }
        } snapshots;
        std::random_device random;
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        snapshots.parent = parent;
        for (int attempt = 0; attempt < 16; ++attempt) {
            const auto candidate =
                parent / ("cycode-search-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (std::filesystem::create_directory(candidate)) {
                snapshots.folder = candidate;
                break;
            }
        }
        if (snapshots.folder.empty())
            throw std::runtime_error("Cannot create private search snapshots");
#ifndef _WIN32
        std::filesystem::permissions(snapshots.folder, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
#endif
        std::map<std::string, std::string> paths;
        size_t bytes = 0;
        if (snapshotSearch)
            for (const auto &[path, text] : dirty) {
                if (cancellation && cancellation->load())
                    return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
                bytes += text.size();
                if (paths.size() >= 128 || bytes > 64 * 1024 * 1024)
                    throw std::runtime_error("Unsaved snapshot search exceeds 128 buffers / 64 MiB");
                const auto name = std::to_string(paths.size()) + ".txt";
                snapshots.owned.push_back(name);
                std::ofstream out(snapshots.folder / name, std::ios::binary);
                out.write(text.data(), std::streamsize(text.size()));
                if (!out)
                    throw std::runtime_error("Cannot write search snapshot");
                paths[name] = path;
            }
        for (const auto &[path, doc] : paged) {
            const auto available = std::filesystem::space(snapshots.folder).available;
            if (doc.snapshot->bytes > available || available - doc.snapshot->bytes < 1024 * 1024)
                throw NativeError(ErrorCode::CapacityExceeded,
                                  "Insufficient disk space for paged search snapshot");
            const auto name = std::to_string(paths.size()) + ".txt";
            AtomicFileOptions options;
            options.access = AtomicAccess::PrivateUser;
            writeFileAtomicStream(
                snapshots.folder / name,
                [&](const AtomicByteSink &sink) {
                    for (const auto &piece : doc.snapshot->pieces)
                        for (uint64_t offset = 0; offset < piece.bytes;) {
                            if (cancellation && cancellation->load())
                                throw NativeError(ErrorCode::Cancelled, "Paged snapshot search cancelled");
                            const auto length = std::min<uint64_t>(65536, piece.bytes - offset);
                            const auto bytes = piece.blob->read(piece.offset + offset, size_t(length));
                            sink(bytes);
                            offset += length;
                        }
                },
                options);
            snapshots.owned.push_back(name);
            paths[name] = path;
        }
        std::vector<std::string> arguments = {"rg",       "--json",      "--no-ignore",
                                              "--hidden", "--max-count", std::to_string(limit)};
        if (!sensitive)
            arguments.push_back("--ignore-case");
        if (!regex)
            arguments.push_back("--fixed-strings");
        arguments.insert(arguments.end(), {"--", query, "."});
        const auto live =
            runProcess(arguments, snapshots.folder, std::chrono::seconds(15), 8 * 1024 * 1024, cancellation);
        if (live.cancelled)
            return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
        if (live.exitCode > 1 && !live.truncated && !live.timedOut)
            throw std::runtime_error(live.output);
        result.truncated = result.truncated || live.truncated;
        result.timedOut = result.timedOut || live.timedOut;
        std::istringstream rows(live.output);
        std::string row;
        while (std::getline(rows, row)) {
            const auto item = Json::parse(row, nullptr, false);
            if (item.is_discarded() || item.value("type", "") != "match")
                continue;
            const auto &data = item.at("data");
            if (!data.at("path").contains("text") || !data.at("lines").contains("text"))
                continue;
            const auto file = pathUtf8(utf8Path(data.at("path").at("text").get<std::string>()).filename());
            if (!paths.contains(file))
                continue;
            for (const auto &sub : data.at("submatches")) {
                if (matches.size() >= size_t(limit)) {
                    over = true;
                    break;
                }
                record(paths.at(file), data.at("line_number"), sub.at("start").get<size_t>() + 1,
                       data.at("lines").at("text").get_ref<const std::string &>());
            }
        }
    }
    if (!snapshotSearch) {
        const auto lower = [](unsigned char c) {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
        };
        std::string needle = query;
        if (!sensitive)
            std::transform(needle.begin(), needle.end(), needle.begin(), lower);
        for (const auto &[path, text] : dirty) {
            if (over)
                break;
            size_t number = 0, start = 0;
            while (start < text.size()) {
                if (cancellation && cancellation->load())
                    return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}};
                ++number;
                const auto newline = text.find('\n', start);
                const auto current = std::string_view(text).substr(
                    start, newline == std::string::npos ? text.size() - start : newline - start);
                std::string folded;
                std::string_view haystack(current);
                if (!sensitive) {
                    folded = current;
                    std::transform(folded.begin(), folded.end(), folded.begin(), lower);
                    haystack = folded;
                }
                size_t at = 0;
                while ((at = haystack.find(needle, at)) != std::string_view::npos) {
                    if (matches.size() >= static_cast<size_t>(limit)) {
                        over = true;
                        break;
                    }
                    record(path, number, at + 1, current);
                    at += std::max<size_t>(needle.size(), 1);
                }
                if (over || newline == std::string::npos)
                    break;
                start = newline + 1;
            }
        }
    }
    if (!paged.empty()) {
        std::lock_guard guard(mutex_);
        for (const auto &[path, snapshot] : paged) {
            const auto current = pagedDocuments_.find(path);
            if (current == pagedDocuments_.end() || current->second.identity != snapshot.identity ||
                current->second.revision != snapshot.revision || current->second.text != snapshot.text ||
                current->second.text->snapshot() != snapshot.snapshot)
                throw NativeError(ErrorCode::RevisionConflict, "Paged document changed during global search");
        }
    }
    return {{"matches", matches},
            {"truncated", over || result.truncated},
            {"timedOut", result.timedOut},
            {"unsavedRegexExcluded", false}};
}
Json Engine::git(const std::string &method, const Json &params) {
    std::lock_guard vcs(vcsMutex_);
    if (method.starts_with("vcs.replay."))
        return gitReplay(method, params);
    if (method.starts_with("vcs.merge."))
        return gitMerge(method, params);
    if (method.starts_with("vcs.checkout."))
        return gitCheckout(method, params);
    if (method.starts_with("vcs.conflict."))
        return gitConflicts(method, params);
    if (method == "vcs.worktrees" || method.starts_with("vcs.worktree."))
        return gitWorktrees(method, params);
    if (method == "vcs.submodules" || method.starts_with("vcs.submodule."))
        return gitSubmodules(method, params);
    if (method == "vcs.hunks" || method == "vcs.applyHunks")
        return gitHunks(method, params);
    if (method != "vcs.status" && method != "vcs.diff" && method != "vcs.log" && method != "vcs.stage" &&
        method != "vcs.unstage" && method != "vcs.commit")
        return gitAdvanced(method, params);
    const auto selected = gitSelection(params);
    const auto &gitRoot = selected.directory;
    std::vector<std::string> args{"git", "--literal-pathspecs", "-C", pathUtf8(gitRoot)};
    hardenGitArguments(args);
    if (method == "vcs.status")
        args.insert(args.end(),
                    {"status", "--porcelain=v1", "-z", "--branch", "--untracked-files=all", "--", "."});
    else if (method == "vcs.diff") {
        // --no-textconv: a repository diff driver must not run a program from .git/config.
        args.insert(args.end(), {"diff", "--no-ext-diff", "--no-textconv", "--no-color"});
        if (params.value("staged", false))
            args.push_back("--cached");
        args.push_back("--");
        if (params.contains("path"))
            args.push_back(gitFileArgument(root_, selected, resolve(params.at("path")), true));
        else
            args.push_back(".");
    } else if (method == "vcs.log")
        args.insert(args.end(), {"log", "-30", "--date=short", "--format=%h %ad %s", "--", "."});
    else if (method == "vcs.stage") {
        args.push_back("add");
        args.push_back("--");
        args.push_back(gitFileArgument(root_, selected, resolve(params.at("path")), true));
    } else if (method == "vcs.unstage") {
        auto head = runProcess({"git", "-C", pathUtf8(gitRoot), "rev-parse", "--verify", "HEAD"}, gitRoot);
        if (head.exitCode == 0)
            args.insert(args.end(), {"restore", "--staged", "--"});
        else
            args.insert(args.end(), {"rm", "--cached", "--ignore-unmatch", "--"});
        args.push_back(gitFileArgument(root_, selected, resolve(params.at("path")), true));
    } else if (method == "vcs.commit") {
        const auto owner =
            runProcess({"git", "-C", pathUtf8(gitRoot), "rev-parse", "--show-toplevel"}, gitRoot);
        auto top = owner.output;
        while (!top.empty() && (top.back() == '\n' || top.back() == '\r'))
            top.pop_back();
        if (owner.exitCode || !std::filesystem::equivalent(utf8Path(top), gitRoot))
            throw std::runtime_error("Select the repository root before committing staged changes");
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
            {"output", processOutputText(result.output, true)},
            {"timedOut", result.timedOut},
            {"truncated", result.truncated}};
}
Json Engine::dispatch(const std::string &method, const Json &params) {
    if (shuttingDown_ && method != "build.cancel" && method != "tests.cancel" && method != "project.cancel")
        throw std::runtime_error("Workspace is closing");
    if (method == "documents.checkpointPaged" || method == "documents.pagedCheckpoints" ||
        method == "documents.previewPagedCheckpoint" || method == "documents.restorePagedCheckpoint")
        return pagedRecoveryOperation(method, params);
    if (method == "documents.readPage" || method == "documents.prepareByteEdit" ||
        method == "documents.savePaged" || method == "documents.closePaged" ||
        method == "documents.readLines" || method == "documents.savePagedAs")
        return pagedDocumentOperation(method, params);
    const bool compilerQuery =
        method == "analysis.prepare" ||
        (method.starts_with("language.") && method != "language.status" && method != "language.configure" &&
         method != "language.preferences" && method != "language.preferencesForget");
    if (compilerQuery) {
        std::lock_guard lock(mutex_);
        for (const auto &[path, doc] : pagedDocuments_)
            if (doc.knownDirty || doc.busy)
                throw NativeError(
                    ErrorCode::BackendUnavailable,
                    "Save or close paged edits before compiler queries; paged storage has no LSP overlay");
    }
    if (method == "documents.createUntitled")
        return createUntitled(params);
    if (method == "documents.saveAs")
        return saveDocumentAs(params);
    // Cancellation must never wait for plugin/adapter/document locks: a plugin
    // callback can itself be waiting for the build process being cancelled.
    if (method == "build.cancel")
        return buildOperations_.cancel(params);
    if (method == "events.read")
        return events_.read(params);
    if (method == "build.status")
        return buildOperations_.status();
    if (method == "build.output")
        return buildOperations_.output(params);
    if (method == "build.location")
        return compilerLocation(params, root_, [this](const std::string &path) { return resolve(path); });
    if (method == "tests.cancel")
        return tests_.cancel(params);
    if (method == "project.cancel")
        return projects_.cancel(params.value("operationId", ""));
    if (method == "project.status")
        return projects_.status();
    if (method == "project.output")
        return projects_.output(params);
    refreshPluginServices();
    if (BuildOperations::supports(method)) {
        std::string operation;
        try {
            auto result =
                buildOperations_.execute(method, params, [&](const std::atomic<bool> *cancellation) {
                    operation = buildOperations_.status().at("operationId");
                    events_.publish("build.started", {{"operationId", operation}, {"method", method}});
                    return buildOperation(method, params, cancellation);
                });
            events_.publish("build.finished", {{"operationId", operation},
                                               {"method", method},
                                               {"exitCode", result.value("exitCode", -1)},
                                               {"cancelled", result.value("cancelled", false)},
                                               {"timedOut", result.value("timedOut", false)},
                                               {"truncated", result.value("truncated", false)}});
            return result;
        } catch (const std::exception &e) {
            if (!operation.empty())
                events_.publish("build.finished",
                                {{"operationId", operation}, {"method", method}, {"error", e.what()}});
            throw;
        }
    }
    if (method == "tests.discover")
        return tests_.discover(params);
    if (method == "tests.run") {
        {
            std::lock_guard lock(mutex_);
            for (const auto &[path, doc] : documents_)
                if (doc.dirty)
                    throw std::runtime_error("Save modified buffers before running project tests");
        }
        return tests_.run(params);
    }
    if (method == "templates.list")
        return templates_->list();
    if (method.starts_with("packages.")) {
        PluginPackages manager(installationRoot());
        if (method == "packages.list")
            return manager.catalog();
        if (method == "packages.inspect")
            return manager.inspect(resolve(params.at("source")));
        if (method == "packages.install")
            return manager.install(resolve(params.at("source")), params.at("expectedRevision"),
                                   params.at("expectedTrustRevision"), params.at("expectedPackage"));
        if (method == "packages.remove")
            return manager.remove(params.at("id"), params.at("expectedRevision"));
        if (method == "packages.rollback")
            return manager.rollback(params.at("id"), params.at("digest"), params.at("expectedRevision"),
                                    params.at("expectedTrustRevision"));
        if (method == "packages.clean")
            return manager.clean(params.at("id"), params.at("digest"), params.at("expectedRevision"));
        throw std::runtime_error(
            "Package operation is not exported; publisher trust is managed explicitly by the desktop");
    }
    if (method == "templates.preview")
        return templates_->preview(params);
    if (method == "templates.create")
        return templates_->create(params.at("id"));
    if (method.starts_with("resources."))
        return resourceOperation(method, params);
    if (method.starts_with("recovery."))
        return recoveryOperation(method, params);
    if (method == "workspace.poll")
        return pollWorkspace();
    if (method == "project.inspect")
        return projects_.inspect(params);
    if (method.starts_with("project.")) {
        {
            std::lock_guard lock(mutex_);
            for (const auto &[path, document] : documents_)
                if (document.dirty)
                    throw std::runtime_error("UnsavedDocuments: save " + path +
                                             " before executing project tasks");
        }
        return projects_.execute(method, params);
    }
    if (method == "msbuild.inspect")
        return msbuild_.inspect(params);
    if (method == "msbuild.tools")
        return msbuild_.tools(params.value("refresh", false));
    if (method.starts_with("debug."))
        return debugger_.execute(method, params);
    if (method == "terminal.create")
        return terminals_.create(
            resolve(params.value("cwd", ".")), Terminals::shell(params.value("profile", "default")),
            params.value("columns", 100), params.value("rows", 24), params.value("env", Json::object()));
    if (method == "terminal.list")
        return terminals_.list();
    if (method.starts_with("terminal.")) {
        const auto id = params.at("id").get<std::string>();
        if (method == "terminal.read") {
            const auto snap =
                terminals_.snapshot(id, params.value("firstRow", 0), params.value("limit", 200));
            Json lines = Json::array();
            for (const auto &line : snap.lines)
                lines.push_back(Terminals::text(line));
            return {{"id", id},
                    {"firstRow", snap.first},
                    {"totalRows", snap.total},
                    {"columns", snap.columns},
                    {"rows", snap.rows},
                    {"cursor", {{"row", snap.cursorRow}, {"column", snap.cursorColumn}}},
                    {"running", snap.running},
                    {"alternate", snap.alternate},
                    {"generation", snap.generation},
                    {"lines", lines}};
        }
        if (method == "terminal.write")
            terminals_.write(id, params.at("data"));
        else if (method == "terminal.resize")
            terminals_.resize(id, params.at("columns"), params.at("rows"));
        else if (method == "terminal.key")
            terminals_.key(id, params.at("key"), params.value("modifiers", 0));
        else if (method == "terminal.close")
            terminals_.close(id);
        else if (method == "terminal.clear")
            terminals_.clear(id);
        else
            throw std::runtime_error("Unknown terminal operation");
        return {{"id", id}, {"ok", true}};
    }
    if (method == "language.highlight")
        return highlight(params);
    if (method == "language.hierarchyPrepare" || method == "language.hierarchyExpand" ||
        method == "language.hierarchyStatus")
        return hierarchyOperation(method, params);
    if (method == "language.symbols" || method == "language.documentSymbols")
        return symbolQuery(method, params);
    if (method == "language.symbolResolve")
        return resolveSymbol(params);
    if (method == "language.rename" || method == "language.format")
        return prepareChange(method, params);
    if (method == "changes.historyStatus" || method == "changes.historyPrepare")
        return changeHistoryOperation(method, params);
    if (method == "changes.preview" || method == "changes.apply" || method == "changes.undo" ||
        method == "changes.status")
        return changeOperation(method, params);
    if (method == "sources.read" || method == "sources.close")
        return sourceOperation(method, params);
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
        languagePreferences_.update(
            params.at("language"), std::nullopt,
            LanguagePreferences::revisionValue(params.at("expectedPreferencesRevision")));
        return languagePreferences_.snapshot(false);
    }
    if (method == "language.status")
        return languageStatus();
    if (method == "snippets.complete")
        return snippetCompletions(params);
    if (method == "snippets.userRead")
        return userSnippets_.snapshot();
    if (method == "workspace.counterparts")
        return relatedFiles(params);
    if (method == "workspace.include")
        return relatedFiles(params, true);
    if (method == "snippets.userDraft")
        return userSnippetDraft(params);
    if (method == "snippets.userSave")
        return userSnippets_.save(params.at("text"), params.at("expectedRevision"));
    if (method == "language.completionResolve" || method == "language.completionPrepare")
        return completionOperation(method, params);
    if (method == "language.codeActions" || method == "language.codeActionPrepare")
        return codeActionOperation(method, params);
    if (locationRequest(method) || method == "language.documentHighlights" || method == "language.complete" ||
        method == "language.hover" || method == "language.signature")
        return languageQuery(method, params);
    if (method == "workspace.describe") {
        std::lock_guard lock(mutex_);
        std::string project = "Folder";
        if (!solution_.is_null())
            project = "VisualStudio";
        else if (entry_.extension() == ".vcxproj" || entry_.extension() == ".csproj" ||
                 entry_.extension() == ".fsproj" || entry_.extension() == ".vbproj")
            project = "MSBuild";
        else if (std::filesystem::exists(root_ / "CMakeLists.txt"))
            project = "CMake";
        else if (std::filesystem::exists(root_ / "Cargo.toml"))
            project = "Cargo";
        else if (std::filesystem::exists(root_ / "package.json"))
            project = "Web";
        else if (ProjectTasks::detect(root_) == "java")
            project = "Java";
        else if (ProjectTasks::detect(root_) == "maven")
            project = "Maven";
        else if (ProjectTasks::detect(root_) == "gradle")
            project = "Gradle";
        else if (ProjectTasks::detect(root_) == "shader")
            project = "Shader";
        return {{"root", pathUtf8(root_)},
                {"name", pathUtf8(root_.filename())},
                {"projectType", project},
                {"version", CYCODE_VERSION},
                {"solution", solution_}};
    }
    if (method == "analysis.prepare" || method == "analysis.status" || method == "analysis.cancel" ||
        method == "analysis.results" || method == "analysis.checkpoints")
        return analysisOperation(method, params);
    if (method == "workspace.files")
        return files(params);
    if (method == "search.text")
        return searchText(params);
    if (method == "index.status" || method == "index.prepare" || method == "index.symbols" ||
        method == "index.outline" || method == "index.read")
        return indexOperation(method, params);
    if (method == "plugins.list")
        return plugins_.describe();
    if (method == "plugins.configure") {
        auto result = plugins_.configure(params);
        refreshPluginServices();
        return result;
    }
    if (method == "plugins.project")
        return plugins_.project(params.at("provider"), params.at("action"),
                                params.value("input", Json::object()));
    if (method == "plugins.invoke")
        return plugins_.invoke(params.at("command"), params.value("input", Json::object()));
    if (method.starts_with("vcs.")) {
        return versionControl(method, params);
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
            docs.push_back({{"path", key},
                            {"id", doc.identity},
                            {"revision", doc.revision},
                            {"dirty", doc.dirty},
                            {"diskConflict", doc.diskConflict}});
        for (const auto &[key, doc] : untitled_)
            docs.push_back({{"path", key},
                            {"id", doc.identity},
                            {"revision", doc.revision},
                            {"dirty", true},
                            {"untitled", true},
                            {"displayPath", untitledDocumentName(key)},
                            {"diskConflict", ""}});
        for (const auto &[key, doc] : pagedDocuments_)
            docs.push_back(pagedDocumentDescription(key, doc));
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
        const auto found = documents_.find(key);
        if (found == documents_.end())
            throw NativeError(ErrorCode::RevisionConflict, "RevisionConflict: document closed before reload");
        auto &doc = found->second;
        if (params.at("expectedRevision") != doc.revision)
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: document changed before reload");
        if (doc.dirty && !params.value("discard", false))
            throw std::runtime_error("Save or reconcile modified text before reloading from disk");
        auto previous = documents_.extract(key);
        Json result;
        try {
            auto &reloaded = openLocked(key, false);
            reloaded.identity = previous.mapped().identity;
            language_.sync(key, reloaded.text, reloaded.revision, reloaded.identity);
            result = documentJson(key, reloaded);
            documentEvent("document.reloaded", key, reloaded);
        } catch (...) {
            documents_.erase(key);
            documents_.insert(std::move(previous));
            throw;
        }
        guard.unlock();
        retireRecovery(key);
        return result;
    }
    if (method == "documents.replace" || method == "documents.save") {
        NativeTraceScope trace(method == "documents.save" ? NativeTraceEvent::DocumentSave
                                                          : NativeTraceEvent::DocumentReplace);
        auto path = params.at("path").get<std::string>();
        auto &doc = openLocked(path);
        auto key = documentKey(path);
        if (!params.contains("expectedRevision") ||
            params.at("expectedRevision").get<std::uint64_t>() != doc.revision)
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: document changed; read it again");
        if (method == "documents.replace") {
            auto text = params.at("text").get<std::string>();
            validateText(text);
            text = normalize(text);
            if (isUntitledDocument(key))
                checkUntitledQuota(key, text.size());
            if (text != doc.text) {
                doc.text = std::move(text);
                doc.revision = nextRevision_++;
                doc.dirty = isUntitledDocument(key) || doc.text != doc.saved;
                if (!isUntitledDocument(key))
                    language_.sync(key, doc.text, doc.revision, doc.identity);
                documentEvent("document.changed", key, doc);
            }
            return documentJson(key, doc);
        }
        if (isUntitledDocument(key))
            throw std::runtime_error("Use Save as to give this document a workspace file");
        auto full = resolve(path);
        if ((doc.diskExisted && readFile(full) != doc.disk) ||
            (!doc.diskExisted && std::filesystem::exists(full)))
            throw NativeError(ErrorCode::DiskConflict, "DiskConflict: file changed outside CyCode");
        if (!doc.dirty && doc.diskExisted) {
            auto result = documentJson(key, doc);
            guard.unlock();
            retireRecovery(key);
            return result;
        }
        const auto bytes = text::encodeBuffer(doc);
        if (!doc.diskExisted)
            std::filesystem::create_directories(full.parent_path());
        writeTextFileAtomic(full, bytes,
                            doc.diskExisted ? std::optional<std::string>(doc.disk) : std::nullopt);
        text::markBufferSaved(doc, bytes);
        documentEvent("document.saved", key, doc);
        auto result = documentJson(key, doc);
        guard.unlock();
        retireRecovery(key);
        return result;
    }
    if (method == "documents.close") {
        auto key = documentKey(params.at("path"));
        if (isUntitledDocument(key) && !params.contains("expectedRevision"))
            throw std::runtime_error("Closing an untitled document requires its expected revision");
        auto &owner = isUntitledDocument(key) ? untitled_ : documents_;
        auto found = owner.find(key);
        if (params.contains("expectedRevision") &&
            (found == owner.end() || params.at("expectedRevision") != found->second.revision))
            throw NativeError(ErrorCode::RevisionConflict,
                              "RevisionConflict: document changed before closing");
        if (found != owner.end() && found->second.dirty && !params.value("discard", false))
            throw std::runtime_error("Unsaved changes");
        if (found != owner.end())
            documentEvent("document.closed", key, found->second);
        owner.erase(key);
        if (!isUntitledDocument(key))
            language_.close(key);
        guard.unlock();
        retireRecovery(key);
        return {{"closed", true}};
    }
    throw NativeError(ErrorCode::UnsupportedOperation, "Unknown operation: " + method);
}
} // namespace cycode

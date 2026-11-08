#include "cycode/ProcessText.h"
#include "cycode/LanguageService.h"
#include <climits>
#include "cycode/TextEdits.h"
#include "cycode/Tools.h"
#include "cycode/Solution.h"
#include "cycode/SemanticTokens.h"
#include "cycode/RustAnalysis.h"
#include "cycode/RustToolchain.h"
#include "cycode/JavaDebug.h"
#include <algorithm>
#include <set>

namespace cycode {
bool LanguageService::supports(const std::string &path) {
    return !languageBackend(path).empty();
}
bool LanguageService::supportsManaged(const std::string &path) {
    auto extension = pathUtf8(utf8Path(path).extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return extension == ".cs";
}
bool LanguageService::supportsCpp(const std::string &path) {
    auto extension = utf8Path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".c" || extension == ".cpp" || extension == ".cc" || extension == ".cxx" ||
           extension == ".h" || extension == ".hpp" || extension == ".hh" || extension == ".hxx" ||
           extension == ".inl";
}
std::string LanguageService::defaultExecutable() {
    if (auto resolved = toolPath("clangd"); !resolved.empty())
        return resolved;
#ifdef _WIN32
    auto installed = std::filesystem::path("C:/Program Files/LLVM/bin/clangd.exe");
    if (std::filesystem::exists(installed))
        return pathUtf8(installed);
#endif
    return "clangd";
}
std::string LanguageService::managedExecutable() {
    if (auto resolved = toolPath("csharp_ls"); !resolved.empty())
        return resolved;
    return pathUtf8(installationRoot() /
                    "runtime/tools/csharp-ls/tools/net10.0/any/CSharpLanguageServer.dll");
}
std::string LanguageService::managedHost() {
    if (auto override = toolPath("csharp_dotnet"); !override.empty())
        return override;
    auto bundled = installationRoot() / "runtime/tools/csharp-dotnet";
#ifdef _WIN32
    bundled /= "dotnet.exe";
#else
    bundled /= "dotnet";
#endif
    if (std::filesystem::is_regular_file(bundled))
        return pathUtf8(bundled);
    return toolPath("dotnet");
}
LanguageService::LanguageService(std::filesystem::path root, bool managed)
    : LanguageService(std::move(root), std::string(managed ? "csharp" : "cpp")) {
    if (!managed)
        for (const auto &key : languageBackends())
            if (key != "cpp")
                children_[key] = std::unique_ptr<LanguageService>(new LanguageService(root_, key));
}
LanguageService::LanguageService(std::filesystem::path root, std::string backend, Json definition)
    : root_(std::move(root)), managed_(backend == "csharp"), backend_(std::move(backend)),
      enabled_(backend_ == "cpp"), definition_(std::move(definition)),
      executable_(managed_ ? managedExecutable() : defaultExecutable()) {
    const auto launch = defaultServerLaunch(root_, backend_);
    argv_ = launch.argv;
    settings_ = launch.settings;
    environment_ = launch.environment;
    if (!definition_.is_null()) {
        argv_ = definition_.at("arguments").get<std::vector<std::string>>();
        settings_ = definition_.value("settings", Json::object());
    }
    worker_ = std::thread([this] { run(); });
}
LanguageService::~LanguageService() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_one();
    worker_.join();
}
void LanguageService::configure(std::string executable, bool enabled, bool prewarm) {
    std::lock_guard route(routingMutex_);
    configureOne(std::move(executable), enabled, prewarm);
    if (children_.contains("csharp"))
        configureManaged(managedExecutable(), enabled, prewarm);
    if (!enabled)
        for (const auto &[key, child] : children_)
            if (key != "csharp")
                child->configureBackend(key, {}, Json::object(), false, false);
}
void LanguageService::configureOne(std::string executable, bool enabled, bool prewarm) {
    if (utf8Path(executable).is_relative())
        if (auto resolved = toolPath(executable); !resolved.empty())
            executable = resolved;
    {
        std::lock_guard lock(mutex_);
        executable_ = std::move(executable);
        argv_.clear();
        settings_ = Json::object();
        environment_ = Json::object();
        configuration_ = {{"executable", executable_}, {"arguments", Json::array()}, {"settings", Json::object()}};
        enabled_ = enabled;
        prewarm_ = prewarm;
        reconfigure_ = true;
        launchPrepared_ = true;
        ++epoch_;
    }
    wake_.notify_one();
}
void LanguageService::configureBuiltin(const std::string &backend, std::string executable, bool enabled, bool prewarm) {
    std::lock_guard route(routingMutex_);
    if (backend != "cpp" && backend != "csharp")
        throw std::runtime_error("Only C++ and C# have legacy executable settings");
    if (children_.contains(backend)) {
        children_.at(backend)->configureBuiltin(backend, std::move(executable), enabled, prewarm);
        return;
    }
    if (backend != backend_)
        throw std::runtime_error("Unknown language backend");
    configureOne(executable.empty() ? (managed_ ? managedExecutable() : defaultExecutable()) : std::move(executable), enabled, prewarm);
}
Json LanguageService::configuration(const std::string &backend) const {
    std::lock_guard route(routingMutex_);
    if (children_.contains(backend))
        return children_.at(backend)->configuration(backend);
    if (backend != backend_)
        throw std::runtime_error("Unknown language backend");
    std::lock_guard lock(mutex_);
    auto result = configuration_;
    result["enabled"] = enabled_;
    return result;
}
void LanguageService::configureManaged(std::string executable, bool enabled, bool prewarm) {
    std::lock_guard route(routingMutex_);
    if (children_.contains("csharp"))
        children_.at("csharp")->configure(executable.empty() ? managedExecutable() : std::move(executable),
                                          enabled, prewarm);
    else if (managed_)
        configure(executable.empty() ? managedExecutable() : std::move(executable), enabled, prewarm);
}
void LanguageService::configureBackend(const std::string &backend, const std::vector<std::string> &argv,
                                       const Json &settings, bool enabled, bool prewarm, const Json &cargoProject) {
    std::lock_guard route(routingMutex_);
    if (children_.contains(backend)) {
        children_.at(backend)->configureBackend(backend, argv, settings, enabled, prewarm, cargoProject);
        return;
    }
    if (backend != backend_)
        throw std::runtime_error("Unknown language backend");
    if (argv.size() > 128 || !settings.is_object() || settings.dump().size() > 65536)
        throw std::runtime_error("Invalid language server configuration");
    if (settings.contains("$workspaceReadyNotification") &&
        (!settings.at("$workspaceReadyNotification").is_string() ||
         (settings.at("$workspaceReadyNotification") != "" &&
          settings.at("$workspaceReadyNotification") != "experimental/serverStatus" &&
          settings.at("$workspaceReadyNotification") != "language/status")))
        throw std::runtime_error("Unsupported language server readiness notification");
    for (const auto &arg : argv)
        if (arg.size() > 8192 || arg.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid language server argument");
    auto defaults = defaultServerLaunch(root_, backend_);
    if (!definition_.is_null()) {
        defaults.argv = definition_.at("arguments").get<std::vector<std::string>>();
        defaults.settings = definition_.value("settings", Json::object());
    }
    auto resolvedSettings=settings.empty()&&argv.empty()?defaults.settings:settings;
    auto resolvedArguments=argv.empty()?defaults.argv:argv;
    auto resolvedEnvironment=argv.empty()?defaults.environment:Json::object();
    Json normalizedCargo;
    if(!cargoProject.is_null()) {
        if(backend_!="rust")throw std::runtime_error("Cargo analysis profiles require the Rust backend");
        normalizedCargo=normalizeRustAnalysisProfile(cargoProject);
        // Disable must remain possible after a manifest disappears or is moved.
        if(enabled) {
            resolvedSettings=rustAnalysisSettings(resolvedSettings,normalizedCargo,root_);
            resolvedArguments=rustAnalysisArguments(normalizedCargo,argv,defaults.argv,toolPath("rustup"));
            const auto toolchain=normalizedCargo.value("toolchain",std::string{});
            if(!toolchain.empty())resolvedEnvironment=rustToolchainEnvironment(toolchain);
        }
    }
    if (enabled && argv.empty() && backend_ == "java" && !defaults.argv.empty()) {
#ifdef _WIN32
        const auto config = "config_win";
#elif defined(__APPLE__)
        const auto config = "config_mac";
#else
        const auto config = "config_linux";
#endif
        const auto option = std::find(defaults.argv.begin(), defaults.argv.end(), "-configuration");
        const auto directory = utf8Path(*std::next(option));
        std::filesystem::create_directories(directory);
        // Copy the pristine settings, never another instance's OSGi caches/locks.
        std::filesystem::copy_file(installationRoot() / "runtime/tools/jdtls" / config / "config.ini",
                                   directory / "config.ini",
                                   std::filesystem::copy_options::overwrite_existing);
    }
    {
        std::lock_guard lock(mutex_);
        argv_ = std::move(resolvedArguments);
        settings_ = std::move(resolvedSettings);
        environment_ = std::move(resolvedEnvironment);
        configuration_ = {{"arguments", argv}, {"settings", settings}};
        if(!normalizedCargo.is_null())configuration_["cargoProject"]=std::move(normalizedCargo);
        if (!argv.empty())
            executable_ = argv.front();
        else if (backend_ == "cpp" || managed_)
            executable_ = managed_ ? managedExecutable() : defaultExecutable();
        enabled_ = enabled;
        prewarm_ = prewarm;
        reconfigure_ = true;
        launchPrepared_ = enabled || !argv.empty();
        ++epoch_;
    }
    wake_.notify_one();
}
void LanguageService::enableBackend(const std::string &key, bool enabled, bool prewarm) {
    std::lock_guard route(routingMutex_);
    if (children_.contains(key)) {
        children_.at(key)->enableBackend(key, enabled, prewarm);
        return;
    }
    if (key != backend_)
        throw std::runtime_error("Unknown language backend");
    bool prepared;
    {
        std::lock_guard lock(mutex_);
        prepared = launchPrepared_;
    }
    if (enabled && !prepared) {
        configureBackend(key, {}, Json::object(), enabled, prewarm);
        return;
    }
    {
        std::lock_guard lock(mutex_);
        enabled_ = enabled;
        prewarm_ = prewarm;
        reconfigure_ = true;
        ++epoch_;
    }
    wake_.notify_one();
}
void LanguageService::setProject(const std::filesystem::path &entry) {
    std::lock_guard route(routingMutex_);
    if (children_.contains("csharp")) {
        children_.at("csharp")->setProject(entry);
        return;
    }
    std::lock_guard lock(mutex_);
    project_ = isSolutionPath(entry) ? entry : std::filesystem::path{};
    reconfigure_ = true;
    ++epoch_;
}
void LanguageService::setCompilationDatabase(std::filesystem::path directory, bool reload) {
    {
        std::lock_guard lock(mutex_);
        if (!reload && directory == compilationDatabase_)
            return;
        compilationDatabase_ = std::move(directory);
        reconfigure_ = true;
        ++epoch_;
    }
    wake_.notify_one();
}
void LanguageService::sync(const std::string &path, const std::string &text, std::uint64_t revision, std::uint64_t identity) {
    std::lock_guard route(routingMutex_);
    const auto backend = this->backend(path);
    if (children_.contains(backend)) {
        children_.at(backend)->sync(path, text, revision, identity);
        return;
    }
    if (backend != backend_)
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = Document{text, revision, identity};
    }
    wake_.notify_one();
}
void LanguageService::close(const std::string &path) {
    std::lock_guard route(routingMutex_);
    const auto backend = this->backend(path);
    if (children_.contains(backend)) {
        children_.at(backend)->close(path);
        return;
    }
    if (backend != backend_)
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = std::nullopt;
    }
    wake_.notify_one();
}
std::future<Json> LanguageService::request(std::string method, Json params, std::shared_ptr<std::atomic<bool>> cancelled) {
    std::lock_guard route(routingMutex_);
    const auto uri = params.value("textDocument", Json::object()).value("uri", "");
    auto documentPath = uri;
    if (uri.starts_with("file:"))
        try {
            documentPath = pathUtf8(LanguageClient::fromUri(uri));
        } catch (const std::exception &) {
        }
    auto backend = params.value("cycodeLanguage", this->backend(documentPath));
    params.erase("cycodeLanguage");
    if (backend.empty())
        backend = backend_;
    if (children_.contains(backend))
        return children_.at(backend)->request(std::move(method), std::move(params), std::move(cancelled));
    if (backend != backend_)
        throw std::runtime_error("Unknown language backend");
    auto query = std::make_shared<Query>();
    query->method = std::move(method);
    query->params = std::move(params);
    query->cancelled = std::move(cancelled);
    query->deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(query->method == "cycode/javaDebugSession" ? 10 : backend_ == "java"               ? 120
                                                                : managed_ || backend_ == "rust" ? 60
                                                                                                 : 15);
    auto future = query->promise.get_future();
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || !enabled_ || outstanding_ >= 32)
            throw std::runtime_error("Language service unavailable or queue full");
        ++outstanding_;
        queries_.push_back(query);
    }
    wake_.notify_one();
    return future;
}
Json LanguageService::status() const {
    std::lock_guard route(routingMutex_);
    std::lock_guard lock(mutex_);
    auto result = status_;
    result["epoch"] = routingEpoch_ * 0x100000000ULL + epoch_;
    result["name"] =
        definition_.is_null() ? languageServerName(backend_) : definition_.at("title").get<std::string>();
    if (!definition_.is_null()) {
        result["plugin"] = definition_.value("plugin", "");
        result["languages"] = definition_.at("languages");
    }
    result["enabled"] = enabled_;
    result["available"] = backend_ == "cpp" || managed_ || !argv_.empty();
    result["arguments"] = argv_;
    result["settings"] = settings_;
    result["configuration"] = configuration_;
    if (!children_.empty()) {
        result["servers"] = {{"cpp", result}};
        for (const auto &[key, child] : children_) {
            const auto server = child->status();
            result["servers"][key] = server;
            result["servers"][key]["epoch"] = routingEpoch_ * 0x100000000ULL + server.at("epoch").get<uint64_t>();
            result["ready"] = result.value("ready", false) || server.value("ready", false);
            result["running"] = result.value("running", false) || server.value("running", false);
            for (const auto &[uri, entries] : server.at("diagnostics").items())
                result["diagnostics"][uri] = entries;
            result["diagnosticRevisions"].update(server.at("diagnosticRevisions"));
            if (server.value("enabled", false) && !server.value("error", "").empty())
                result["error"] =
                    result.value("error", "") + " " + key + ": " + server.at("error").get<std::string>();
        }
    }
    return result;
}
uint64_t LanguageService::epoch(const std::string &path) const {
    std::lock_guard route(routingMutex_);
    const auto key = backend(path);
    if (children_.contains(key))
        return routingEpoch_ * 0x100000000ULL + children_.at(key)->epoch(path);
    std::lock_guard lock(mutex_);
    return routingEpoch_ * 0x100000000ULL + epoch_;
}
uint64_t LanguageService::backendEpoch(const std::string &key) const {
    std::lock_guard route(routingMutex_);
    if (children_.contains(key)) return routingEpoch_ * 0x100000000ULL + children_.at(key)->backendEpoch(key);
    if (key != backend_) throw std::runtime_error("Unknown language backend");
    std::lock_guard lock(mutex_);
    return routingEpoch_ * 0x100000000ULL + epoch_;
}
std::string LanguageService::backend(const std::string &path) const {
    std::lock_guard route(routingMutex_);
    if (!definition_.is_null())
        return contributedLanguageId(definition_, path).empty() ? std::string{} : backend_;
    const auto builtin = languageBackend(path);
    if (!builtin.empty())
        return builtin;
    for (const auto &d : extensionDefinitions_)
        if (!contributedLanguageId(d, path).empty())
            return d.at("id");
    return {};
}
bool LanguageService::hasBackend(const std::string &key) const {
    std::lock_guard route(routingMutex_);
    return key == backend_ || children_.contains(key);
}
void LanguageService::replaceExtensions(const Json &definitions) {
    std::lock_guard route(routingMutex_);
    if (definitions == extensionDefinitions_)
        return;
    if (!definitions.is_array() || definitions.size() > 32)
        throw std::runtime_error("At most 32 plugin language servers are supported");
    std::set<std::string> ids, selectors;
    for (const auto &d : definitions) {
        validateLanguageServer(d);
        const auto id = d.at("id").get<std::string>();
        if (!ids.insert(id).second ||
            std::find(languageBackends().begin(), languageBackends().end(), id) != languageBackends().end())
            throw std::runtime_error("Duplicate language backend");
        for (const auto &l : d.at("languages"))
            for (const auto *kind : {"extensions", "filenames"})
                for (const auto &v : l.value(kind, Json::array())) {
                    auto selector = std::string(kind) + ":" + v.get<std::string>();
                    std::transform(selector.begin(), selector.end(), selector.begin(),
                                   [](unsigned char c) { return char(std::tolower(c)); });
                    if (!selectors.insert(selector).second)
                        throw std::runtime_error("Plugin language selectors overlap");
                }
    }
    // Build replacements before publication. Existing owners keep their state when unchanged.
    std::map<std::string, std::unique_ptr<LanguageService>> next;
    for (const auto &d : definitions) {
        const auto id = d.at("id").get<std::string>();
        if (!children_.contains(id) || children_.at(id)->definition_ != d)
            next[id] = std::unique_ptr<LanguageService>(new LanguageService(root_, id, d));
    }
    for (auto it = children_.begin(); it != children_.end();) {
        if (!it->second->definition_.is_null() && (!ids.contains(it->first) || next.contains(it->first)))
            it = children_.erase(it);
        else
            ++it;
    }
    children_.merge(next);
    extensionDefinitions_ = definitions;
    ++routingEpoch_;
}
void LanguageService::run() {
    SemanticTokens semanticTokens;
    size_t semanticRefresh = 0;
    LanguageClient client;
    std::map<std::string, Document> documents;
    std::map<std::string, std::pair<std::uint64_t, int>> sent;
    std::map<std::string, uint64_t> sentIdentities;
    int nextVersion = 0; // Connection-wide: reopening a path never reuses an LSP version.
    std::vector<std::shared_ptr<Query>> pending;
    bool attempted = false, enabled = backend_ == "cpp", prewarm = false;
    std::string executable = managed_ ? managedExecutable() : defaultExecutable(), failure;
    const auto defaults = defaultServerLaunch(root_, backend_);
    auto argv = defaults.argv;
    auto settings = defaults.settings;
    auto environment = defaults.environment;
    std::filesystem::path compilationDatabase, project;
    size_t diagnosticsGeneration = 0;
    auto fail = [](const std::shared_ptr<Query> &q, const std::string &message) {
        if (q->done)
            return;
        q->done = true;
        q->promise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
    };
    for (;;) {
        bool reset = false, stop = false;
        std::map<std::string, std::optional<Document>> updates;
        {
            std::unique_lock lock(mutex_);
            auto changed = [this] {
                return stopping_ || reconfigure_ || !updates_.empty() || !queries_.empty();
            };
            if (client.running() || !pending.empty())
                wake_.wait_for(lock, std::chrono::milliseconds(8), changed);
            else
                wake_.wait(lock, changed);
            stop = stopping_;
            reset = reconfigure_;
            reconfigure_ = false;
            if (reset) {
                executable = executable_;
                enabled = enabled_;
                prewarm = prewarm_;
                compilationDatabase = compilationDatabase_;
                project = project_;
                argv = argv_;
                settings = settings_;
                environment = environment_;
            }
            updates.swap(updates_);
            while (!queries_.empty()) {
                pending.push_back(std::move(queries_.front()));
                queries_.pop_front();
            }
        }
        if (reset || stop) {
            client.stop();
            semanticTokens.clear();
            sent.clear();
            sentIdentities.clear();
            nextVersion = 0;
            attempted = false;
            failure.clear();
            for (auto &q : pending)
                if (stop || q->id)
                    fail(q, "Language service restarted or stopped");
        }
        if (stop)
            break;
        for (auto &[path, doc] : updates) {
            if (doc)
                documents.insert_or_assign(path, std::move(*doc));
            else
                documents.erase(path);
        }
        if (!attempted && enabled && ((!documents.empty() && prewarm) || !pending.empty())) {
            attempted = true;
            if (!argv.empty()) {
                if (backend_ == "cpp" && !compilationDatabase.empty())
                    settings["compilationDatabasePath"] = pathUtf8(compilationDatabase);
                if (!client.startServer(root_, argv, settings, managed_, environment))
                    failure = client.error();
            } else if (managed_) {
                std::vector<std::string> argv{executable};
                if (utf8Path(executable).extension() == ".dll") {
                    const auto dotnet = managedHost();
                    if (dotnet.empty() || !std::filesystem::is_regular_file(utf8Path(executable))) {
                        failure = "Repair the managed C# language runtime (app/scripts/setup_csharp.py)";
                        argv.clear();
                    } else
                        argv.insert(argv.begin(), dotnet);
                }
                if (!argv.empty()) {
                    argv.insert(argv.end(), {"--loglevel", "error"});
                    auto configuration = settings;
                    if (!configuration.contains("csharp"))
                        configuration["csharp"] = Json::object();
                    if (configuration.at("csharp").is_object() && !configuration.at("csharp").contains("applyFormattingOptions"))
                        configuration["csharp"]["applyFormattingOptions"] = true;
                    if (!project.empty()) {
                        argv.insert(argv.end(), {"--solution", pathUtf8(project)});
                        if (configuration.at("csharp").is_object())
                            configuration["csharp"]["solutionPathOverride"] = pathUtf8(project);
                    }
                    if (!client.startServer(root_, argv, configuration, true))
                        failure = client.error();
                }
            } else if (backend_ == "cpp") {
                if (!client.start(root_, executable, compilationDatabase, settings))
                    failure = client.error();
            } else if (argv.empty())
                failure = "Install or configure the " + languageServerName(backend_) + " language server";
            else if (!client.startServer(root_, argv, settings))
                failure = client.error();
        }
        client.poll();
        if (attempted && !client.running() && failure.empty()) {
            failure = client.error();
            if (failure.empty()) failure = "Language server stopped; install or configure the matching backend and restart it in Problems";
            const auto logs = client.logs();
            if (!logs.empty()) failure += "\n" + processOutputText(std::string_view(logs).substr(logs.size() > 4096 ? logs.size() - 4096 : 0), true);
        }
        if (semanticRefresh != client.semanticRefreshGeneration()) {
            semanticTokens.clear();
            semanticRefresh = client.semanticRefreshGeneration();
        }
        bool synchronized = client.ready();
        // Roslyn starts lazy workspace loading from didOpen. Its request scheduler
        // preserves notification order; queries wait for the load progress to end.
        if (client.initialized()) {
            for (auto it = sent.begin(); it != sent.end();) {
                if (documents.contains(it->first) && documents.at(it->first).identity == sentIdentities.at(it->first)) {
                    ++it;
                    continue;
                }
                if (!client.notify(
                        "textDocument/didClose",
                        {{"textDocument", {{"uri", LanguageClient::uri(root_ / utf8Path(it->first))}}}})) {
                    synchronized = false;
                    ++it;
                    continue;
                }
                client.invalidateSemantic();
                semanticTokens.erase(LanguageClient::uriKey(LanguageClient::uri(root_ / utf8Path(it->first))));
                sentIdentities.erase(it->first);
                it = sent.erase(it);
            }
            for (const auto &[path, doc] : documents) {
                auto uri = LanguageClient::uri(root_ / utf8Path(path));
                auto entry = sent.find(path);
                if (entry != sent.end() && sentIdentities.at(path) != doc.identity) {
                    synchronized = false; // didClose has not been accepted yet.
                    continue;
                }
                if ((entry == sent.end() || entry->second.first != doc.revision) && nextVersion == INT_MAX) {
                    synchronized = false;
                    failure = "Language document versions exhausted; restart this language server";
                    continue;
                }
                if (entry == sent.end()) {
                    if (client.notify("textDocument/didOpen",
                                      {{"textDocument",
                                        {{"uri", uri},
                                         {"languageId", definition_.is_null()
                                                            ? languageId(path)
                                                            : contributedLanguageId(definition_, path)},
                                         {"version", nextVersion + 1},
                                         {"text", doc.text}}}})) {
                        sent[path] = {doc.revision, ++nextVersion};
                        sentIdentities[path] = doc.identity;
                        client.invalidateSemantic();
                        semanticTokens.erase(LanguageClient::uriKey(uri));
                    } else
                        synchronized = false;
                } else if (entry->second.first != doc.revision) {
                    if (client.notify(
                            "textDocument/didChange",
                            {{"textDocument", {{"uri", uri}, {"version", nextVersion + 1}}},
                             {"contentChanges", Json::array({{{"text", doc.text}}})}})) {
                        entry->second.first = doc.revision;
                        entry->second.second = ++nextVersion;
                        client.invalidateSemantic();
                    } else
                        synchronized = false;
                }
            }
        }
        auto now = std::chrono::steady_clock::now();
        for (auto &q : pending) {
            if (q->done)
                continue;
            if (q->cancelled && q->cancelled->load()) {
                if (q->id && q->method == "cycode/javaBuildWorkspace") {
                    // Retain the callback/reservation until JDT acknowledges or
                    // the original deadline expires. Its compiler may be busy.
                    if (!q->cancelSent) client.notify("$/cancelRequest", {{"id",q->id}});
                    q->cancelSent = true;
                } else {
                    if (q->id) client.cancel(q->id);
                    fail(q, "Language request cancelled");
                    continue;
                }
            }
            if ((!enabled || (attempted && !client.running())) || now >= q->deadline) {
                if (q->id)
                    client.cancel(q->id);
                fail(q, now >= q->deadline ? "Language request timed out"
                        : failure.empty()  ? "Language server stopped; restart it in Problems"
                                           : failure);
            } else if (synchronized && !q->id) {
                if ((q->method == "cycode/javaDebugSession" || q->method == "cycode/javaBuildWorkspace") &&
                    (backend_ != "java" || !javaDebugAvailable(client.capabilities()))) {
                    fail(q, "Java debugging requires JDT LS with the Microsoft java-debug bundle in initializationOptions.bundles; configure it in language settings and restart Java");
                    continue;
                }
                // Respect the server's advertised surface. These operations are
                // shared by the UI and MCP, including servers without rename or
                // workspace search (for example HTML).
                static const std::map<std::string, std::string> capabilities = {
                    {"textDocument/completion", "completionProvider"},
                    {"textDocument/definition", "definitionProvider"},
                    {"textDocument/declaration", "declarationProvider"},
                    {"textDocument/typeDefinition", "typeDefinitionProvider"},
                    {"textDocument/implementation", "implementationProvider"},
                    {"textDocument/documentHighlight", "documentHighlightProvider"},
                    {"textDocument/hover", "hoverProvider"},
                    {"textDocument/signatureHelp", "signatureHelpProvider"},
                    {"textDocument/references", "referencesProvider"},
                    {"workspace/symbol", "workspaceSymbolProvider"},
                    {"textDocument/documentSymbol", "documentSymbolProvider"},
                    {"textDocument/rename", "renameProvider"},
                    {"textDocument/formatting", "documentFormattingProvider"},
                    {"textDocument/rangeFormatting", "documentRangeFormattingProvider"},
                    {"textDocument/codeAction", "codeActionProvider"}};
                const bool hierarchy = q->method == "textDocument/prepareCallHierarchy" ||
                                       q->method == "textDocument/prepareTypeHierarchy" ||
                                       q->method.starts_with("callHierarchy/") ||
                                       q->method.starts_with("typeHierarchy/");
                if (hierarchy) {
                    const auto name = q->method.find("CallHierarchy") != std::string::npos ||
                                      q->method.starts_with("callHierarchy/")
                                          ? "callHierarchyProvider" : "typeHierarchyProvider";
                    const auto value = client.capabilities().value(name, Json());
                    if (value != true && !value.is_object()) {
                        fail(q, languageServerName(backend_) + " does not support " + q->method);
                        continue;
                    }
                }
                if (((backend_ != "cpp" && !managed_) || q->method == "textDocument/hover" ||
                     q->method == "textDocument/signatureHelp" || q->method == "textDocument/codeAction" ||
                     q->method == "textDocument/declaration" || q->method == "textDocument/typeDefinition" ||
                     q->method == "textDocument/implementation" || q->method == "textDocument/documentHighlight") &&
                    capabilities.contains(q->method)) {
                    const auto value = client.capabilities().value(capabilities.at(q->method), Json());
                    if (value != true && !value.is_object()) {
                        fail(q, languageServerName(backend_) + " does not support " + q->method);
                        continue;
                    }
                }
                const bool highlight = q->method == "cycode/semanticTokens";
                const auto completionProvider = client.capabilities().value("completionProvider", Json());
                if (q->method == "completionItem/resolve" &&
                    (!completionProvider.is_object() ||
                     !completionProvider.value("resolveProvider", false))) {
                    fail(q, languageServerName(backend_) + " does not support completionItem/resolve");
                    continue;
                }
                const auto symbolProvider = client.capabilities().value("workspaceSymbolProvider", Json());
                if (q->method == "workspaceSymbol/resolve" &&
                    (!symbolProvider.is_object() || !symbolProvider.value("resolveProvider", false))) {
                    fail(q, languageServerName(backend_) + " does not support workspaceSymbol/resolve");
                    continue;
                }
                const auto actionProvider = client.capabilities().value("codeActionProvider", Json());
                if (q->method == "codeAction/resolve" &&
                    (!actionProvider.is_object() || !actionProvider.value("resolveProvider", false))) {
                    fail(q, languageServerName(backend_) + " does not support codeAction/resolve");
                    continue;
                }
                if (q->method == "textDocument/codeAction") {
                    try {
                        auto diagnostics = Json::array();
                        size_t bytes = 0;
                        const auto uri = q->params.at("textDocument").at("uri").get<std::string>();
                        for (const auto &[path, version] : sent)
                            if (LanguageClient::uri(root_ / utf8Path(path)) == uri) {
                                const auto &text = documents.at(path).text;
                                const auto a = utf16Offset(text, q->params.at("range").at("start")),
                                           b = utf16Offset(text, q->params.at("range").at("end"));
                                for (const auto &[diagnosticUri, entries] : client.diagnostics().items()) {
                                    bool matches = false;
                                    try {
                                        matches = LanguageClient::uriKey(diagnosticUri) ==
                                                  LanguageClient::uriKey(uri);
                                    } catch (const std::exception &) {
                                    }
                                    if (!matches || !client.diagnosticVersions().contains(diagnosticUri))
                                        continue;
                                    const auto &diagnosticVersion =
                                        client.diagnosticVersions().at(diagnosticUri);
                                    if (!diagnosticVersion.is_null() && diagnosticVersion != version.second)
                                        continue;
                                    for (const auto &entry : entries) {
                                        try {
                                            const auto start =
                                                           utf16Offset(text, entry.at("range").at("start")),
                                                       end = utf16Offset(text, entry.at("range").at("end"));
                                            if (end < start ||
                                                (a == b ? a < start || a > end : end <= a || b <= start))
                                                continue;
                                            const auto size = entry.dump().size();
                                            if (diagnostics.size() >= 100 || bytes + size > 256 * 1024)
                                                break;
                                            diagnostics.push_back(entry);
                                            bytes += size;
                                        } catch (const std::exception &) { /* Invalid server diagnostics
                                                                              cannot become fix context. */
                                        }
                                    }
                                }
                            }
                        q->params["context"]["diagnostics"] = std::move(diagnostics);
                    } catch (const std::exception &e) {
                        fail(q, e.what());
                        continue;
                    }
                }
                const auto provider = client.capabilities().value("semanticTokensProvider", Json::object());
                const auto generation = client.semanticGeneration();
                const bool supported = provider.is_object() && provider.contains("full") &&
                                       (provider.at("full").is_object() || provider.at("full") == true);
                if (highlight && !supported) {
                    q->done = true;
                    q->promise.set_value({{"supported", false}, {"generation", generation}});
                    continue;
                }
                const auto legend = highlight ? provider.value("legend", Json::object()) : Json::object();
                SemanticTokens::Snapshot semanticBase;
                std::string semanticUri;
                auto requestParams = q->params;
                if (q->method == "cycode/javaDebugSession")
                    requestParams = {{"command", "vscode.java.startDebugSession"}, {"arguments", Json::array()}};
                if (q->method == "cycode/javaBuildWorkspace") requestParams = javaIncrementalBuildParams();
                if (highlight) {
                    try {
                        semanticUri = LanguageClient::uriKey(q->params.at("textDocument").at("uri"));
                        const auto full = provider.at("full");
                        if (!q->semanticFull && full.is_object() && full.value("delta", Json()) == true)
                            semanticBase = semanticTokens.find(semanticUri);
                        if (semanticBase)
                            requestParams["previousResultId"] = *semanticBase->resultId;
                    } catch (const std::exception &e) {
                        fail(q, e.what());
                        continue;
                    }
                }
                std::map<std::string, std::pair<uint64_t, int>> versions;
                auto uriKey = LanguageClient::uriKey;
                if (q->method == "textDocument/rename" || q->method == "textDocument/codeAction" ||
                    q->method == "codeAction/resolve")
                    for (const auto &[path, version] : sent)
                        versions[uriKey(LanguageClient::uri(root_ / utf8Path(path)))] = version;
                q->id = client.request(
                    highlight ? (semanticBase ? "textDocument/semanticTokens/full/delta"
                                               : "textDocument/semanticTokens/full") : q->method == "cycode/javaDebugSession" ? "workspace/executeCommand" : q->method == "cycode/javaBuildWorkspace" ? "java/buildWorkspace" : q->method,
                    std::move(requestParams),
                    [q, highlight, legend, generation, versions = std::move(versions),
                     uriKey, semanticBase, semanticUri, &semanticTokens, &client](const Json &result) {
                        if (q->done)
                            return;
                        if (highlight) {
                            try {
                                if (generation != client.semanticGeneration())
                                    throw std::runtime_error("RevisionConflict: semantic context changed");
                                if (result.is_object() && result.contains("error"))
                                    throw std::runtime_error("Language server: " + result.at("error").dump());
                                const auto data = SemanticTokens::decode(result, semanticBase);
                                semanticTokens.remember(semanticUri, data);
                                q->promise.set_value({{"supported", true}, {"legend", legend},
                                                      {"result", data->result()}, {"generation", generation}});
                                q->done = true;
                            } catch (...) {
                                semanticTokens.erase(semanticUri);
                                if (semanticBase && generation == client.semanticGeneration()) {
                                    // Retry one full request within the original query deadline.
                                    q->semanticFull = true;
                                    q->id = 0;
                                } else {
                                    q->done = true;
                                    q->promise.set_exception(std::current_exception());
                                }
                            }
                            return;
                        }
                        q->done = true;
                        if (result.is_object() && result.contains("error"))
                            q->promise.set_exception(std::make_exception_ptr(
                                std::runtime_error("Language server: " + result["error"].dump())));
                        else {
                            try {
                                auto response = result;
                                auto mapVersions = [&](Json &edit) {
                                    if (!edit.is_object() || !edit.contains("documentChanges"))
                                        return;
                                    for (auto &change : edit.at("documentChanges"))
                                        if (change.contains("textDocument")) {
                                            auto &document = change.at("textDocument");
                                            if (!document.value("version", Json()).is_null()) {
                                                const auto found = versions.find(uriKey(document.at("uri")));
                                                if (found == versions.end() ||
                                                    document.at("version") != found->second.second)
                                                    throw std::runtime_error(
                                                        "RevisionConflict: language server workspace edit "
                                                        "has an obsolete document version");
                                                document["cycodeRevision"] = found->second.first;
                                                document.erase("version");
                                            }
                                        }
                                };
                                if (q->method == "textDocument/rename")
                                    mapVersions(response);
                                if (q->method == "textDocument/codeAction" && response.is_array())
                                    for (auto &action : response)
                                        if (action.is_object() && action.contains("edit"))
                                            mapVersions(action.at("edit"));
                                if (q->method == "codeAction/resolve" && response.is_object() &&
                                    response.contains("edit"))
                                    mapVersions(response.at("edit"));
                                q->promise.set_value(std::move(response));
                            } catch (...) {
                                q->promise.set_exception(std::current_exception());
                            }
                        }
                    });
            }
        }
        auto completed = std::erase_if(pending, [](const auto &q) { return q->done; });
        {
            std::lock_guard lock(mutex_);
            outstanding_ -= completed;
            status_["ready"] = client.ready();
            status_["running"] = client.running();
            const auto provider = client.capabilities().value("semanticTokensProvider", Json::object());
            status_["semanticTokens"] = provider.is_object() && provider.contains("full") &&
                                        (provider.at("full").is_object() || provider.at("full") == true);
            status_["semanticGeneration"] = client.semanticGeneration();
            status_["error"] = failure.empty() ? client.error() : failure;
            status_["logs"] = client.logs();
            Json methods = Json::object();
            for (const auto &[name, capability] :
                 std::map<std::string, std::string>{{"completion", "completionProvider"},
                                                    {"hover", "hoverProvider"},
                                                    {"signature", "signatureHelpProvider"},
                                                    {"definition", "definitionProvider"},
                                                    {"declaration", "declarationProvider"},
                                                    {"typeDefinition", "typeDefinitionProvider"},
                                                    {"implementation", "implementationProvider"},
                                                    {"documentHighlights", "documentHighlightProvider"},
                                                    {"references", "referencesProvider"},
                                                    {"callHierarchy", "callHierarchyProvider"},
                                                    {"typeHierarchy", "typeHierarchyProvider"},
                                                    {"symbols", "workspaceSymbolProvider"},
                                                    {"documentSymbols", "documentSymbolProvider"},
                                                    {"rename", "renameProvider"},
                                                    {"format", "documentFormattingProvider"},
                                                    {"rangeFormat", "documentRangeFormattingProvider"}}) {
                const auto value = client.capabilities().value(capability, Json());
                methods[name] = value == true || value.is_object();
            }
            status_["methods"] = std::move(methods);
            const auto completionProvider = client.capabilities().value("completionProvider", Json());
            status_["methods"]["completionResolve"] =
                completionProvider.is_object() && completionProvider.value("resolveProvider", false);
            const auto symbolProvider = client.capabilities().value("workspaceSymbolProvider", Json());
            status_["methods"]["symbolResolve"] =
                symbolProvider.is_object() && symbolProvider.value("resolveProvider", false);
            const auto actionProvider = client.capabilities().value("codeActionProvider", Json());
            status_["methods"]["codeActions"] = actionProvider == true || actionProvider.is_object();
            status_["methods"]["codeActionResolve"] =
                actionProvider.is_object() && actionProvider.value("resolveProvider", false);
            if (reset || diagnosticsGeneration != client.diagnosticGeneration()) {
                diagnosticsGeneration = client.diagnosticGeneration();
                status_["diagnostics"] = client.diagnostics();
            }
            status_["diagnosticGeneration"]=diagnosticsGeneration;
            // Only an explicitly versioned publication can prove editor ranges
            // belong to this exact native revision. Keep advisory/unversioned
            // entries in Problems without painting stale positions onto code.
            Json eligible=Json::object();
            if(enabled && client.ready()) {
                std::map<std::string,std::string> published;
                for(const auto &[uri,version]:client.diagnosticVersions().items())
                    if(version.is_number_integer())try {published[LanguageClient::uriKey(uri)]=uri;}catch(const std::exception &){}
                for(const auto &[path,version]:sent) {
                    const auto document=documents.find(path);
                    if(document==documents.end()||document->second.revision!=version.first||document->second.identity!=sentIdentities.at(path))continue;
                    const auto publication=published.find(LanguageClient::uriKey(LanguageClient::uri(root_/utf8Path(path))));
                    if(publication!=published.end()&&client.diagnosticVersions().at(publication->second)==version.second)
                        eligible[path]={{"revision",version.first},{"id",document->second.identity},{"uri",publication->second}};
                }
            }
            status_["diagnosticRevisions"]=std::move(eligible);
        }
    }
}
} // namespace cycode

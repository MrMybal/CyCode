#include "cycode/LanguageService.h"
#include "cycode/Tools.h"
#include "cycode/TextEdits.h"
#include <algorithm>

namespace cycode {
bool LanguageService::supports(const std::string &path) { return !languageBackend(path).empty(); }
bool LanguageService::supportsManaged(const std::string &path) {
    auto extension = pathUtf8(utf8Path(path).extension());
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c) { return char(std::tolower(c)); });
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
    if (auto resolved = toolPath("clangd"); !resolved.empty()) return resolved;
#ifdef _WIN32
    auto installed = std::filesystem::path("C:/Program Files/LLVM/bin/clangd.exe");
    if (std::filesystem::exists(installed))
        return pathUtf8(installed);
#endif
    return "clangd";
}
std::string LanguageService::managedExecutable() {
    if (auto resolved = toolPath("csharp_ls"); !resolved.empty()) return resolved;
    return pathUtf8(installationRoot()/"runtime/tools/csharp-ls/tools/net10.0/any/CSharpLanguageServer.dll");
}
std::string LanguageService::managedHost() {
    if (auto override = toolPath("csharp_dotnet"); !override.empty()) return override;
    auto bundled = installationRoot()/"runtime/tools/csharp-dotnet";
#ifdef _WIN32
    bundled /= "dotnet.exe";
#else
    bundled /= "dotnet";
#endif
    if (std::filesystem::is_regular_file(bundled)) return pathUtf8(bundled);
    return toolPath("dotnet");
}
LanguageService::LanguageService(std::filesystem::path root, bool managed)
    : LanguageService(std::move(root),std::string(managed ? "csharp" : "cpp")) {
    if (!managed) for (const auto &key : languageBackends()) if (key!="cpp") children_[key]=std::unique_ptr<LanguageService>(new LanguageService(root_,key));
}
LanguageService::LanguageService(std::filesystem::path root,std::string backend)
    : root_(std::move(root)), managed_(backend=="csharp"), backend_(std::move(backend)), enabled_(backend_=="cpp"),
      executable_(managed_ ? managedExecutable() : defaultExecutable()) {
    const auto launch=defaultServerLaunch(root_,backend_); argv_=launch.argv; settings_=launch.settings; environment_=launch.environment;
    worker_=std::thread([this] { run(); });
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
    if (utf8Path(executable).is_relative()) if (auto resolved = toolPath(executable); !resolved.empty()) executable = resolved;
    {
        std::lock_guard lock(mutex_);
        executable_ = std::move(executable);
        argv_.clear(); settings_=Json::object(); environment_=Json::object();
        enabled_ = enabled;
        prewarm_ = prewarm;
        reconfigure_ = true;
        ++epoch_;
    }
    wake_.notify_one();
    if (children_.contains("csharp")) configureManaged(managedExecutable(), enabled, prewarm);
    if (!enabled) for (const auto &[key,child] : children_) if (key!="csharp") child->configureBackend(key,{},Json::object(),false,false);
}
void LanguageService::configureManaged(std::string executable, bool enabled, bool prewarm) {
    if (children_.contains("csharp")) children_.at("csharp")->configure(executable.empty() ? managedExecutable() : std::move(executable),enabled,prewarm);
    else if (managed_) configure(executable.empty() ? managedExecutable() : std::move(executable),enabled,prewarm);
}
void LanguageService::configureBackend(const std::string &backend,const std::vector<std::string> &argv,
                                      const Json &settings,bool enabled,bool prewarm) {
    if (children_.contains(backend)) { children_.at(backend)->configureBackend(backend,argv,settings,enabled,prewarm); return; }
    if (backend!=backend_) throw std::runtime_error("Unknown language backend");
    if (argv.size()>128 || !settings.is_object() || settings.dump().size()>65536) throw std::runtime_error("Invalid language server configuration");
    if (settings.contains("$workspaceReadyNotification") && (!settings.at("$workspaceReadyNotification").is_string() ||
        (settings.at("$workspaceReadyNotification")!="" && settings.at("$workspaceReadyNotification")!="experimental/serverStatus" && settings.at("$workspaceReadyNotification")!="language/status")))
        throw std::runtime_error("Unsupported language server readiness notification");
    for (const auto &arg : argv) if (arg.size()>8192 || arg.find('\0')!=std::string::npos) throw std::runtime_error("Invalid language server argument");
    const auto defaults=defaultServerLaunch(root_,backend_);
    if (enabled && argv.empty() && backend_=="java" && !defaults.argv.empty()) {
#ifdef _WIN32
        const auto config="config_win";
#elif defined(__APPLE__)
        const auto config="config_mac";
#else
        const auto config="config_linux";
#endif
        const auto option=std::find(defaults.argv.begin(),defaults.argv.end(),"-configuration");
        const auto directory=utf8Path(*std::next(option));
        std::filesystem::create_directories(directory);
        // Copy the pristine settings, never another instance's OSGi caches/locks.
        std::filesystem::copy_file(installationRoot()/"runtime/tools/jdtls"/config/"config.ini",directory/"config.ini",
            std::filesystem::copy_options::overwrite_existing);
    }
    {
        std::lock_guard lock(mutex_);
        argv_=argv.empty() ? defaults.argv : argv; settings_=settings.empty() && argv.empty() ? defaults.settings : settings;
        environment_=argv.empty() ? defaults.environment : Json::object();
        if (!argv.empty()) executable_=argv.front();
        enabled_=enabled; prewarm_=prewarm; reconfigure_=true; ++epoch_;
    }
    wake_.notify_one();
}
void LanguageService::setProject(const std::filesystem::path &entry) {
    if (children_.contains("csharp")) { children_.at("csharp")->setProject(entry); return; }
    std::lock_guard lock(mutex_);
    project_ = entry.extension() == ".sln" ? entry : std::filesystem::path{};
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
void LanguageService::sync(const std::string &path, const std::string &text, std::uint64_t revision) {
    const auto backend=languageBackend(path);
    if (children_.contains(backend)) { children_.at(backend)->sync(path,text,revision); return; }
    if (backend!=backend_)
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = Document{text, revision};
    }
    wake_.notify_one();
}
void LanguageService::close(const std::string &path) {
    const auto backend=languageBackend(path);
    if (children_.contains(backend)) { children_.at(backend)->close(path); return; }
    if (backend!=backend_)
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = std::nullopt;
    }
    wake_.notify_one();
}
std::future<Json> LanguageService::request(std::string method, Json params) {
    const auto uri = params.value("textDocument",Json::object()).value("uri","");
    auto backend = params.value("cycodeLanguage",languageBackend(uri)); params.erase("cycodeLanguage");
    if (backend.empty()) backend=backend_;
    if (children_.contains(backend)) return children_.at(backend)->request(std::move(method),std::move(params));
    if (backend!=backend_) throw std::runtime_error("Unknown language backend");
    auto query = std::make_shared<Query>();
    query->method = std::move(method);
    query->params = std::move(params);
    query->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(backend_=="java" ? 120 : managed_ || backend_=="rust" ? 60 : 15);
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
    std::lock_guard lock(mutex_);
    auto result = status_;
    result["name"] = languageServerName(backend_);
    result["enabled"] = enabled_;
    result["available"] = backend_=="cpp" || managed_ || !argv_.empty();
    if (!children_.empty()) {
        result["servers"]={{"cpp",result}};
        for (const auto &[key,child] : children_) {
            const auto server=child->status(); result["servers"][key]=server;
            result["ready"]=result.value("ready",false) || server.value("ready",false);
            result["running"]=result.value("running",false) || server.value("running",false);
            for (const auto &[uri,entries] : server.at("diagnostics").items()) result["diagnostics"][uri]=entries;
            if (server.value("enabled",false) && !server.value("error","").empty()) result["error"]=result.value("error","")+" "+key+": "+server.at("error").get<std::string>();
        }
    }
    return result;
}
uint64_t LanguageService::epoch(const std::string &path) const {
    const auto backend=languageBackend(path);
    if (children_.contains(backend)) return children_.at(backend)->epoch(path);
    std::lock_guard lock(mutex_);return epoch_;
}
void LanguageService::run() {
    LanguageClient client;
    std::map<std::string, Document> documents;
    std::map<std::string, std::pair<std::uint64_t, int>> sent;
    std::vector<std::shared_ptr<Query>> pending;
    bool attempted = false, enabled = backend_=="cpp", prewarm = false;
    std::string executable = managed_ ? managedExecutable() : defaultExecutable(), failure;
    const auto defaults=defaultServerLaunch(root_,backend_);
    auto argv=defaults.argv; auto settings=defaults.settings; auto environment=defaults.environment;
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
            auto changed=[this] {
                return stopping_ || reconfigure_ || !updates_.empty() || !queries_.empty();
            };
            if (client.running() || !pending.empty()) wake_.wait_for(lock,std::chrono::milliseconds(8),changed);
            else wake_.wait(lock,changed);
            stop = stopping_;
            reset = reconfigure_;
            reconfigure_ = false;
            if (reset) {
                executable = executable_;
                enabled = enabled_;
                prewarm = prewarm_;
                compilationDatabase = compilationDatabase_;
                project = project_;
                argv=argv_; settings=settings_; environment=environment_;
            }
            updates.swap(updates_);
            while (!queries_.empty()) {
                pending.push_back(std::move(queries_.front()));
                queries_.pop_front();
            }
        }
        if (reset || stop) {
            client.stop();
            sent.clear();
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
                if (!client.startServer(root_,argv,settings,managed_,environment)) failure=client.error();
            } else if (managed_) {
                std::vector<std::string> argv{executable};
                if (utf8Path(executable).extension() == ".dll") {
                    const auto dotnet = managedHost();
                    if (dotnet.empty() || !std::filesystem::is_regular_file(utf8Path(executable))) {
                        failure = "Repair the managed C# language runtime (app/scripts/setup_csharp.py)";
                        argv.clear();
                    } else argv.insert(argv.begin(),dotnet);
                }
                if (!argv.empty()) {
                    argv.insert(argv.end(),{"--loglevel","error"});
                    Json settings = {{"csharp",{{"applyFormattingOptions",true}}}};
                    if (!project.empty()) { argv.insert(argv.end(),{"--solution",pathUtf8(project)}); settings["csharp"]["solutionPathOverride"] = pathUtf8(project); }
                    if (!client.startServer(root_,argv,settings,true)) failure = client.error();
                }
            } else if (backend_=="cpp") { if (!client.start(root_, executable, compilationDatabase)) failure=client.error(); }
            else if (argv.empty()) failure="Install or configure the "+languageServerName(backend_)+" language server";
            else if (!client.startServer(root_,argv,settings)) failure=client.error();
        }
        client.poll();
        bool synchronized = client.ready();
        // Roslyn starts lazy workspace loading from didOpen. Its request scheduler
        // preserves notification order; queries wait for the load progress to end.
        if (client.initialized()) {
            for (auto it = sent.begin(); it != sent.end();) {
                if (documents.contains(it->first)) {
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
                client.invalidateSemantic(); it = sent.erase(it);
            }
            for (const auto &[path, doc] : documents) {
                auto uri = LanguageClient::uri(root_ / utf8Path(path));
                auto entry = sent.find(path);
                if (entry == sent.end()) {
                    if (client.notify("textDocument/didOpen",
                                      {{"textDocument",
                                        {{"uri", uri},
                                         {"languageId", languageId(path)},
                                         {"version", 1},
                                         {"text", doc.text}}}}))
                        { sent[path] = {doc.revision, 1}; client.invalidateSemantic(); }
                    else
                        synchronized = false;
                } else if (entry->second.first != doc.revision) {
                    if (client.notify(
                            "textDocument/didChange",
                            {{"textDocument", {{"uri", uri}, {"version", entry->second.second + 1}}},
                             {"contentChanges", Json::array({{{"text", doc.text}}})}})) {
                        entry->second.first = doc.revision;
                        ++entry->second.second;
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
            if ((!enabled || (attempted && !client.running())) || now >= q->deadline) {
                if (q->id)
                    client.cancel(q->id);
                fail(q, now >= q->deadline ? "Language request timed out"
                        : failure.empty()  ? "Language server stopped; restart it in Problems"
                                           : failure);
            } else if (synchronized && !q->id) {
                // Respect the server's advertised surface. These operations are
                // shared by the UI and MCP, including servers without rename or
                // workspace search (for example HTML).
                static const std::map<std::string,std::string> capabilities={
                    {"textDocument/completion","completionProvider"},{"textDocument/definition","definitionProvider"},
                    {"textDocument/hover","hoverProvider"},{"textDocument/signatureHelp","signatureHelpProvider"},
                    {"textDocument/references","referencesProvider"},{"workspace/symbol","workspaceSymbolProvider"},
                    {"textDocument/documentSymbol","documentSymbolProvider"},{"textDocument/rename","renameProvider"},
                    {"textDocument/formatting","documentFormattingProvider"},{"textDocument/rangeFormatting","documentRangeFormattingProvider"},{"textDocument/codeAction","codeActionProvider"}};
                if (((backend_!="cpp" && !managed_) || q->method=="textDocument/hover" || q->method=="textDocument/signatureHelp" || q->method=="textDocument/codeAction") && capabilities.contains(q->method)) {
                    const auto value=client.capabilities().value(capabilities.at(q->method),Json());
                    if (value!=true && !value.is_object()) { fail(q,languageServerName(backend_)+" does not support "+q->method); continue; }
                }
                const bool highlight = q->method == "cycode/semanticTokens";
                const auto completionProvider=client.capabilities().value("completionProvider",Json());
                if (q->method=="completionItem/resolve" && (!completionProvider.is_object() || !completionProvider.value("resolveProvider",false))) {
                    fail(q,languageServerName(backend_)+" does not support completionItem/resolve");continue;
                }
                const auto actionProvider=client.capabilities().value("codeActionProvider",Json());
                if (q->method=="codeAction/resolve" && (!actionProvider.is_object() || !actionProvider.value("resolveProvider",false))) { fail(q,languageServerName(backend_)+" does not support codeAction/resolve");continue; }
                if (q->method=="textDocument/codeAction") {
                    try {
                    auto diagnostics=Json::array();size_t bytes=0;const auto uri=q->params.at("textDocument").at("uri").get<std::string>();
                    for (const auto &[path,version] : sent) if (LanguageClient::uri(root_/utf8Path(path))==uri) {
                        const auto &text=documents.at(path).text;const auto a=utf16Offset(text,q->params.at("range").at("start")),b=utf16Offset(text,q->params.at("range").at("end"));
                        for (const auto &[diagnosticUri,entries] : client.diagnostics().items()) {
                        bool matches=false;try { matches=LanguageClient::uriKey(diagnosticUri)==LanguageClient::uriKey(uri); } catch (const std::exception &) {}
                        if (!matches || !client.diagnosticVersions().contains(diagnosticUri)) continue;
                        const auto &diagnosticVersion=client.diagnosticVersions().at(diagnosticUri);
                        if (!diagnosticVersion.is_null() && diagnosticVersion!=version.second) continue;
                        for (const auto &entry : entries) {
                            try {
                                const auto start=utf16Offset(text,entry.at("range").at("start")),end=utf16Offset(text,entry.at("range").at("end"));
                                if (end<start || (a==b ? a<start || a>end : end<=a || b<=start)) continue;
                                const auto size=entry.dump().size();if (diagnostics.size()>=100 || bytes+size>256*1024) break;
                                diagnostics.push_back(entry);bytes+=size;
                            } catch (const std::exception &) { /* Invalid server diagnostics cannot become fix context. */ }
                        }
                        }
                    }
                    q->params["context"]["diagnostics"]=std::move(diagnostics);
                    } catch (const std::exception &e) { fail(q,e.what());continue; }
                }
                const auto provider=client.capabilities().value("semanticTokensProvider",Json::object());
                const auto generation=client.semanticGeneration();
                const bool supported=provider.is_object() && provider.contains("full") && (provider.at("full").is_object() || provider.at("full")==true);
                if (highlight && !supported) { q->done=true; q->promise.set_value({{"supported",false},{"generation",generation}}); continue; }
                const auto legend=highlight ? provider.value("legend",Json::object()) : Json::object();
                std::map<std::string,std::pair<uint64_t,int>> versions;
                auto uriKey=LanguageClient::uriKey;
                if (q->method=="textDocument/rename" || q->method=="textDocument/codeAction" || q->method=="codeAction/resolve") for (const auto &[path,version] : sent) versions[uriKey(LanguageClient::uri(root_/utf8Path(path)))]=version;
                q->id = client.request(highlight ? "textDocument/semanticTokens/full" : q->method, q->params, [q,highlight,legend,generation,versions=std::move(versions),uriKey](const Json &result) {
                    if (q->done)
                        return;
                    q->done = true;
                    if (result.is_object() && result.contains("error"))
                        q->promise.set_exception(std::make_exception_ptr(
                            std::runtime_error("Language server: " + result["error"].dump())));
                    else if (highlight)
                        q->promise.set_value({{"supported",true},{"legend",legend},{"result",result},{"generation",generation}});
                    else {
                        try {
                            auto response=result;
                            auto mapVersions=[&](Json &edit) {
                                if (!edit.is_object() || !edit.contains("documentChanges")) return;
                                for (auto &change : edit.at("documentChanges")) if (change.contains("textDocument")) {
                                    auto &document=change.at("textDocument");
                                    if (!document.value("version",Json()).is_null()) {
                                        const auto found=versions.find(uriKey(document.at("uri")));
                                        if (found==versions.end() || document.at("version")!=found->second.second)
                                            throw std::runtime_error("RevisionConflict: language server workspace edit has an obsolete document version");
                                        document["cycodeRevision"]=found->second.first;
                                        document.erase("version");
                                    }
                                }
                            };
                            if (q->method=="textDocument/rename") mapVersions(response);
                            if (q->method=="textDocument/codeAction" && response.is_array()) for (auto &action : response) if (action.is_object() && action.contains("edit")) mapVersions(action.at("edit"));
                            if (q->method=="codeAction/resolve" && response.is_object() && response.contains("edit")) mapVersions(response.at("edit"));
                            q->promise.set_value(std::move(response));
                        } catch (...) { q->promise.set_exception(std::current_exception()); }
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
            const auto provider=client.capabilities().value("semanticTokensProvider",Json::object());
            status_["semanticTokens"] = provider.is_object() && provider.contains("full") && (provider.at("full").is_object() || provider.at("full")==true);
            status_["semanticGeneration"] = client.semanticGeneration();
            status_["error"] = failure.empty() ? client.error() : failure;
            Json methods=Json::object();
            for (const auto &[name,capability] : std::map<std::string,std::string>{{"completion","completionProvider"},{"hover","hoverProvider"},{"signature","signatureHelpProvider"},{"definition","definitionProvider"},{"references","referencesProvider"},{"symbols","workspaceSymbolProvider"},{"documentSymbols","documentSymbolProvider"},{"rename","renameProvider"},{"format","documentFormattingProvider"},{"rangeFormat","documentRangeFormattingProvider"}}) {
                const auto value=client.capabilities().value(capability,Json()); methods[name]=value==true || value.is_object();
            }
            status_["methods"]=std::move(methods);
            const auto completionProvider=client.capabilities().value("completionProvider",Json());
            status_["methods"]["completionResolve"]=completionProvider.is_object() && completionProvider.value("resolveProvider",false);
            const auto actionProvider=client.capabilities().value("codeActionProvider",Json());
            status_["methods"]["codeActions"]=actionProvider==true || actionProvider.is_object();
            status_["methods"]["codeActionResolve"]=actionProvider.is_object() && actionProvider.value("resolveProvider",false);
            if (reset || diagnosticsGeneration != client.diagnosticGeneration()) {
                diagnosticsGeneration = client.diagnosticGeneration();
                status_["diagnostics"] = client.diagnostics();
            }
        }
    }
}
} // namespace cycode

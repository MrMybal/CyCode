#include "cycode/LanguageService.h"
#include "cycode/Tools.h"
#include <algorithm>

namespace cycode {
bool LanguageService::supports(const std::string &path) { return supportsCpp(path) || supportsManaged(path); }
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
    : root_(std::move(root)), managed_(managed), enabled_(!managed),
      executable_(managed ? managedExecutable() : defaultExecutable()), worker_([this] { run(); }) {
    if (!managed_) managedService_ = std::make_unique<LanguageService>(root_,true);
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
        enabled_ = enabled;
        prewarm_ = prewarm;
        reconfigure_ = true;
    }
    wake_.notify_one();
    if (managedService_) configureManaged(managedExecutable(), enabled, prewarm);
}
void LanguageService::configureManaged(std::string executable, bool enabled, bool prewarm) {
    if (managedService_) managedService_->configure(executable.empty() ? managedExecutable() : std::move(executable),enabled,prewarm);
}
void LanguageService::setProject(const std::filesystem::path &entry) {
    if (managedService_) { managedService_->setProject(entry); return; }
    std::lock_guard lock(mutex_);
    project_ = entry.extension() == ".sln" ? entry : std::filesystem::path{};
    reconfigure_ = true;
}
void LanguageService::setCompilationDatabase(std::filesystem::path directory, bool reload) {
    {
        std::lock_guard lock(mutex_);
        if (!reload && directory == compilationDatabase_)
            return;
        compilationDatabase_ = std::move(directory);
        reconfigure_ = true;
    }
    wake_.notify_one();
}
void LanguageService::sync(const std::string &path, const std::string &text, std::uint64_t revision) {
    if (managedService_ && supportsManaged(path)) { managedService_->sync(path,text,revision); return; }
    if (!(managed_ ? supportsManaged(path) : supportsCpp(path)))
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = Document{text, revision};
    }
    wake_.notify_one();
}
void LanguageService::close(const std::string &path) {
    if (managedService_ && supportsManaged(path)) { managedService_->close(path); return; }
    if (!(managed_ ? supportsManaged(path) : supportsCpp(path)))
        return;
    {
        std::lock_guard lock(mutex_);
        updates_[path] = std::nullopt;
    }
    wake_.notify_one();
}
std::future<Json> LanguageService::request(std::string method, Json params) {
    const auto uri = params.value("textDocument",Json::object()).value("uri","");
    const auto backend = params.value("cycodeLanguage",""); params.erase("cycodeLanguage");
    if (managedService_ && (backend == "csharp" || supportsManaged(uri))) return managedService_->request(std::move(method),std::move(params));
    auto query = std::make_shared<Query>();
    query->method = std::move(method);
    query->params = std::move(params);
    query->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(managed_ ? 60 : 15);
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
    result["name"] = managed_ ? "C# / Roslyn" : "C++ / clangd";
    result["enabled"] = enabled_;
    if (managedService_) {
        auto csharp = managedService_->status();
        result["servers"] = {{"cpp",result},{"csharp",csharp}};
        result["ready"] = result.value("ready",false) || csharp.value("ready",false);
        result["running"] = result.value("running",false) || csharp.value("running",false);
        for (const auto &[uri,entries] : csharp.at("diagnostics").items()) result["diagnostics"][uri] = entries;
        if (!csharp.value("error","").empty()) result["error"] = result.value("error","") + " C#: " + csharp.at("error").get<std::string>();
    }
    return result;
}
void LanguageService::run() {
    LanguageClient client;
    std::map<std::string, Document> documents;
    std::map<std::string, std::pair<std::uint64_t, int>> sent;
    std::vector<std::shared_ptr<Query>> pending;
    bool attempted = false, enabled = !managed_, prewarm = false;
    std::string executable = managed_ ? managedExecutable() : defaultExecutable(), failure;
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
            wake_.wait_for(lock, std::chrono::milliseconds(8), [this] {
                return stopping_ || reconfigure_ || !updates_.empty() || !queries_.empty();
            });
            stop = stopping_;
            reset = reconfigure_;
            reconfigure_ = false;
            if (reset) {
                executable = executable_;
                enabled = enabled_;
                prewarm = prewarm_;
                compilationDatabase = compilationDatabase_;
                project = project_;
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
            if (managed_) {
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
            } else if (!client.start(root_, executable, compilationDatabase)) failure = client.error();
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
                                         {"languageId", managed_ ? "csharp" : utf8Path(path).extension() == ".c" ? "c" : "cpp"},
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
                const bool highlight = q->method == "cycode/semanticTokens";
                const auto provider=client.capabilities().value("semanticTokensProvider",Json::object());
                const auto generation=client.semanticGeneration();
                const bool supported=provider.is_object() && provider.contains("full") && (provider.at("full").is_object() || provider.at("full")==true);
                if (highlight && !supported) { q->done=true; q->promise.set_value({{"supported",false},{"generation",generation}}); continue; }
                const auto legend=highlight ? provider.value("legend",Json::object()) : Json::object();
                q->id = client.request(highlight ? "textDocument/semanticTokens/full" : q->method, q->params, [q,highlight,legend,generation](const Json &result) {
                    if (q->done)
                        return;
                    q->done = true;
                    if (result.is_object() && result.contains("error"))
                        q->promise.set_exception(std::make_exception_ptr(
                            std::runtime_error("Language server: " + result["error"].dump())));
                    else if (highlight)
                        q->promise.set_value({{"supported",true},{"legend",legend},{"result",result},{"generation",generation}});
                    else
                        q->promise.set_value(result);
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
            if (reset || diagnosticsGeneration != client.diagnosticGeneration()) {
                diagnosticsGeneration = client.diagnosticGeneration();
                status_["diagnostics"] = client.diagnostics();
            }
        }
    }
}
} // namespace cycode

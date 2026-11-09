#include "cycode/Error.h"
#include "cycode/Engine.h"
#include "cycode/SymbolCatalogue.h"
#include "cycode/TextEdits.h"
#include "cycode/ChangeAnnotations.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

namespace cycode {
namespace fs = std::filesystem;
static std::string rawSource(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read source: " + pathUtf8(path));
    in.seekg(0, std::ios::end); const auto length = in.tellg();
    if (length < 0 || length > 4 * 1024 * 1024) throw std::runtime_error("Editor limit: 4 MiB per file");
    in.seekg(0); std::string text(static_cast<size_t>(length), '\0');
    in.read(text.data(), length);
    if (!in && length) throw std::runtime_error("Source changed while reading");
    if (text.find('\0') != std::string::npos) throw std::runtime_error("Binary source files cannot be opened");
    (void)Json(text).dump();
    return text;
}
static std::string normalized(std::string text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') { if (i+1 < text.size() && text[i+1] == '\n') ++i; out += '\n'; }
        else out += text[i];
    }
    return out;
}
void Engine::addSdkRoots(const Json &model) {
    std::lock_guard lock(mutex_);
    auto add = [&](const fs::path &path) {
        std::error_code ec;
        if (!path.empty() && fs::is_directory(path, ec)) {
            auto canonical = fs::weakly_canonical(path, ec);
            if (!ec && std::find(sdkRoots_.begin(), sdkRoots_.end(), canonical) == sdkRoots_.end()) sdkRoots_.push_back(canonical);
        }
    };
    if (model.value("available", false) && model.contains("profile")) {
        const auto root = utf8Path(model.at("profile").value("engineRoot", ""));
        if (!root.empty()) { add(root / "Engine/Source"); add(root / "Engine/Plugins"); }
    }
#ifdef _WIN32
    fs::path program = _wgetenv(L"ProgramFiles") ? _wgetenv(L"ProgramFiles") : L"C:/Program Files";
    fs::path program32 = _wgetenv(L"ProgramFiles(x86)") ? _wgetenv(L"ProgramFiles(x86)") : L"C:/Program Files (x86)";
    add(program32 / "Windows Kits/10/Include"); add(program32 / "Windows Kits/8.1/Include");
    add(program / "LLVM/lib/clang"); add(program / "LLVM/include");
    const auto vs = program / "Microsoft Visual Studio";
    std::error_code ec;
    if (fs::is_directory(vs, ec)) for (const auto &year : fs::directory_iterator(vs, fs::directory_options::skip_permission_denied, ec)) {
        if (!year.is_directory(ec)) continue;
        for (const auto &edition : fs::directory_iterator(year.path(), fs::directory_options::skip_permission_denied, ec)) {
            auto versions = edition.path() / "VC/Tools/MSVC";
            if (!fs::is_directory(versions, ec)) continue;
            for (const auto &version : fs::directory_iterator(versions, fs::directory_options::skip_permission_denied, ec)) add(version.path() / "include");
        }
    }
#else
    for (const auto *root : {"/usr/include", "/usr/local/include", "/opt/homebrew/include", "/Library/Developer/CommandLineTools/SDKs", "/Applications/Xcode.app/Contents/Developer/Platforms"}) add(root);
#endif
}
Json Engine::sourceLocationLocked(const std::string &uri, const Json &range, bool includeExternal) {
    auto path = fs::weakly_canonical(LanguageClient::fromUri(uri));
    if (containsPath(root_, path)) return {{"path", pathUtf8(path.lexically_relative(root_))}, {"range", range}};
    if (!includeExternal) return nullptr;
    auto extension = pathUtf8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!language_.accepts(pathUtf8(path)) && extension != ".inc" && extension != ".tpp") return nullptr;
    if (std::none_of(sdkRoots_.begin(), sdkRoots_.end(), [&](const auto &root) { return containsPath(root, path); })) return nullptr;
    std::string handle;
    for (const auto &[key, source] : externalSources_) if (source.path == path) { handle = key; break; }
    if (handle.empty()) {
        if (externalSources_.size() >= 1024) return nullptr;
        handle = "sdk://" + std::to_string(nextSource_++) + "/" + pathUtf8(path.filename());
        externalSources_.emplace(handle, ExternalSource{path});
    }
    return {{"path", handle}, {"range", range}, {"external", true}, {"displayPath", pathUtf8(path)}};
}
Json Engine::navigationSource(const fs::path &path, const Json &range) {
    std::lock_guard lock(mutex_); return sourceLocationLocked(LanguageClient::uri(path),range,true);
}
Json Engine::sourceOperation(const std::string &method, const Json &params) {
    std::lock_guard lock(mutex_);
    const auto handle = params.at("source").get<std::string>();
    auto found = externalSources_.find(handle);
    if (found == externalSources_.end()) throw std::runtime_error("Unknown SDK source reference; navigate to a declared SDK source first");
    auto &source = found->second;
    if (method == "sources.close") { language_.close(pathUtf8(source.path)); source.loaded = false; source.document = {}; return {{"closed", true}}; }
    auto canonical = fs::weakly_canonical(source.path);
    if (std::none_of(sdkRoots_.begin(), sdkRoots_.end(), [&](const auto &root) { return containsPath(root, canonical); })) throw std::runtime_error("SDK source moved outside its registered root");
    if (!source.loaded) {
        auto raw = rawSource(canonical);
        auto &doc = source.document;
        doc.disk = raw; doc.bom = raw.starts_with("\xef\xbb\xbf");
        if (doc.bom) raw.erase(0, 3);
        doc.crlf = raw.find("\r\n") != std::string::npos;
        doc.crOnly = raw.find('\r') != std::string::npos && raw.find('\n') == std::string::npos;
        doc.text = normalized(raw); doc.saved = doc.text; doc.revision = nextRevision_++; doc.identity = nextDocumentIdentity_++;
        source.loaded = true; language_.sync(pathUtf8(source.path), doc.text, doc.revision, doc.identity);
    }
    auto result = documentJson(handle, source.document);
    result["readOnly"] = true; result["displayPath"] = pathUtf8(source.path);
    return result;
}
std::pair<Engine::Document *, fs::path> Engine::semanticDocumentLocked(const std::string &key) {
    auto external = externalSources_.find(key);
    if (external != externalSources_.end()) {
        if (!external->second.loaded) throw std::runtime_error("Open the SDK source before querying it");
        return {&external->second.document, external->second.path};
    }
    return {&openLocked(key), resolve(key)};
}
Json Engine::symbolQuery(const std::string &method, const Json &params) {
    std::future<Json> future;
    uint64_t revision = 0, identity = 0, generation = 0, context = 0, diskGeneration = 0, epoch = 0;
    const bool document = method == "language.documentSymbols";
    std::string key = document ? params.at("path").get<std::string>() : "", backend, uri;
    fs::path source;
    {
        std::lock_guard lock(mutex_);
        Json request;
        if (document) {
            auto [doc, full] = semanticDocumentLocked(key);
            revision = doc->revision; identity = doc->identity; source = full;
            if (revision != params.at("expectedRevision") || (params.contains("expectedDocumentId") && params.at("expectedDocumentId") != identity))
                throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: document changed; read it again");
            if (!externalSources_.contains(key)) key = pathUtf8(full.lexically_relative(root_));
            uri = LanguageClient::uri(full);
            backend = language_.backend(pathUtf8(full));
            request = {{"textDocument", {{"uri", uri}}}};
        } else {
            const auto query = params.value("query", "");
            if (query.size() > 256 || query.find('\0') != query.npos) throw std::runtime_error("Invalid symbol query (maximum 256 UTF-8 bytes)");
            backend = params.value("language", "cpp");
            if (!language_.hasBackend(backend)) throw std::runtime_error("Unknown symbol search language");
            request = {{"query", query}, {"cycodeLanguage", backend}};
        }
        epoch = language_.backendEpoch(backend);
        if (params.contains("expectedEpoch") && params.at("expectedEpoch") != epoch)
            throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: language configuration changed");
        generation = nextRevision_; context=hierarchyGeneration_; diskGeneration=monitor_.snapshot().revision;
        future = language_.request(document ? "textDocument/documentSymbol" : "workspace/symbol", request);
    }
    const auto response = future.get();
    std::lock_guard lock(mutex_);
    if (epoch != language_.backendEpoch(backend)) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: language configuration changed during symbol query");
    const Document *current = nullptr;
    if (document) {
        if (const auto found = externalSources_.find(key); found != externalSources_.end()) {
            if (found->second.loaded) current = &found->second.document;
        } else if (const auto found = documents_.find(key); found != documents_.end()) current = &found->second;
        if (!current || current->identity != identity || current->revision != revision)
            throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: symbol document was edited, closed or replaced");
    } else if (generation != nextRevision_ || context!=hierarchyGeneration_ || diskGeneration!=monitor_.snapshot().revision) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: workspace changed during symbol query");
    std::optional<SymbolTextBounds> bounds;
    if (document) bounds.emplace(current->text);
    const auto limit = static_cast<size_t>(std::clamp(params.value("limit", 200), 1, 1000));
    pruneSymbolSessionsLocked();
    SymbolSession session;
    session.backend=backend; session.epoch=epoch; session.generation=generation; session.context=context; session.diskGeneration=diskGeneration;
    session.includeExternal=params.value("includeExternal", false);
    auto result = collectSymbols(response, uri, limit, params.value("typesOnly", false),
        [&](const std::string &targetUri, const Json &selection, const Json &extent) -> Json {
            if (document) {
                if (LanguageClient::uriKey(targetUri) != LanguageClient::uriKey(uri))
                    throw std::runtime_error("Document symbol belongs to another source");
                if (!bounds->accepts(selection) || !bounds->accepts(extent))
                    throw std::runtime_error("Symbol range is outside the live UTF-16 buffer");
            }
            return sourceLocationLocked(targetUri, selection, params.value("includeExternal", false));
        }, document ? PendingSymbolResolver{} : PendingSymbolResolver{[&](const std::string &targetUri, const Json &item) -> Json {
            auto target=sourceLocationLocked(targetUri, Json(), session.includeExternal);
            if (target.is_null()) return nullptr;
            const auto bytes=item.dump().size();
            if (bytes>64*1024 || session.items.size()>=1000 || bytes>1024*1024-session.bytes)
                throw std::runtime_error("Workspace symbol resolve data exceeds the session budget");
            target.erase("range"); target["unresolved"]=true; target["resolveIndex"]=session.items.size();
            session.items.push_back(item); session.bytes+=bytes;
            return target;
        }});
    for (const auto &row:result.at("symbols")) if (row.value("unresolved", false))
        session.descriptions[std::to_string(row.at("resolveIndex").get<size_t>())]=row.at("node");
    if (!session.descriptions.empty()) {
        const auto id="ws-symbol-"+std::to_string(nextSymbolSession_++);
        symbolSessions_.emplace(id,std::move(session));
        pruneSymbolSessionsLocked(); result["session"]=id;
    }
    result["backend"] = backend; result["epoch"] = epoch;
    if (document) { result["path"] = key; result["revision"] = revision; result["documentId"] = identity; }
    return result;
}
void Engine::pruneSymbolSessionsLocked() {
    const auto now=std::chrono::steady_clock::now();
    std::erase_if(symbolSessions_,[&](const auto &entry) {
        return now-entry.second.created>std::chrono::minutes(2);
    });
    while (symbolSessions_.size()>4) {
        const auto oldest=std::min_element(symbolSessions_.begin(),symbolSessions_.end(),[](const auto &a,const auto &b) {
            return a.second.created<b.second.created;
        });
        symbolSessions_.erase(oldest);
    }
}
Json Engine::resolveSymbol(const Json &params) {
    const auto id=params.at("session").get<std::string>();
    if (id.empty() || id.size()>40 || (!params.at("index").is_number_unsigned() && !params.at("index").is_number_integer()))
        throw std::runtime_error("Invalid workspace symbol reference");
    const auto index=params.at("index").get<int64_t>();
    if (index<0 || index>=1000) throw std::runtime_error("Invalid workspace symbol index");
    const auto slot=std::to_string(index);
    std::future<Json> future;
    Json original;
    std::string backend;
    uint64_t epoch=0,generation=0,context=0,diskGeneration=0;
    bool includeExternal=false;
    {
        std::lock_guard lock(mutex_);
        pruneSymbolSessionsLocked();
        const auto found=symbolSessions_.find(id);
        if (found==symbolSessions_.end() || !found->second.descriptions.contains(slot))
            throw std::runtime_error("Unknown or expired workspace symbol; search again");
        auto &session=found->second;
        backend=session.backend; epoch=session.epoch; generation=session.generation; context=session.context; diskGeneration=session.diskGeneration; includeExternal=session.includeExternal;
        if (generation!=nextRevision_ || context!=hierarchyGeneration_ || diskGeneration!=monitor_.snapshot().revision || epoch!=language_.backendEpoch(backend) ||
            (params.contains("expectedEpoch") && params.at("expectedEpoch")!=epoch))
            throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: workspace or language configuration changed; search again");
        if (session.resolved.contains(slot)) return session.resolved.at(slot);
        original=session.items.at(static_cast<size_t>(index));
        auto request=original; request["cycodeLanguage"]=backend;
        future=language_.request("workspaceSymbol/resolve",request);
    }
    const auto response=future.get();
    std::lock_guard lock(mutex_);
    pruneSymbolSessionsLocked();
    const auto found=symbolSessions_.find(id);
    if (found==symbolSessions_.end() || generation!=nextRevision_ || context!=hierarchyGeneration_ || diskGeneration!=monitor_.snapshot().revision || epoch!=language_.backendEpoch(backend))
        throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: symbol session expired or workspace/language changed during resolution");
    if (!response.is_object() || response.value("name",Json())!=original.at("name") ||
        response.value("kind",Json())!=original.at("kind") || !response.contains("location") ||
        response.at("location").value("uri",Json())!=original.at("location").at("uri"))
        throw std::runtime_error("Language server resolved a different workspace symbol");
    // Only the advertised lazy property is trusted. Retain the issued metadata/data.
    auto resolved=original;
    resolved["location"]["range"]=response.at("location").at("range");
    auto report=collectSymbols(Json::array({resolved}),"",1,false,
        [&](const std::string &uri,const Json &range,const Json &) {
            return sourceLocationLocked(uri,range,includeExternal);
        });
    if (report.at("symbols").size()!=1)
        throw std::runtime_error("Resolved workspace symbol has an invalid or unavailable source location");
    auto row=std::move(report.at("symbols").at(0));
    row["node"]=found->second.descriptions.at(slot);
    Json result={{"symbol",std::move(row)},{"backend",backend},{"epoch",epoch},{"positionEncoding","utf-16"}};
    const auto bytes=result.dump().size();
    if (bytes<=1024*1024-found->second.bytes && !found->second.resolved.contains(slot)) {
        found->second.bytes+=bytes; found->second.resolved[slot]=result;
    }
    return result;
}
Json Engine::prepareChange(const std::string &method, const Json &params) {
    const auto key = pathUtf8(resolve(params.at("path").get<std::string>()).lexically_relative(root_));
    if (!language_.accepts(key)) throw std::runtime_error("No language backend for this file type");
    std::string before, oldName; std::uint64_t revision, generation;
    Json request;
    const bool rename = method == "language.rename";
    {
        std::lock_guard lock(mutex_);
        auto &doc = openLocked(key); before = doc.text; revision = doc.revision; generation = nextRevision_;
        if (params.at("expectedRevision") != revision) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: document changed; read it again");
        request = {{"textDocument", {{"uri", LanguageClient::uri(resolve(key))}}}};
        if (rename) {
            (void)utf16Offset(before, params.at("position"));
            request["position"] = params.at("position");
            const auto name = params.at("newName").get<std::string>();
            if (name.empty() || name.size() > 256) throw std::runtime_error("Invalid identifier");
            for (unsigned char c : name) if (c<33 || c==127) throw std::runtime_error("Invalid identifier");
            if (LanguageService::supportsCpp(key) || LanguageService::supportsManaged(key)) {
                if (name[0]>='0' && name[0]<='9') throw std::runtime_error("Invalid identifier");
                for (unsigned char c : name) if (c<128 && !(std::isalnum(c) || c=='_')) throw std::runtime_error("Invalid identifier");
            }
            (void)Json(name).dump();
        } else {
            const auto size = params.value("tabSize", 4);
            if (size < 1 || size > 16) throw std::runtime_error("Tab size must be between 1 and 16");
            request["options"] = {{"tabSize", size}, {"insertSpaces", params.value("insertSpaces", true)}};
            if (params.contains("range")) {
                (void)utf16Offset(before, params.at("range").at("start")); (void)utf16Offset(before, params.at("range").at("end"));
                request["range"] = params.at("range");
            }
        }
    }
    auto ask = [&](const std::string &operation, const Json &data) {
        std::future<Json> future;
        {
            std::lock_guard lock(mutex_);
            if (generation != nextRevision_ || !documents_.contains(key) || documents_.at(key).revision != revision) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: document changed during refactoring");
            future = language_.request(operation, data);
        }
        return future.get();
    };
    Json edits,coverage;
    std::string title;
    if (rename) {
        // CSS supports rename without prepareRename. Its symbol range is the
        // current selector identifier; the server still computes every edit.
        Json prepared;
        if (language_.backend(key)=="css") {
            const auto offset=utf16Offset(before,request.at("position"));size_t a=offset,b=offset;
            auto identifier=[](unsigned char c) { return c>=128 || std::isalnum(c) || c=='_' || c=='-'; };
            while (a && identifier(before[a-1])) --a;
            while (b<before.size() && identifier(before[b])) ++b;
            if (a && (before[a-1]=='.' || before[a-1]=='#')) --a;
            auto position=[&](size_t at) { const auto start=before.rfind('\n',at ? at-1 : 0);const auto lineStart=start==std::string::npos ? 0 : start+1;
                return Json{{"line",std::count(before.begin(),before.begin()+at,'\n')},{"character",LanguageClient::utf16Length(before.substr(lineStart,at-lineStart))}}; };
            prepared={{"start",position(a)},{"end",position(b)}};
        } else prepared = ask("textDocument/prepareRename", request);
        if (prepared.is_null()) throw std::runtime_error("The selected symbol cannot be renamed");
        const auto range = prepared.value("range", prepared);
        const auto a = utf16Offset(before, range.at("start")), b = utf16Offset(before, range.at("end"));
        if (b <= a) throw std::runtime_error("The selected symbol has no renameable range");
        oldName = before.substr(a, b-a);
        request["newName"] = params.at("newName");
        if (language_.backend(key)=="css" && (oldName.starts_with('.') || oldName.starts_with('#'))) {
            auto name=request.at("newName").get<std::string>();
            if (!name.starts_with('.') && !name.starts_with('#')) request["newName"]=oldName.substr(0,1)+name;
        }
        if(LanguageService::supportsCpp(key)) {
            // Clangd can return a valid but incomplete WorkspaceEdit before its
            // background index knows unopened declarations/callers. Prepare every
            // source-spelled candidate before asking for the authoritative edits.
            // A compiler-confirmed parameter is confined to its function and
            // does not need a project-wide spelling scan.
            bool parameter=false;
            try {
                const auto ast=ask("textDocument/ast",{{"textDocument",request.at("textDocument")},{"range",range}});
                const auto kind=ast.value("kind",std::string{});
                parameter=kind=="ParmVar" || kind=="ParmVarDecl";
            } catch(const NativeError &error) {
                if(error.code()==ErrorCode::RevisionConflict || error.code()==ErrorCode::Cancelled)throw;
            }
            if(!parameter) {
                const auto preparation=prepareAnalysis(params,oldName,key,&generation,&edits);
                coverage={{"strategy","workspace-spelling-candidates"},{"preparedFiles",preparation.at("total")},
                    {"wholeProjectIndexCompleteness","unknown"},
                    {"limitations",Json::array({"Source-spelled candidates inside this workspace; not serialized Unreal assets, token-pasted names or external projects"})}};
            } else coverage={{"strategy","compiler-local-parameter"},{"preparedFiles",1},{"wholeProjectIndexCompleteness","unknown"},{"limitations",Json::array()}};
        }
        if(edits.is_null())edits = ask("textDocument/rename", request);
        {
            // A server may coalesce edits over whole method bodies. Acquire disk
            // baselines and send live versions for previously unopened targets,
            // then recompute the edits against those exact buffers. The first
            // response is never applied to freshly read disk content.
            bool synchronize=false;
            {
                std::lock_guard lock(mutex_);
                if (generation!=nextRevision_) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: workspace changed during refactoring");
                std::set<std::string> paths;
                if (edits.is_object() && edits.contains("changes")) for (const auto &[uri,list] : edits.at("changes").items()) paths.insert(pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(uri))).lexically_relative(root_)));
                if (edits.is_object() && edits.contains("documentChanges")) for (const auto &change : edits.at("documentChanges")) {
                    if (!change.contains("textDocument")) throw std::runtime_error("File operations are not supported in refactor previews");
                    paths.insert(pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(change.at("textDocument").at("uri")))).lexically_relative(root_)));
                }
                for (const auto &path : paths) if (!documents_.contains(path)) { openLocked(path);synchronize=true; }
                generation=nextRevision_;
            }
            if (synchronize) edits=ask("textDocument/rename",request);
        }
        title = "Rename " + oldName + " -> " + request.at("newName").get<std::string>();
    } else {
        auto formatted = ask(params.contains("range") ? "textDocument/rangeFormatting" : "textDocument/formatting", request);
        edits = {{"changes", {{LanguageClient::uri(resolve(key)), formatted.is_null() ? Json::array() : formatted}}}};
        title = params.contains("range") ? "Format selection" : "Format document";
    }
    std::lock_guard lock(mutex_);
    if (generation != nextRevision_ || !documents_.contains(key) || documents_.at(key).revision != revision) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: workspace changed during refactoring");
    auto result=workspaceChangeLocked(title,edits,LanguageService::supportsCpp(key) || LanguageService::supportsManaged(key) ? oldName : "",rename);
    if(!coverage.is_null()) {
        auto &plan=changes_.at(result.at("id").get<std::string>());plan.coverage=coverage;
        if(coverage.at("strategy")=="workspace-spelling-candidates") {
            plan.scopeGeneration=nextRevision_;plan.scopeWorkspaceRevision=monitor_.snapshot().revision;
            plan.scopeBackend=language_.backend(key);plan.scopeEpoch=language_.backendEpoch(plan.scopeBackend);
        }
        result["coverage"]=std::move(coverage);
    }
    return result;
}
Json Engine::workspaceEditFilesLocked(const Json &supplied) {
    auto edits=supplied;
    Json files = Json::object();
    if (edits.is_null()) edits = Json::object();
    if (!edits.is_object()) throw std::runtime_error("Invalid workspace edit from language server");
    if (edits.contains("changes")) {
        if (!edits.at("changes").is_object()) throw std::runtime_error("Invalid workspace changes");
        files = edits.at("changes");
    }
    if (edits.contains("documentChanges")) for (const auto &change : edits.at("documentChanges")) {
        if (change.contains("kind") || !change.contains("textDocument")) throw std::runtime_error("This refactoring requires unsupported file operations");
        const auto &doc = change.at("textDocument");
        if (doc.contains("version") && !doc.at("version").is_null()) throw std::runtime_error("Versioned workspace edits are not supported by this adapter");
        if (doc.contains("cycodeRevision")) {
            const auto path=pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(doc.at("uri")))).lexically_relative(root_));
            const auto found=documents_.find(path);
            if (found==documents_.end() || found->second.revision!=doc.at("cycodeRevision")) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: workspace edit document changed");
        }
        const auto uri = doc.at("uri").get<std::string>();
        if (files.contains(uri)) throw std::runtime_error("Duplicate document in workspace edit");
        files[uri] = change.at("edits");
    }
    Json canonical=Json::object();
    for (auto i=files.begin();i!=files.end();++i) {
        const auto path=pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(i.key()))).lexically_relative(root_));
        if (canonical.contains(path)) throw std::runtime_error("Duplicate canonical document in workspace edit");
        canonical[path]=i.value();
    }
    return canonical;
}

Json Engine::workspaceChangeLocked(const std::string &title,const Json &edits,const std::string &oldName,bool onlyOpened) {
    const auto annotated=normalizeChangeAnnotations(workspaceEditFilesLocked(edits),edits.is_null()?Json::object():edits.value("changeAnnotations",Json::object()));
    const auto &files=annotated.files;
    ChangePlan plan; plan.title = title; size_t total = 0; std::set<std::string> paths;
    for (auto i = files.begin(); i != files.end(); ++i) {
        const auto path = i.key();
        if (!paths.insert(path).second) throw std::runtime_error("Duplicate canonical document in refactoring");
        if (onlyOpened && !documents_.contains(path))
            throw std::runtime_error("Workspace symbol index changed; prepare the rename again");
        auto &doc = openLocked(path);
        if (rawSource(resolve(path)) != doc.disk) throw NativeError(ErrorCode::DiskConflict,"DiskConflict: source changed outside CyCode: " + path);
        // JDT LS can coalesce replacements into a larger edit. All ranges,
        // document revisions and disk bytes remain checked by the preview.
        auto edited = applyTextEdits(doc.text, i.value(), oldName);
        edited.text = normalized(edited.text);
        if (edited.text == doc.text) continue;
        total += doc.text.size() + edited.text.size();
        if (total > 16 * 1024 * 1024) throw std::runtime_error("Refactoring preview exceeds 16 MiB");
        plan.files.push_back({path, doc.text, std::move(edited.text), doc.disk, std::move(edited.edits), doc.revision});
    }
    Json effective=Json::object();
    for(const auto &file:plan.files)effective[file.path]=file.edits;
    plan.annotations=usedChangeAnnotations(annotated.annotations,effective);
    return storeChange(std::move(plan));
}
Json Engine::storeChange(ChangePlan plan) {
    for (auto i = changes_.begin(); i != changes_.end();) {
        if (std::chrono::steady_clock::now() - i->second.created > std::chrono::minutes(20)) i = changes_.erase(i); else ++i;
    }
    if (changes_.size() >= 8) {
        auto oldest = std::min_element(changes_.begin(), changes_.end(), [](const auto &a, const auto &b) { return a.second.created < b.second.created; });
        changes_.erase(oldest);
    }
    const auto id = "change-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(nextChange_++);
    std::map<std::string,std::string> annotationIds;
    for(auto &annotation:plan.annotations) {
        const auto previous=annotation.at("id").get<std::string>();
        annotationIds[previous]=id+":"+previous;annotation["id"]=annotationIds.at(previous);
    }
    for(auto &file:plan.files)for(auto &edit:file.edits)if(edit.contains("annotationId"))
        edit["annotationId"]=annotationIds.at(edit.at("annotationId").get<std::string>());
    if(plan.annotations.dump().size()>512*1024)throw std::runtime_error("Issued change annotations exceed 512 KiB");
    changes_.emplace(id, std::move(plan));
    return changeDescription(id);
}
Json Engine::changeDescription(const std::string &id) const {
    const auto &plan = changes_.at(id);
    Json files = Json::array(); size_t edits = 0;
    for (const auto &file : plan.files) { edits += file.edits.size(); files.push_back({{"path", file.path}, {"expectedRevision", file.revision}, {"editCount", file.edits.size()}}); }
    Json result={{"id", id}, {"title", plan.title}, {"state", plan.state}, {"files", files}, {"editCount", edits}, {"annotations",plan.annotations}};
    if(!plan.coverage.is_null())result["coverage"]=plan.coverage;
    return result;
}
Json Engine::changeOperation(const std::string &method, const Json &params) {
    std::lock_guard lock(mutex_);
    const auto id = params.at("id").get<std::string>();
    auto found = changes_.find(id);
    if (found == changes_.end()) throw std::runtime_error("Refactoring preview has expired; prepare it again");
    auto &plan = found->second;
    if (method == "changes.preview") {
        const auto path = params.at("path").get<std::string>();
        for (const auto &file : plan.files) if (file.path == path) {
            const auto annotations=usedChangeAnnotations(plan.annotations,Json{{path,file.edits}});
            return {{"id", id}, {"path", path}, {"before", file.before}, {"after", file.after}, {"edits", file.edits}, {"annotations",annotations}};
        }
        throw std::runtime_error("Document is not part of this refactoring");
    }
    const bool undo = method == "changes.undo";
    if (plan.state != (undo ? "applied" : "preview")) throw std::runtime_error("Refactoring is not in the expected state");
    if(!undo && plan.scopeGeneration && (plan.scopeGeneration!=nextRevision_ ||
        plan.scopeWorkspaceRevision!=monitor_.snapshot().revision || language_.backendEpoch(plan.scopeBackend)!=plan.scopeEpoch))
        throw NativeError(ErrorCode::RevisionConflict,"Rename preparation scope changed; prepare the rename again");
    if (!undo) confirmChangeAnnotations(plan.annotations,params.value("confirmedAnnotations",Json::array()));
    // Validate EVERY affected buffer and disk baseline before mutating ANY buffer.
    for (const auto &file : plan.files) {
        auto doc = documents_.find(file.path);
        if (doc == documents_.end() || doc->second.revision != (undo ? file.appliedRevision : file.revision) || doc->second.text != (undo ? file.after : file.before)) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: refactoring source changed: " + file.path);
        if (rawSource(resolve(file.path)) != doc->second.disk) throw NativeError(ErrorCode::DiskConflict,"DiskConflict: refactoring source changed outside CyCode: " + file.path);
    }
    for (auto &file : plan.files) {
        auto &doc = documents_.at(file.path);
        doc.text = undo ? file.before : file.after; doc.revision = nextRevision_++; doc.dirty = doc.text != doc.saved;
        if (!undo) file.appliedRevision = doc.revision;
        language_.sync(file.path, doc.text, doc.revision, doc.identity, true);
    }
    plan.state = undo ? "undone" : "applied";
    for(const auto &file:plan.files)documentEvent("document.changed",file.path,documents_.at(file.path),id);
    events_.publish("transaction.committed",{{"id",id},{"action",undo?"undo":"apply"},{"documents",plan.files.size()}});
    return changeDescription(id);
}
} // namespace cycode

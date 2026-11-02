#include "cycode/Engine.h"
#include "cycode/TextEdits.h"
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
    if (!LanguageService::supports(pathUtf8(path)) && extension != ".inc" && extension != ".tpp") return nullptr;
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
        doc.text = normalized(raw); doc.saved = doc.text; doc.revision = nextRevision_++;
        source.loaded = true; language_.sync(pathUtf8(source.path), doc.text, doc.revision);
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
    std::uint64_t revision = 0, generation;
    const bool document = method == "language.documentSymbols";
    const auto key = document ? params.at("path").get<std::string>() : "";
    fs::path source;
    {
        std::lock_guard lock(mutex_);
        Json request;
        if (document) {
            auto [doc, full] = semanticDocumentLocked(key);
            revision = doc->revision; source = full;
            if (revision != params.at("expectedRevision")) throw std::runtime_error("RevisionConflict: document changed; read it again");
            request = {{"textDocument", {{"uri", LanguageClient::uri(full)}}}};
        } else {
            auto query = params.value("query", "");
            if (query.size() > 256) throw std::runtime_error("Symbol query exceeds 256 bytes");
            const auto backend = params.value("language","cpp");
            const auto &backends=languageBackends();
            if (std::find(backends.begin(),backends.end(),backend)==backends.end()) throw std::runtime_error("Unknown symbol search language");
            request = {{"query", query}, {"cycodeLanguage",backend}};
        }
        generation = nextRevision_;
        future = language_.request(document ? "textDocument/documentSymbol" : "workspace/symbol", request);
    }
    auto response = future.get();
    std::lock_guard lock(mutex_);
    if (generation != nextRevision_) throw std::runtime_error("RevisionConflict: workspace changed during symbol query");
    Json symbols = Json::array(); std::set<std::string> unique;
    size_t outside = 0; bool truncated = false;
    const auto limit = static_cast<size_t>(std::clamp(params.value("limit", 200), 1, 1000));
    const bool types = params.value("typesOnly", false), external = params.value("includeExternal", false);
    std::function<void(const Json &, const std::string &, size_t)> collect = [&](const Json &list, const std::string &container, size_t depth) {
        if (!list.is_array() || depth > 64) return;
        for (const auto &item : list) {
            if (!item.is_object()) continue;
            const auto name = item.value("name", ""), parent = item.value("containerName", container);
            const int kind = item.value("kind", 0);
            auto location = item.value("location", Json::object());
            auto range = item.value("selectionRange", location.value("range", item.value("range", Json())));
            const auto uri = location.value("uri", document ? LanguageClient::uri(source) : "");
            if (!name.empty() && range.is_object() && (!types || kind == 5 || kind == 10 || kind == 11 || kind == 23 || kind == 26)) {
                Json target;
                try { target = sourceLocationLocked(uri, range, external); } catch (const std::exception &) { target = nullptr; }
                if (target.is_null()) ++outside;
                else {
                    target.update({{"name", name}, {"kind", kind}, {"container", parent}, {"detail", item.value("detail", "")}});
                    if (unique.insert(target.dump()).second) {
                        if (symbols.size() < limit) symbols.push_back(std::move(target)); else truncated = true;
                    }
                }
            }
            if (item.contains("children")) collect(item.at("children"), parent.empty() ? name : parent + "::" + name, depth+1);
        }
    };
    collect(response, "", 0);
    Json result = {{"symbols", symbols}, {"outsideWorkspace", outside}, {"truncated", truncated}, {"positionEncoding", "utf-16"}};
    if (document) { result["path"] = key; result["revision"] = revision; }
    return result;
}
Json Engine::prepareChange(const std::string &method, const Json &params) {
    const auto key = pathUtf8(resolve(params.at("path").get<std::string>()).lexically_relative(root_));
    if (!LanguageService::supports(key)) throw std::runtime_error("No language backend for this file type");
    std::string before, oldName; std::uint64_t revision, generation;
    Json request;
    const bool rename = method == "language.rename";
    {
        std::lock_guard lock(mutex_);
        auto &doc = openLocked(key); before = doc.text; revision = doc.revision; generation = nextRevision_;
        if (params.at("expectedRevision") != revision) throw std::runtime_error("RevisionConflict: document changed; read it again");
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
            if (generation != nextRevision_ || !documents_.contains(key) || documents_.at(key).revision != revision) throw std::runtime_error("RevisionConflict: document changed during refactoring");
            future = language_.request(operation, data);
        }
        return future.get();
    };
    Json edits;
    std::string title;
    if (rename) {
        // CSS supports rename without prepareRename. Its symbol range is the
        // current selector identifier; the server still computes every edit.
        Json prepared;
        if (languageBackend(key)=="css") {
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
        if (languageBackend(key)=="css" && (oldName.starts_with('.') || oldName.starts_with('#'))) {
            auto name=request.at("newName").get<std::string>();
            if (!name.starts_with('.') && !name.starts_with('#')) request["newName"]=oldName.substr(0,1)+name;
        }
        edits = ask("textDocument/rename", request);
        if (!LanguageService::supportsCpp(key) && !LanguageService::supportsManaged(key)) {
            // A server may coalesce edits over whole method bodies. Acquire disk
            // baselines and send live versions for previously unopened targets,
            // then recompute the edits against those exact buffers. The first
            // response is never applied to freshly read disk content.
            bool synchronize=false;
            {
                std::lock_guard lock(mutex_);
                if (generation!=nextRevision_) throw std::runtime_error("RevisionConflict: workspace changed during refactoring");
                std::set<std::string> paths;
                if (edits.is_object() && edits.contains("changes")) for (const auto &[uri,list] : edits.at("changes").items()) paths.insert(pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(uri))).lexically_relative(root_)));
                if (edits.is_object() && edits.contains("documentChanges")) for (const auto &change : edits.at("documentChanges")) {
                    if (!change.contains("textDocument")) throw std::runtime_error("File operations are not supported in refactor previews");
                    paths.insert(pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(change.at("textDocument").at("uri")))).lexically_relative(root_)));
                }
                if (paths.size()>100) throw std::runtime_error("Refactoring exceeds the 100-document limit");
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
    if (generation != nextRevision_ || !documents_.contains(key) || documents_.at(key).revision != revision) throw std::runtime_error("RevisionConflict: workspace changed during refactoring");
    return workspaceChangeLocked(title,edits,LanguageService::supportsCpp(key) || LanguageService::supportsManaged(key) ? oldName : "",rename && !LanguageService::supportsCpp(key) && !LanguageService::supportsManaged(key));
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
            if (found==documents_.end() || found->second.revision!=doc.at("cycodeRevision")) throw std::runtime_error("RevisionConflict: workspace edit document changed");
        }
        const auto uri = doc.at("uri").get<std::string>();
        if (files.contains(uri)) throw std::runtime_error("Duplicate document in workspace edit");
        files[uri] = change.at("edits");
    }
    if (files.size() > 100) throw std::runtime_error("Refactoring exceeds the 100-document limit");
    Json canonical=Json::object();
    for (auto i=files.begin();i!=files.end();++i) {
        const auto path=pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(i.key()))).lexically_relative(root_));
        if (canonical.contains(path)) throw std::runtime_error("Duplicate canonical document in workspace edit");
        canonical[path]=i.value();
    }
    return canonical;
}

Json Engine::workspaceChangeLocked(const std::string &title,const Json &edits,const std::string &oldName,bool onlyOpened) {
    const auto files=workspaceEditFilesLocked(edits);
    ChangePlan plan; plan.title = title; size_t total = 0; std::set<std::string> paths;
    for (auto i = files.begin(); i != files.end(); ++i) {
        const auto path = i.key();
        if (!paths.insert(path).second) throw std::runtime_error("Duplicate canonical document in refactoring");
        if (onlyOpened && !documents_.contains(path))
            throw std::runtime_error("Workspace symbol index changed; prepare the rename again");
        auto &doc = openLocked(path);
        if (rawSource(resolve(path)) != doc.disk) throw std::runtime_error("DiskConflict: source changed outside CyCode: " + path);
        // JDT LS can coalesce replacements into a larger edit. All ranges,
        // document revisions and disk bytes remain checked by the preview.
        auto edited = applyTextEdits(doc.text, i.value(), oldName);
        edited.text = normalized(edited.text);
        if (edited.text == doc.text) continue;
        total += doc.text.size() + edited.text.size();
        if (total > 16 * 1024 * 1024) throw std::runtime_error("Refactoring preview exceeds 16 MiB");
        plan.files.push_back({path, doc.text, std::move(edited.text), doc.disk, std::move(edited.edits), doc.revision});
    }
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
    changes_.emplace(id, std::move(plan));
    return changeDescription(id);
}
Json Engine::changeDescription(const std::string &id) const {
    const auto &plan = changes_.at(id);
    Json files = Json::array(); size_t edits = 0;
    for (const auto &file : plan.files) { edits += file.edits.size(); files.push_back({{"path", file.path}, {"expectedRevision", file.revision}, {"editCount", file.edits.size()}}); }
    return {{"id", id}, {"title", plan.title}, {"state", plan.state}, {"files", files}, {"editCount", edits}};
}
Json Engine::changeOperation(const std::string &method, const Json &params) {
    std::lock_guard lock(mutex_);
    const auto id = params.at("id").get<std::string>();
    auto found = changes_.find(id);
    if (found == changes_.end()) throw std::runtime_error("Refactoring preview has expired; prepare it again");
    auto &plan = found->second;
    if (method == "changes.preview") {
        const auto path = params.at("path").get<std::string>();
        for (const auto &file : plan.files) if (file.path == path) return {{"id", id}, {"path", path}, {"before", file.before}, {"after", file.after}, {"edits", file.edits}};
        throw std::runtime_error("Document is not part of this refactoring");
    }
    const bool undo = method == "changes.undo";
    if (plan.state != (undo ? "applied" : "preview")) throw std::runtime_error("Refactoring is not in the expected state");
    // Validate EVERY affected buffer and disk baseline before mutating ANY buffer.
    for (const auto &file : plan.files) {
        auto doc = documents_.find(file.path);
        if (doc == documents_.end() || doc->second.revision != (undo ? file.appliedRevision : file.revision) || doc->second.text != (undo ? file.after : file.before)) throw std::runtime_error("RevisionConflict: refactoring source changed: " + file.path);
        if (rawSource(resolve(file.path)) != doc->second.disk) throw std::runtime_error("DiskConflict: refactoring source changed outside CyCode: " + file.path);
    }
    for (auto &file : plan.files) {
        auto &doc = documents_.at(file.path);
        doc.text = undo ? file.before : file.after; doc.revision = nextRevision_++; doc.dirty = doc.text != doc.saved;
        if (!undo) file.appliedRevision = doc.revision;
        language_.sync(file.path, doc.text, doc.revision);
    }
    plan.state = undo ? "undone" : "applied";
    return changeDescription(id);
}
} // namespace cycode

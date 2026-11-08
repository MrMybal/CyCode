#include "cycode/text/DocumentSession.h"
#include "cycode/text/Search.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <cctype>
namespace cycode::text {
DocumentSession::DocumentSession(const std::filesystem::path &plugins) { if (!plugins.empty()) plugins_.load(plugins); }
std::shared_ptr<LiveDocument> DocumentSession::add(Document document) {
    std::lock_guard lock(mutex_);
    if (!document.untitled()) for (const auto &[id,existing] : documents_) if (sameFile(existing->document.path(),document.path())) return existing;
    if (documents_.size()>=64) throw std::runtime_error("Close a document before opening more files");
    auto live=std::make_shared<LiveDocument>();live->id=nextId_++;live->document=std::move(document);live->revision=revision_++;live->observed=live->document.text();documents_.emplace(live->id,live);return live;
}
void DocumentSession::remove(size_t id) { std::lock_guard lock(mutex_);documents_.erase(id); }
void DocumentSession::refresh() { for (auto &[id,live] : documents_) { const auto format=std::string(live->document.encoding())+live->document.lineEnding();if (live->observed!=live->document.text() || live->observedFormat!=format) { live->observed=live->document.text();live->observedFormat=format;live->revision=revision_++; } } }
Json DocumentSession::describe(const LiveDocument &live,bool text) const {
    Json result={{"id",live.id},{"path",pathUtf8(live.document.path())},{"revision",live.revision},{"dirty",live.document.dirty()},{"readOnly",live.readOnly},{"encoding",live.document.encoding()},{"lineEnding",live.document.lineEnding()}};
    if (text) result["text"]=live.document.text();return result;
}
Json DocumentSession::dispatch(const std::string &method,const Json &params) {
    std::lock_guard lock(mutex_);refresh();
    if (method=="documents.list") { Json list=Json::array();for (const auto &[id,live] : documents_) list.push_back(describe(*live));return {{"documents",list}}; }
    if (method=="documents.open") { auto opened=add(Document::open(utf8Path(params.at("path")),params.value("encoding","auto")));refresh();return describe(*opened,true); }
    if (method=="documents.new") { auto opened=add();refresh();return describe(*opened,true); }
    if (method=="plugins.list") return plugins_.describe();
    if (method=="plugins.configure") return plugins_.configure(params.at("plugin"),params.at("options"));
    if (method=="search.openDocuments") {
        const auto query=params.at("query").get<std::string>();if (query.empty() || query.size()>4096) throw std::runtime_error("Search query must contain 1 to 4096 bytes");
        Json matches=Json::array();bool truncated=false;
        const bool sensitive=params.value("caseSensitive",true),words=params.value("wholeWord",false);
        for (const auto &[id,live] : documents_) {
            if (params.contains("document") && params.at("document")!=id) continue;
            const auto found=findTextMatches(live->document.text(),query,sensitive,words,2000-matches.size());
            for (const auto &m : found.matches) matches.push_back({{"id",id},{"revision",live->revision},{"offset",m.offset},{"line",m.line},{"column",m.column}});
            if (found.truncated) { truncated=true;break; }
        }
        return {{"matches",matches},{"truncated",truncated}};
    }
    const auto id=params.at("id").get<size_t>();const auto found=documents_.find(id);if (found==documents_.end()) throw std::runtime_error("Document is no longer open");auto &live=*found->second;
    if (method=="documents.read") return describe(live,true);
    if (params.at("expectedRevision")!=live.revision) throw std::runtime_error("RevisionConflict: read the document again");
    if (live.readOnly) throw std::runtime_error("Document is read-only");
    if (method=="documents.replace") live.document.replace(params.at("text"));
    else if (method=="documents.save") live.document.save();
    else if (method=="documents.format") live.document.format(params.value("encoding",""),params.value("lineEnding",""));
    else if (method=="documents.close") { if (live.document.dirty() && !params.value("discard",false)) throw std::runtime_error("Document has unsaved changes");documents_.erase(found);return {{"closed",id}}; }
    else if (method=="text.transform") live.document.replace(transformText(live.document.text(),params.at("operation")));
    else if (method=="plugins.invoke") {
        auto result=plugins_.invoke(params.at("plugin"),params.at("command"),{{"text",live.document.text()},{"path",pathUtf8(live.document.path())},{"revision",live.revision},{"encoding",live.document.encodingId()}});
        if (result.contains("text")) live.document.replace(result.at("text"));refresh();return {{"document",describe(live,true)},{"result",result}};
    } else throw std::runtime_error("Unknown Reader operation: "+method);
    refresh();return describe(live,true);
}
}

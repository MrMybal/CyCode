#include "cycode/Engine.h"
#include "cycode/Completion.h"
#include "cycode/TextFile.h"
#include <algorithm>
namespace cycode {
Json Engine::issueCompletion(const std::string &path,uint64_t revision,uint64_t generation,uint64_t epoch,const Json &position,const Json &result) {
    bool truncated=false;auto bounded=completionItems(result,&truncated);size_t bytes=bounded.dump().size();
    for (auto i=completions_.begin();i!=completions_.end();) {
        if (std::chrono::steady_clock::now()-i->second.created>std::chrono::minutes(5)) i=completions_.erase(i);else ++i;
    }
    auto cacheBytes=[&] { size_t n=0;for (const auto &[id,session] : completions_) n+=session.bytes;return n; };
    while (!completions_.empty() && (completions_.size()>=8 || cacheBytes()+bytes>8*1024*1024)) {
        auto oldest=std::min_element(completions_.begin(),completions_.end(),[](const auto &a,const auto &b) { return a.second.created<b.second.created; });completions_.erase(oldest);
    }
    const auto id="completion-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(nextCompletion_++);
    completions_.emplace(id,CompletionSession{path,revision,generation,epoch,position,bounded,{},bytes});
    return {{"path",path},{"revision",revision},{"sessionId",id},{"truncated",truncated},
        {"result",{{"items",std::move(bounded)},{"isIncomplete",truncated || (result.is_object() && result.value("isIncomplete",false))}}}};
}
Json Engine::completionOperation(const std::string &method,const Json &params) {
    const auto id=params.at("sessionId").get<std::string>();const auto index=params.at("item").get<size_t>();
    std::unique_lock lock(mutex_);
    auto checked=[&]() -> CompletionSession& {
        auto found=completions_.find(id);
        if (found==completions_.end() || std::chrono::steady_clock::now()-found->second.created>std::chrono::minutes(5)) throw std::runtime_error("Completion session expired; request completion again");
        auto &session=found->second;
        auto [doc,full]=semanticDocumentLocked(session.path);
        if (doc->revision!=session.revision || params.at("expectedRevision")!=session.revision || nextRevision_!=session.generation || language_.epoch(session.path)!=session.epoch)
            throw std::runtime_error("RevisionConflict: completion context changed; request completion again");
        if (index>=session.items.size()) throw std::runtime_error("Invalid completion item index");
        return session;
    };
    auto &initial=checked();const auto path=initial.path;
    if (method=="language.completionPrepare" && path.starts_with("sdk://")) throw std::runtime_error("SDK sources are read-only");
    bool resolved=initial.resolved.contains(index);
    const auto statuses=language_.status();const auto backend=languageBackend(path);
    const auto server=statuses.contains("servers") ? statuses.at("servers").at(backend) : statuses;
    const bool supported=server.value("methods",Json::object()).value("completionResolve",false);
    if (method=="language.completionResolve" && !supported) throw std::runtime_error("Language server does not support completionItem/resolve");
    if (!resolved && supported && params.value("resolve",true)) {
        const auto original=initial.items.at(index);auto request=original;request["cycodeLanguage"]=backend;
        auto future=language_.request("completionItem/resolve",std::move(request));lock.unlock();
        auto response=future.get();lock.lock();auto &session=checked();
        if (!response.is_object() || response.dump().size()>256*1024) throw std::runtime_error("Invalid or oversized resolved completion");
        for (const auto *key : {"label","sortText","filterText","insertText","textEdit","insertTextFormat"})
            if (response.contains(key) && (!original.contains(key) || response.at(key)!=original.at(key))) throw std::runtime_error(std::string("Resolved completion changed protected field: ")+key+"="+response.at(key).dump());
        auto merged=original;merged.update(response);
        // Keep the entire issued-session cache bounded even after lazy imports/docs.
        const auto oldSize=session.items.at(index).dump().size(),newSize=merged.dump().size();size_t total=0;
        for (const auto &[key,value] : completions_) total+=value.bytes;
        if (total-oldSize+newSize>8*1024*1024 || session.bytes-oldSize+newSize>4*1024*1024) throw std::runtime_error("Resolved completion cache exceeds its limit");
        session.bytes=session.bytes-oldSize+newSize;session.items[index]=std::move(merged);session.resolved.insert(index);resolved=true;
    }
    const auto &session=checked();const auto &item=session.items.at(index);
    if (method=="language.completionResolve") return {{"path",path},{"revision",session.revision},{"item",item},{"resolved",resolved}};
    if (!completionCommandAllowed(item,backend)) throw std::runtime_error("Completion requires an unsupported server command");
    auto &doc=documents_.at(path);
    if (readTextFileBytes(resolve(path))!=doc.disk) throw std::runtime_error("DiskConflict: completion source changed outside CyCode");
    auto edit=completionEdit(doc.text,item,session.position,params.value("replace",true));
    ChangePlan plan;plan.title="Complete "+item.at("label").get<std::string>();
    if (edit.after!=doc.text) plan.files.push_back({path,doc.text,std::move(edit.after),doc.disk,std::move(edit.edits),doc.revision});
    return {{"path",path},{"revision",doc.revision},{"plan",storeChange(std::move(plan))},{"cursor",edit.cursor},{"snippet",edit.snippet}};
}
}

#include "cycode/Engine.h"
#include "cycode/Completion.h"
#include "cycode/TextFile.h"
#include "cycode/TextEdits.h"
#include "cycode/SnippetCatalogue.h"
#include <algorithm>
namespace cycode {
Json Engine::issueCompletion(const std::string &path,uint64_t revision,uint64_t generation,uint64_t epoch,const Json &position,const Json &result,
                             bool local,std::string catalogue,std::optional<CompletionSelection> selection,uint64_t pluginRevision,std::string userRevision) {
    bool truncated=false;auto bounded=completionItems(result,&truncated);size_t bytes=bounded.dump().size()+catalogue.size()+userRevision.size();
    for (auto i=completions_.begin();i!=completions_.end();) {
        if (std::chrono::steady_clock::now()-i->second.created>std::chrono::minutes(5)) i=completions_.erase(i);else ++i;
    }
    auto cacheBytes=[&] { size_t n=0;for (const auto &[id,session] : completions_) n+=session.bytes;return n; };
    while (!completions_.empty() && (completions_.size()>=8 || cacheBytes()+bytes>8*1024*1024)) {
        auto oldest=std::min_element(completions_.begin(),completions_.end(),[](const auto &a,const auto &b) { return a.second.created<b.second.created; });completions_.erase(oldest);
    }
    const auto id="completion-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(nextCompletion_++);
    auto session=CompletionSession{path,revision,generation,epoch,position,bounded,{},bytes};
    session.local=local;session.catalogue=std::move(catalogue);session.selection=selection;
    session.snippetPluginRevision=pluginRevision;
    session.userSnippetRevision=std::move(userRevision);
    completions_.emplace(id,std::move(session));
    return {{"path",path},{"revision",revision},{"sessionId",id},{"truncated",truncated},
        {"result",{{"items",std::move(bounded)},{"isIncomplete",truncated || (result.is_object() && result.value("isIncomplete",false))}}}};
}
Json Engine::snippetCompletions(const Json &params) {
    std::unique_lock lock(mutex_);
    const auto requested=params.at("path").get<std::string>();
    if (requested.find("://")!=requested.npos || isUntitledDocument(requested))
        throw std::runtime_error("Insertion snippets require a saved workspace document");
    const auto path=pathUtf8(resolve(requested).lexically_relative(root_));
    const auto opened=documents_.find(path);
    if (opened==documents_.end()) throw std::runtime_error("Open the workspace document before requesting insertion snippets");
    auto *doc=&opened->second;
    if (params.at("expectedRevision")!=doc->revision) throw std::runtime_error("RevisionConflict: snippet source changed");
    const auto position=params.at("position");const auto at=utf16Offset(doc->text,position);
    std::optional<CompletionSelection> selection;
    if (params.contains("selection")) selection=CompletionSelection{
        utf16Offset(doc->text,params.at("selection").at("start")),
        utf16Offset(doc->text,params.at("selection").at("end"))};
    const auto selected=selectedSnippetText(doc->text,at,selection);
    if (!selected) throw std::runtime_error("Selection must be ordered, at most 8 KiB and end at the caret");
    const bool wrapping=!selected->empty();
    const auto before=doc->text;const auto revision=doc->revision,generation=nextRevision_;
    const auto language=snippetLanguage(path);
    auto source=readSnippetCatalogue(resolve(std::string(snippetCataloguePath)));
    lock.unlock();
    // Plugin callbacks may enter Engine while owning their registry lock. Never
    // acquire that lock while holding Engine's mutex; revision() is atomic.
    const auto plugins=plugins_.snippetSnapshot();
    const auto pluginRevision=plugins.at("revision").get<uint64_t>();
    const auto user=userSnippets_.snapshot();
    if(user.at("available")==true && user.at("valid")!=true)
        throw std::runtime_error("User snippet catalogue is invalid or unreadable; open Edit user snippets to repair it");
    const auto userRevision=user.at("revision").get<std::string>();
    auto result=mergedSnippetItems(source,plugins.at("catalogues"),language,
        user.at("available")==true ? user.at("text").get_ref<const std::string&>() : "{}");
    for (auto &item:result.at("items")) {
        if (wrapping) {
            bool deferred=false,used=false;
            const auto preview=plainSnippet(item.at("insertText").get_ref<const std::string&>(),
                {root_/utf8Path(path),root_,before,at,selection},&deferred,&used);
            item["cycodeSnippetAvailable"]=bool(preview) && used;
            item["cycodeSnippetReason"]=!preview ? "This snippet cannot wrap the current selection." :
                !used ? "This snippet does not use TM_SELECTED_TEXT." : "";
            item["textEdit"]={{"range",params.at("selection")},{"newText",item.at("insertText")}};
            continue;
        }
        const auto length=snippetPrefixBytes(before,at,item.at("filterText").get_ref<const std::string&>());
        const auto start=at-length;
        const auto newline=start ? before.rfind('\n',start-1) : std::string::npos;
        const auto lineStart=newline==std::string::npos ? 0 : newline+1;
        const Json begin={{"line",position.at("line")},{"character",LanguageClient::utf16Length(before.substr(lineStart,start-lineStart))}};
        item["textEdit"]={{"range",{{"start",begin},{"end",position}}},{"newText",item.at("insertText")}};
    }
    lock.lock();
    const auto current=documents_.find(path);
    if (current==documents_.end() || current->second.revision!=revision || nextRevision_!=generation)
        throw std::runtime_error("RevisionConflict: snippet source changed during catalogue preparation");
    if(plugins_.revision()!=pluginRevision)
        throw std::runtime_error("RevisionConflict: plugin snippets changed; request snippets again");
    if(userSnippets_.snapshot().at("revision")!=userRevision)
        throw std::runtime_error("RevisionConflict: user snippets changed; request snippets again");
    if (readSnippetCatalogue(resolve(std::string(snippetCataloguePath)))!=source)
        throw std::runtime_error("RevisionConflict: snippet catalogue changed; request snippets again");
    auto reply=issueCompletion(path,revision,generation,0,position,result,true,std::move(source),selection,pluginRevision,userRevision);
    reply["result"]["cycodePluginRevision"]=pluginRevision;
    reply["result"]["cycodeUserRevision"]=userRevision;
    reply["result"]["cycodeUserGeneration"]=userSnippets_.generation();
    reply["language"]=language;reply["catalogue"]=snippetCataloguePath;
    return reply;
}
Json Engine::userSnippetDraft(const Json &params) {
    std::unique_lock lock(mutex_);
    const auto requested=params.at("path").get<std::string>();
    if(requested.find("://")!=requested.npos || isUntitledDocument(requested))
        throw std::runtime_error("Snippet capture requires an open saved workspace document");
    const auto path=pathUtf8(resolve(requested).lexically_relative(root_));
    const auto found=documents_.find(path);
    if(found==documents_.end() || params.at("expectedRevision")!=found->second.revision)
        throw std::runtime_error("RevisionConflict: open the current source before capturing a snippet");
    const auto &doc=found->second;
    const CompletionSelection range{utf16Offset(doc.text,params.at("selection").at("start")),
        utf16Offset(doc.text,params.at("selection").at("end"))};
    const auto selection=selectedSnippetText(doc.text,range.end,range);
    if(!selection || selection->empty())throw std::runtime_error("Capture one nonempty ordered selection of at most 8 KiB");
    const auto source=std::string(*selection);const auto revision=doc.revision,generation=nextRevision_;
    lock.unlock();
    auto draft=userSnippets_.draft(source,snippetLanguage(path));
    lock.lock();const auto current=documents_.find(path);
    if(current==documents_.end() || current->second.revision!=revision || nextRevision_!=generation)
        throw std::runtime_error("RevisionConflict: source changed while preparing the snippet draft");
    draft["path"]=path;draft["revision"]=revision;return draft;
}
Json Engine::completionOperation(const std::string &method,const Json &params) {
    const auto id=params.at("sessionId").get<std::string>();const auto index=params.at("item").get<size_t>();
    std::unique_lock lock(mutex_);
    auto checked=[&]() -> CompletionSession& {
        auto found=completions_.find(id);
        if (found==completions_.end() || std::chrono::steady_clock::now()-found->second.created>std::chrono::minutes(5)) throw std::runtime_error("Completion session expired; request completion again");
        auto &session=found->second;
        Document *doc=nullptr;
        if (session.local) {
            const auto opened=documents_.find(session.path);
            if (opened==documents_.end()) throw std::runtime_error("Snippet source document was closed");
            doc=&opened->second;
        } else doc=semanticDocumentLocked(session.path).first;
        if (doc->revision!=session.revision || params.at("expectedRevision")!=session.revision || nextRevision_!=session.generation ||
            (!session.local && language_.epoch(session.path)!=session.epoch))
            throw std::runtime_error("RevisionConflict: completion context changed; request completion again");
        if (session.local && readSnippetCatalogue(resolve(std::string(snippetCataloguePath)))!=session.catalogue)
            throw std::runtime_error("RevisionConflict: snippet catalogue changed; request snippets again");
        if(session.local && plugins_.revision()!=session.snippetPluginRevision)
            throw std::runtime_error("RevisionConflict: plugin snippets changed; request snippets again");
        if(session.local && userSnippets_.snapshot().at("revision")!=session.userSnippetRevision)
            throw std::runtime_error("RevisionConflict: user snippets changed; request snippets again");
        if (index>=session.items.size()) throw std::runtime_error("Invalid completion item index");
        return session;
    };
    auto &initial=checked();const auto path=initial.path;
    if (method=="language.completionPrepare" && path.starts_with("sdk://")) throw std::runtime_error("SDK sources are read-only");
    bool resolved=initial.resolved.contains(index);
    const auto backend=initial.local ? std::string{} : language_.backend(path);
    Json server=Json::object();
    if (!initial.local) {
        const auto statuses=language_.status();
        server=statuses.contains("servers") ? statuses.at("servers").at(backend) : statuses;
    }
    const bool supported=!initial.local && server.value("methods",Json::object()).value("completionResolve",false);
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
    if (session.local && !item.value("cycodeSnippetAvailable",true))
        throw std::runtime_error(item.value("cycodeSnippetReason",std::string("Unavailable insertion snippet")));
    if (!completionCommandAllowed(item,backend)) throw std::runtime_error("Completion requires an unsupported server command");
    const auto &source=documents_.at(path);
    if (readTextFileBytes(resolve(path))!=source.disk) throw std::runtime_error("DiskConflict: completion source changed outside CyCode");
    // Rendering/regex work uses an immutable snapshot without retaining the
    // Engine lock. Revalidate the issued context and disk before storing a plan.
    const auto before=source.text;const auto insertion=item;const auto point=session.position;const auto selection=session.selection;
    lock.unlock();
    auto edit=completionEdit(before,insertion,point,params.value("replace",true),
                             {root_ / utf8Path(path), root_,{},0,selection});
    lock.lock();checked();
    const auto &doc=documents_.at(path);
    if (readTextFileBytes(resolve(path))!=doc.disk) throw std::runtime_error("DiskConflict: completion source changed during preparation");
    ChangePlan plan;plan.title="Complete "+insertion.at("label").get<std::string>();
    if (edit.after!=doc.text) plan.files.push_back({path,doc.text,std::move(edit.after),doc.disk,std::move(edit.edits),doc.revision});
    return {{"path",path},{"revision",doc.revision},{"plan",storeChange(std::move(plan))},{"cursor",edit.cursor},{"snippet",edit.snippet}};
}
}

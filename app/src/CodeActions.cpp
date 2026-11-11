#include "cycode/Error.h"
#include "cycode/Engine.h"
#include "cycode/TextEdits.h"
#include "cycode/ChangeAnnotations.h"
#include <algorithm>
namespace cycode {
static std::string actionText(std::string text) {
    for (auto &c : text) if (static_cast<unsigned char>(c)<32 || c==127) c=' ';
    if (text.size()>4096) { text.resize(4096);while (!text.empty() && (static_cast<unsigned char>(text.back())&0xc0)==0x80) text.pop_back();if (!text.empty() && static_cast<unsigned char>(text.back())>=0xc0) text.pop_back(); }
    return text;
}
static bool previewableCppTweak(const Json &action, const std::string &backend) {
    if (backend != "cpp" || !action.contains("command") || !action.at("command").is_object()) return false;
    const auto &command = action.at("command");
    if (command.value("command", "") != "clangd.applyTweak") return false;
    const auto arguments = command.value("arguments", Json());
    if (!arguments.is_array() || arguments.size() != 1 || !arguments[0].is_object()) return false;
    const auto &argument = arguments[0];
    if (!argument.contains("file") || !argument.at("file").is_string() || !argument.contains("selection")) return false;
    static const std::set<std::string> supported = {"ExtractFunction", "ExtractVariable", "InlineVariable",
        "ExpandMacro", "ExpandDeducedType", "DefineInline", "DefineOutline", "RemoveUsingNamespace",
        "ScopifyEnum", "SwapIfBranches"};
    return supported.contains(argument.value("tweakID", ""));
}
static bool passiveActionCommand(const Json &action,const std::string &backend) {
    if (!action.contains("command") || action.at("command").is_null()) return true;
    const auto &command=action.at("command");
    // TLS 5.0.1 attaches an acknowledgement even when there are no TS commands.
    // Its implementation only loops action.commands; this empty acknowledgement
    // can be omitted after applying the separately supplied workspace edit.
    if (backend!="typescript" || !command.is_object() || command.value("command","")!="_typescript.applyCodeActionCommand") return false;
    const auto arguments=command.value("arguments",Json());
    if (!arguments.is_array() || arguments.size()!=1 || !arguments[0].is_object() || !arguments[0].contains("action") || !arguments[0].at("action").is_object()) return false;
    const auto commands=arguments[0].at("action").value("commands",Json());return commands.is_null() || (commands.is_array() && commands.empty());
}
Json Engine::codeActionDescriptionLocked(const Json &action,size_t index,bool resolvable,const std::string &backend) {
    const bool tweak = previewableCppTweak(action, backend);
    const bool edit=action.contains("edit") && !action.at("edit").is_null();
    std::string reason;bool needsResolve=!edit && (resolvable || tweak);
    if (action.contains("disabled") && !action.at("disabled").is_null()) {
        reason=action.at("disabled").value("reason","Disabled by the language server");
        if (reason.empty()) reason="Disabled by the language server";
    }
    if (reason.empty() && !tweak && !passiveActionCommand(action,backend)) reason="This action requires an unsupported language-server command";
    if (reason.empty() && edit) {
        try {
            const auto files=workspaceEditFilesLocked(action.at("edit"));
            (void)normalizeChangeAnnotations(files,action.at("edit").value("changeAnnotations",Json::object()));
            for (const auto &[path,edits] : files.items()) {
                if (!edits.is_array()) throw std::runtime_error("Invalid code action text edits");
                for (const auto &entry : edits) if (entry.value("insertTextFormat",1)!=1) throw std::runtime_error("Snippet code action edits are not supported");
            }
        } catch (const std::exception &e) { reason=e.what(); }
    }
    if (reason.empty() && !edit && !resolvable && !tweak) reason="This action has no supported workspace edit";
    return {{"index",index},{"title",actionText(action.at("title"))},{"kind",actionText(action.value("kind",std::string{}))},
        {"preferred",action.value("isPreferred",false)},{"available",reason.empty()},{"needsResolve",reason.empty() && needsResolve},{"reason",actionText(reason)}};
}
Json Engine::codeActionOperation(const std::string &method,const Json &params) {
    std::unique_lock lock(mutex_);
    auto serverCanResolve=[&](const std::string &path) { const auto status=language_.status();return status.at("servers").at(language_.backend(path)).value("methods",Json::object()).value("codeActionResolve",false); };
    if (method=="language.codeActions") {
        const auto supplied=params.at("path").get<std::string>();if (supplied.starts_with("sdk://")) throw NativeError(ErrorCode::ReadOnly,"SDK sources are read-only");
        const auto path=pathUtf8(resolve(supplied).lexically_relative(root_));if (!language_.accepts(path)) throw std::runtime_error("No language backend for this file type");
        auto &doc=openLocked(path);const auto revision=doc.revision,epoch=language_.epoch(path);auto generation=documentGenerationLocked();
        if (params.at("expectedRevision")!=revision) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: document changed; read it again");
        const auto &range=params.at("range");if (utf16Offset(doc.text,range.at("start"))>utf16Offset(doc.text,range.at("end"))) throw std::runtime_error("Reversed code action range");
        const auto only=params.value("only",Json::array());if (!only.is_array() || only.size()>16) throw std::runtime_error("Code actions accept at most 16 kind filters");
        for (const auto &kind : only) if (!kind.is_string() || kind.get_ref<const std::string&>().empty() || kind.get_ref<const std::string&>().size()>128) throw std::runtime_error("Invalid code action kind filter");
        Json request={{"textDocument",{{"uri",LanguageClient::uri(resolve(path))}}},{"range",range},{"context",{{"diagnostics",Json::array()},{"triggerKind",1}}}};
        if (!only.empty()) request["context"]["only"]=only;
        Json items;bool truncated=false;bool resolvable=false;
        for (int pass=0;pass<3;++pass) {
            auto future=language_.request("textDocument/codeAction",request);lock.unlock();auto response=future.get();lock.lock();
            if (documents_.at(path).revision!=revision || documentGenerationLocked()!=generation || language_.epoch(path)!=epoch) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: code action context changed during request");
            if (response.is_null()) response=Json::array();if (!response.is_array()) throw std::runtime_error("Invalid code action list");
            items=Json::array();size_t bytes=2;resolvable=serverCanResolve(path);std::set<std::string> unopened;
            for (auto &action : response) {
                if (!action.is_object() || !action.contains("title") || !action.at("title").is_string()) { truncated=true;continue; }
                const auto size=action.dump().size()+1;if (size>512*1024) { truncated=true;continue; }
                if (items.size()>=256 || bytes+size>2*1024*1024) { truncated=true;break; }
                if (!only.empty() && action.contains("kind")) {
                    const auto kind=action.at("kind").get<std::string>();bool matches=false;
                    for (const auto &filter : only) { const auto prefix=filter.get<std::string>();matches|=kind==prefix || kind.starts_with(prefix+"."); }if (!matches) continue;
                }
                const auto description=codeActionDescriptionLocked(action,items.size(),resolvable,language_.backend(path));
                if (description.at("available")==true && action.contains("edit") && !action.at("edit").is_null()) {
                    const auto targets=workspaceEditFilesLocked(action.at("edit"));
                    for (const auto &[target,edits] : targets.items()) if (!documents_.contains(target)) unopened.insert(target);
                }
                bytes+=size;items.push_back(std::move(action));
            }
            if (unopened.empty()) break;
            if (unopened.size()>100 || pass==2) throw std::runtime_error("Code action targets changed; request actions again");
            for (const auto &target : unopened) openLocked(target);
            generation=documentGenerationLocked(); // Recompute edits only after new targets have live overlays.
        }
        const auto now=std::chrono::steady_clock::now();
        for (auto i=codeActions_.begin();i!=codeActions_.end();) { if (now-i->second.created>std::chrono::minutes(5)) i=codeActions_.erase(i);else ++i; }
        const auto bytes=items.dump().size();auto total=[&] { size_t n=bytes;for (const auto &[id,session] : codeActions_) n+=session.bytes;return n; };
        while (!codeActions_.empty() && (codeActions_.size()>=8 || total()>8*1024*1024)) {
            auto oldest=std::min_element(codeActions_.begin(),codeActions_.end(),[](const auto &a,const auto &b) { return a.second.created<b.second.created; });codeActions_.erase(oldest);
        }
        const auto id="actions-"+std::to_string(now.time_since_epoch().count())+"-"+std::to_string(nextAction_++);Json actions=Json::array();
        for (size_t i=0;i<items.size();++i) actions.push_back(codeActionDescriptionLocked(items.at(i),i,resolvable,language_.backend(path)));
        codeActions_.emplace(id,ActionSession{path,revision,generation,epoch,std::move(items),resolvable,bytes});
        return {{"path",path},{"revision",revision},{"sessionId",id},{"actions",std::move(actions)},{"truncated",truncated}};
    }
    const auto id=params.at("sessionId").get<std::string>();const auto index=params.at("item").get<size_t>();
    auto checked=[&]() -> ActionSession& {
        auto found=codeActions_.find(id);if (found==codeActions_.end() || std::chrono::steady_clock::now()-found->second.created>std::chrono::minutes(5)) throw std::runtime_error("Code action session expired; request actions again");
        auto &session=found->second;
        if (index>=session.items.size()) throw std::runtime_error("Invalid code action index");
        if (!documents_.contains(session.path) || documents_.at(session.path).revision!=session.revision || params.at("expectedRevision")!=session.revision || documentGenerationLocked()!=session.generation || language_.epoch(session.path)!=session.epoch) throw NativeError(ErrorCode::RevisionConflict,"RevisionConflict: code action context changed; request actions again");
        return session;
    };
    const auto &initial=checked();auto action=initial.items.at(index);auto description=codeActionDescriptionLocked(action,index,initial.resolvable,language_.backend(initial.path));
    if (description.at("available")!=true) throw std::runtime_error(description.at("reason").get<std::string>());
    if (previewableCppTweak(action, language_.backend(initial.path))) {
        if (!params.value("resolve", true)) throw std::runtime_error("This C++ transformation requires preparation");
        auto argument = action.at("command").at("arguments").at(0);
        if (LanguageClient::uriKey(argument.at("file")) != LanguageClient::uriKey(LanguageClient::uri(resolve(initial.path))))
            throw std::runtime_error("C++ transformation changed its source document");
        const auto &selection = argument.at("selection");
        const auto &sourceText = documents_.at(initial.path).text;
        if (utf16Offset(sourceText, selection.at("start")) > utf16Offset(sourceText, selection.at("end")))
            throw std::runtime_error("C++ transformation has a reversed selection");
        argument["cycodeLanguage"] = "cpp";
        auto future = language_.request("cycode/prepareTweak", std::move(argument));
        lock.unlock();
        auto edit = future.get();
        lock.lock();
        checked();
        action.erase("command");
        action["edit"] = std::move(edit);
        description = codeActionDescriptionLocked(action, index, false, "cpp");
        if (description.at("available") != true) throw std::runtime_error(description.at("reason").get<std::string>());
    } else if (description.at("needsResolve")==true) {
        if (!params.value("resolve",true)) throw std::runtime_error("This code action requires resolution");
        auto request=action;request["cycodeLanguage"]=language_.backend(initial.path);
        auto future=language_.request("codeAction/resolve",std::move(request));lock.unlock();auto resolved=future.get();lock.lock();checked();
        if (!resolved.is_object() || resolved.dump().size()>2*1024*1024) throw std::runtime_error("Invalid or oversized resolved code action");
        for (const auto &[key,value] : action.items()) if (key!="edit" && resolved.contains(key) && resolved.at(key)!=value) throw std::runtime_error("Resolved code action changed a protected field: "+key);
        action.update(resolved);description=codeActionDescriptionLocked(action,index,false,language_.backend(checked().path));
        if (description.at("available")!=true) throw std::runtime_error(description.at("reason").get<std::string>());
    }
    checked();const auto files=workspaceEditFilesLocked(action.at("edit"));std::vector<std::string> unopened;
    for (const auto &[target,edits] : files.items()) if (!documents_.contains(target)) unopened.push_back(target);
    if (!unopened.empty()) {
        for (const auto &target : unopened) openLocked(target);
        throw std::runtime_error("Code action targets were synchronized; request actions again before preparing this edit");
    }
    return workspaceChangeLocked(action.at("title"),action.at("edit"),"",true);
}
}

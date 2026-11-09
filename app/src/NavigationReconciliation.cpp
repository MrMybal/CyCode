#include "cycode/Engine.h"
#include "cycode/Error.h"
#include "cycode/TextEdits.h"
#include <functional>
#include <set>

namespace cycode {
Json Engine::reconcileNavigationTargets(Json response,const Json &sourceSymbols,uint64_t generation,uint64_t epoch) {
    // clangd's index may override an AST declaration with an old disk location.
    // Match compiler USRs in the target's own current AST, rather than searching
    // text, remapping speculative coordinates or guessing an overload.
    std::map<std::string,std::set<std::string>> symbols;
    const auto source=sourceSymbols.is_array()?sourceSymbols:Json::array({sourceSymbols});
    if(source.size()>64)return response;
    for(const auto &item:source)if(item.is_object() && item.value("name",Json()).is_string() && item.value("usr",Json()).is_string())
        symbols[item.at("name").get<std::string>()].insert(item.at("usr").get<std::string>());
    if(symbols.empty())return response;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
    auto guard=[&] {
        if(std::chrono::steady_clock::now()>deadline)
            throw NativeError(ErrorCode::DeadlineExceeded,"Compiler target reconciliation timed out; prepare the target and retry");
        if(nextRevision_!=generation || language_.backendEpoch("cpp")!=epoch)
            throw NativeError(ErrorCode::RevisionConflict,"Navigation target context changed during compiler lookup");
    };
    std::map<std::string,std::map<std::string,Json>> targets;
    std::map<std::string,std::set<std::string>> verifiedSelections;
    const auto rows=response.is_array()?response:Json::array({response});
    if(rows.size()>32)return response;
    for(const auto &row:rows) {
        if(!row.is_object())continue;
        const auto field=row.contains("targetUri")?"targetUri":"uri";
        if(!row.value(field,Json()).is_string())continue;
        const auto uri=row.at(field).get<std::string>();
        std::string path;
        try {path=pathUtf8(resolve(pathUtf8(LanguageClient::fromUri(uri))).lexically_relative(root_));}
        catch(const std::exception &) {continue;}
        if(targets.contains(uri))continue;
        std::future<Json> outline;
        {
            std::lock_guard lock(mutex_);guard();
            const auto found=documents_.find(path);
            if(found==documents_.end() || !LanguageService::supportsCpp(path))continue;
            outline=language_.request("textDocument/documentSymbol",{{"textDocument",{{"uri",uri}}}});
        }
        const auto tree=outline.get();
        std::vector<Json> candidates;size_t visited=0;bool overflow=false;
        std::function<void(const Json &,unsigned)> visit=[&](const Json &value,unsigned depth) {
            if(++visited>8192 || depth>64){overflow=true;return;}
            if(value.is_array()){for(const auto &entry:value){if(overflow)break;visit(entry,depth+1);}return;}
            if(!value.is_object())return;
            const auto name=value.value("name",Json());
            if(name.is_string() && symbols.contains(name.get<std::string>()) && value.contains("selectionRange")) {
                if(candidates.size()<64)candidates.push_back(value);else overflow=true;
            }
            if(value.contains("children"))visit(value.at("children"),depth+1);
        };
        auto &resolved=targets[uri];
        visit(tree,0);if(overflow)continue;
        std::set<std::string> ambiguous;
        for(const auto &candidate:candidates) {
            std::future<Json> identity;
            {
                std::lock_guard lock(mutex_);guard();
                const auto &text=documents_.at(path).text;
                const auto &selection=candidate.at("selectionRange");
                const auto a=utf16Offset(text,selection.at("start")),b=utf16Offset(text,selection.at("end"));
                if(b<=a || text.substr(a,b-a)!=candidate.at("name").get<std::string>())continue;
                (void)utf16Offset(text,candidate.at("range").at("start"));
                (void)utf16Offset(text,candidate.at("range").at("end"));
                identity=language_.request("textDocument/symbolInfo",{{"textDocument",{{"uri",uri}}},{"position",selection.at("start")}});
            }
            const auto answer=identity.get();
            const auto details=answer.is_array()?answer:Json::array({answer});
            for(const auto &item:details) {
                const auto name=candidate.at("name").get<std::string>();
                if(!item.is_object() || !item.value("usr",Json()).is_string())continue;
                const auto usr=item.at("usr").get<std::string>();
                if(usr.empty() || !symbols.at(name).contains(usr))continue;
                verifiedSelections[uri].insert(candidate.at("selectionRange").dump());
                if(resolved.contains(usr) && resolved.at(usr).at("selectionRange")!=candidate.at("selectionRange"))ambiguous.insert(usr);
                else resolved[usr]=candidate;
            }
        }
        for(const auto &usr:ambiguous)resolved.erase(usr);
    }
    {
        std::lock_guard lock(mutex_);guard();
    }
    auto reconcile=[&](Json &row) {
        if(!row.is_object())return;
        const bool link=row.contains("targetUri");
        const auto uri=row.value(link?"targetUri":"uri",std::string{});
        const auto found=targets.find(uri);
        if(found==targets.end())return;
        // Several current declarations of one USR do not invalidate a range
        // already confirmed by the target AST. Preserve that exact destination.
        if(verifiedSelections[uri].contains(row.at(link?"targetSelectionRange":"range").dump()))return;
        if(found->second.size()!=1) {row=nullptr;return;}
        const auto &candidate=found->second.begin()->second;
        row[link?"targetSelectionRange":"range"]=candidate.at("selectionRange");
        if(link)row["targetRange"]=candidate.at("range");
    };
    if(response.is_array())for(auto &row:response)reconcile(row);
    else reconcile(response);
    return response;
}
} // namespace cycode

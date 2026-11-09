#include "cycode/Engine.h"
#include "cycode/Error.h"
#include "cycode/TextEdits.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace cycode {
namespace {
std::filesystem::path compilerDatabase(const LanguageService &language,const std::filesystem::path &root) {
    auto directory=language.compilationDatabase("cpp");
    const auto config=language.configuration("cpp");
    if(config.contains("arguments")) {
        const auto &args=config.at("arguments");
        for(size_t i=0;i<args.size();++i) {
            const auto arg=args[i].get<std::string>();
            const auto prefix=arg.starts_with("--compile-commands-dir=")?23:arg.starts_with("-compile-commands-dir=")?22:0;
            if(prefix)directory=utf8Path(arg.substr(prefix));
            else if((arg=="--compile-commands-dir" || arg=="-compile-commands-dir") && i+1<args.size())directory=utf8Path(args[++i].get<std::string>());
        }
    }
    if(directory.empty())directory=root;
    if(directory.is_relative())directory=root/directory;
    return std::filesystem::weakly_canonical(directory/"compile_commands.json");
}
Json sourcePosition(const std::string &text,size_t at) {
    const auto begin=text.rfind('\n',at?at-1:0);
    const auto start=begin==std::string::npos?0:begin+1;
    return {{"line",std::count(text.begin(),text.begin()+at,'\n')},
            {"character",LanguageClient::utf16Length(text.substr(start,at-start))}};
}
bool identifierByte(unsigned char c) {return c>=128 || std::isalnum(c) || c=='_';}
}

std::string Engine::analysisCompilerContext() const {
    auto context=language_.configuration("cpp").dump();
    const auto file=compilerDatabase(language_,root_);
    context+=pathUtf8(file);
    std::error_code error;
    const auto stamp=std::filesystem::last_write_time(file,error);
    if(error)context+="missing";
    else {context+=std::to_string(stamp.time_since_epoch().count());context+=':';context+=std::to_string(std::filesystem::file_size(file,error));}
    return context;
}

Json Engine::analysisOperation(const std::string &method,const Json &params) {
    if(method=="analysis.prepare")return prepareAnalysis(params);
    std::lock_guard lock(analysisStateMutex_);
    if(method=="analysis.cancel") {
        const auto id=params.value("operationId",std::string{});
        const bool matched=analysisCancellation_ && analysisState_.at("state")=="running" && (id.empty() || id==analysisOperationId_);
        if(matched)analysisCancellation_->store(true);
        return {{"cancelled",matched}};
    }
    if(method=="analysis.results") {
        if(params.at("session")!=analysisSession_.id)throw NativeError(ErrorCode::RevisionConflict,"Analysis session was replaced; prepare again");
        const auto offset=params.value("offset",size_t{0}),limit=params.value("limit",size_t{128});
        if(limit<1 || limit>256 || offset>analysisSession_.records.size())throw NativeError(ErrorCode::InvalidParams,"Invalid analysis result page");
        Json rows=Json::array();
        const auto end=std::min(offset+limit,analysisSession_.records.size());
        for(size_t i=offset;i<end;++i)rows.push_back(analysisSession_.records.at(i));
        return {{"session",analysisSession_.id},{"documents",std::move(rows)},{"offset",offset},{"nextOffset",end},
            {"available",analysisSession_.records.size()},{"total",analysisSession_.paths.size()},{"hasMore",end<analysisSession_.records.size()}};
    }
    const auto id=params.value("operationId",std::string{});
    if(!id.empty() && id!=analysisOperationId_)return {{"state","idle"},{"phase","idle"},{"completed",0},{"total",0},{"progress",0},{"error",""}};
    return analysisState_;
}

Json Engine::prepareAnalysis(const Json &params,const std::string &symbol,const std::string &source,uint64_t *issuedGeneration,Json *renameEdits) {
    std::unique_lock operation(analysisMutex_,std::try_to_lock);
    if(!operation.owns_lock())throw NativeError(ErrorCode::BackendUnavailable,"Another semantic preparation is active; observe analysis.status and retry");
    const bool rename=renameEdits!=nullptr;
    const auto cancellation=std::make_shared<std::atomic<bool>>(false);
    AnalysisSession work;
    Json collected=Json::object();
    std::map<std::string,std::string> baselines;
    size_t renameBytes=0;
    std::string temporary;
    uint64_t temporaryId=0,temporaryRevision=0;
    {
        std::lock_guard lock(analysisStateMutex_);
        analysisOperationId_=params.value("operationId",std::string{});
        analysisCancellation_=cancellation;
        analysisState_={{"state","running"},{"phase","discovering"},{"completed",0},{"total",0},{"progress",0},{"error",""}};
    }
    auto checked=[&] {if(cancellation->load())throw NativeError(ErrorCode::Cancelled,"Semantic preparation cancelled");};
    auto cleanup=[&] {
        if(temporary.empty())return;
        std::lock_guard lock(mutex_);
        const auto found=documents_.find(temporary);
        if(analysisTemporaryDocuments_.erase(temporary) && found!=documents_.end() && found->second.identity==temporaryId &&
            found->second.revision==temporaryRevision && !found->second.dirty) {
            language_.close(temporary);documents_.erase(found);
        }
        temporary.clear();
    };
    auto publish=[&](const char *phase) {
        work.summary["observed"]=work.records.size();work.summary["remaining"]=work.paths.size()-work.records.size();
        std::lock_guard lock(analysisStateMutex_);
        analysisState_["phase"]=phase;analysisState_["completed"]=work.records.size();analysisState_["total"]=work.paths.size();
        analysisState_["progress"]=work.paths.empty()?0:std::min<size_t>(99,100*work.records.size()/work.paths.size());
        analysisState_["session"]=work.id;analysisState_["coverage"]=work.summary;
        analysisState_["resumable"]=!rename && work.discoveryComplete;
    };
    // A checkpoint is native compiler evidence, not a second job scheduler.
    // The official SDK owns each call and cancellation. Checkpoints live only
    // in this Core process and are invalidated by document/configuration/disk changes.
    auto checkpoint=[&] {std::lock_guard lock(analysisStateMutex_);analysisSession_=work;};
    auto guard=[&] {
        checked();
        if(nextRevision_!=work.generation)throw NativeError(ErrorCode::RevisionConflict,"A document changed during semantic preparation; read and prepare again");
        if(monitor_.snapshot().revision!=work.workspaceRevision)throw NativeError(ErrorCode::RevisionConflict,"Workspace observation changed during semantic preparation; prepare again");
        for(const auto &[backend,epoch]:work.epochs)if(language_.backendEpoch(backend)!=epoch)
            throw NativeError(ErrorCode::RevisionConflict,"Language configuration changed during semantic preparation");
    };
    auto request=[&](const std::string &method,const Json &data) {
        for(;;) {
            {std::lock_guard lock(mutex_);guard();}
            try {return language_.request(method,data,cancellation).get();}
            catch(const NativeError &error) {if(error.code()!=ErrorCode::DeadlineExceeded)throw;}
        }
    };
    try {
        const auto continuation=params.value("continuation",std::string{});
        if(!continuation.empty()) {
            if(rename || params.contains("paths") || params.contains("scope") || params.contains("pathPrefix"))throw NativeError(ErrorCode::InvalidParams,"Continue with the issued session ID and an optional batchSize only");
            std::lock_guard lock(analysisStateMutex_);
            if(continuation!=analysisSession_.id)throw NativeError(ErrorCode::RevisionConflict,"Analysis continuation expired or was replaced");
            work=analysisSession_;
            if(!work.discoveryComplete || work.scope=="workspace-spelling-candidates")
                throw NativeError(ErrorCode::InvalidParams,"This observation is not resumable; start a new preparation or global rename");
        } else {
            work.id="analysis-"+std::to_string(nextAnalysisSession_++);
            work.scope=rename?"workspace-spelling-candidates":params.value("scope",std::string("explicit-files"));
            work.summary={{"diagnosticClean",0},{"diagnosticErrors",0},{"diagnosticUnknown",0},{"commands",0},{"translationUnits",0},{"outOfScope",0}};
            {std::lock_guard lock(mutex_);work.generation=nextRevision_;work.workspaceRevision=monitor_.snapshot().revision;}
            if(issuedGeneration && work.generation!=*issuedGeneration)throw NativeError(ErrorCode::RevisionConflict,"Workspace changed before semantic preparation");
            std::set<std::string> paths;
            if(!source.empty())paths.insert(source);
            work.compilerContext=analysisCompilerContext();
            if(work.scope=="compilation-database") {
                if(params.contains("paths"))throw NativeError(ErrorCode::InvalidParams,"Compilation-database scope does not accept paths");
                const auto file=compilerDatabase(language_,root_);
                std::ifstream input(file);
                if(!input)throw NativeError(ErrorCode::BackendUnavailable,"No readable active C++ compilation database");
                const auto prefix=params.value("pathPrefix",std::string{});
                const auto folder=prefix.empty()?root_:resolve(prefix);
                if(!std::filesystem::is_directory(folder))throw NativeError(ErrorCode::InvalidParams,"pathPrefix must be an existing workspace directory");
                size_t commands=0,outside=0;
                std::set<std::string> allUnits;
                // Consume one command object at a time. Do not retain 37,000
                // expanded compiler argument lists merely to enumerate sources.
                const auto parsed=Json::parse(input,[&](int depth,Json::parse_event_t event,Json &value) {
                    checked();
                    if(depth!=1 || event!=Json::parse_event_t::object_end)return true;
                    if(!value.is_object() || !value.value("file",Json()).is_string() || !value.value("directory",Json()).is_string())
                        throw NativeError(ErrorCode::InvalidParams,"Invalid compilation command entry");
                    ++commands;
                    auto path=utf8Path(value.at("file").get<std::string>());
                    if(path.is_relative())path=utf8Path(value.at("directory").get<std::string>())/path;
                    path=std::filesystem::weakly_canonical(path);
                    if(!containsPath(root_,path) || !LanguageService::supportsCpp(pathUtf8(path))){++outside;return false;}
                    const auto key=pathUtf8(resolve(pathUtf8(path)).lexically_relative(root_));
                    allUnits.insert(key);
                    if(containsPath(folder,path))paths.insert(key);
                    return false;
                });
                if(!parsed.is_array())throw NativeError(ErrorCode::InvalidParams,"Compilation database must be an array");
                work.summary["commands"]=commands;work.summary["translationUnits"]=allUnits.size();
                work.summary["outOfScope"]=allUnits.size()-paths.size()+outside;
                work.epochs["cpp"]=language_.backendEpoch("cpp");
            } else if(!rename) {
                if(work.scope!="explicit-files" || !params.contains("paths") || !params.at("paths").is_array() || params.at("paths").empty())
                    throw NativeError(ErrorCode::InvalidParams,"Analysis requires explicit source paths or compilation-database scope");
                for(const auto &entry:params.at("paths")) {
                    const auto path=pathUtf8(resolve(entry.get<std::string>()).lexically_relative(root_));
                    if(!language_.accepts(path))throw NativeError(ErrorCode::UnsupportedOperation,"No configured language backend for "+path);
                    paths.insert(path);
                }
            } else {
                std::string pending;
                bool invalidPath=false;
                const std::vector<std::string> args={"rg","--files-with-matches","--null","--hidden","--no-ignore","--fixed-strings","--word-regexp",
                    "-g","*.{c,cc,cpp,cxx,h,hh,hpp,hxx,inl,ipp,tpp,ixx,cppm}","-g","!.git","-g","!runtime","-g","!data","-g","!.cycode","--",symbol,"."};
                const auto result=runProcessStreamed(args,root_,*cancellation,[&](std::string_view chunk) {
                    pending.append(chunk);
                    for(size_t at;(at=pending.find('\0'))!=std::string::npos;) {
                        auto value=pending.substr(0,at);pending.erase(0,at+1);
                        std::replace(value.begin(),value.end(),'\\','/');if(value.starts_with("./"))value.erase(0,2);
                        try {const auto path=pathUtf8(resolve(value).lexically_relative(root_));if(LanguageService::supportsCpp(path))paths.insert(path);}
                        catch(const std::exception &) {invalidPath=true;cancellation->store(true);}
                    }
                    if(pending.size()>32768){invalidPath=true;cancellation->store(true);}
                });
                if(invalidPath)throw NativeError(ErrorCode::FileSystemError,"A rename candidate could not be safely resolved");
                checked();
                if(result.exitCode>1 || !pending.empty())throw NativeError(ErrorCode::OperationFailed,"Rename discovery failed: "+result.output);
                std::lock_guard lock(mutex_);guard();
                for(const auto &[path,doc]:documents_)if(LanguageService::supportsCpp(path) && doc.text.find(symbol)!=std::string::npos)paths.insert(path);
            }
            work.discoveryComplete=true;
            work.paths.assign(paths.begin(),paths.end());
            if(rename) {
                const auto first=std::find(work.paths.begin(),work.paths.end(),source);
                if(first!=work.paths.end())std::rotate(work.paths.begin(),first,first+1);
            }
            for(const auto &path:work.paths)work.epochs.try_emplace(language_.backend(path),language_.backendEpoch(language_.backend(path)));
        }
        if(analysisCompilerContext()!=work.compilerContext)throw NativeError(ErrorCode::RevisionConflict,"Compiler database or configuration changed; prepare again");
        {std::lock_guard lock(mutex_);guard();}
        publish("analysing");checkpoint();
        std::set<std::string> usrs;
        const auto batch=params.value("batchSize",rename?work.paths.size():work.scope=="compilation-database"?size_t{64}:work.paths.size());
        if(!rename && params.contains("batchSize") && batch>4096)throw NativeError(ErrorCode::InvalidParams,"batchSize must be 0..4096");
        const auto end=std::min(work.paths.size(),work.records.size()+batch);
        for(size_t index=work.records.size();index<end;++index) {
            checked();
            const auto &path=work.paths[index];
            uint64_t revision,identity;
            std::string uri,text;
            {
                std::lock_guard lock(mutex_);guard();
                const bool owned=!documents_.contains(path);
                auto &doc=openLocked(path,false);work.generation=nextRevision_;
                revision=doc.revision;identity=doc.identity;text=doc.text;uri=LanguageClient::uri(resolve(path));
                if(owned){analysisTemporaryDocuments_.insert(path);temporary=path;temporaryId=identity;temporaryRevision=revision;}
                // Reopening only the LSP draft is deliberate for explicit C++
                // analysis: clangd suppresses identical-text diagnostic updates.
                // Native buffers, identities, revisions and UI ownership stay intact.
                language_.sync(path,doc.text,revision,identity,true,false,LanguageService::supportsCpp(path));
            }
            const Json document={{"textDocument",{{"uri",uri}}}};
            (void)request("textDocument/documentSymbol",document);
            if(rename && path==source) {
                const auto info=request("textDocument/symbolInfo",{{"textDocument",{{"uri",uri}}},{"position",params.at("position")}});
                const auto rows=info.is_array()?info:Json::array({info});
                for(const auto &row:rows)if(row.is_object() && row.value("usr",Json()).is_string() && !row.at("usr").get<std::string>().empty())usrs.insert(row.at("usr").get<std::string>());
                if(usrs.size()!=1)throw NativeError(ErrorCode::UnsupportedOperation,"Compiler did not identify one unambiguous C++ symbol; no partial preview issued");
            }
            // A compiler AST response and version-bound diagnostics are different
            // evidence. Unknown/errorful diagnostics never become a clean proof.
            std::string diagnostic="unknown";size_t errorCount=0;
            for(size_t attempt=0;attempt<100;++attempt) {
                checked();
                const auto status=language_.status();
                const auto backend=language_.backend(path);
                const auto server=status.contains("servers")?status.at("servers").at(backend):status;
                const auto publications=server.value("diagnosticRevisions",Json::object());
                if(publications.contains(path) && publications.at(path).value("revision",uint64_t{0})==revision && publications.at(path).value("id",uint64_t{0})==identity) {
                    const auto diagnostics=server.value("diagnostics",Json::object());
                    const auto publishedUri=publications.at(path).value("uri",uri);
                    if(diagnostics.contains(publishedUri)) {
                        for(const auto &item:diagnostics.at(publishedUri))if(item.value("severity",0)==1)++errorCount;
                        diagnostic=errorCount?"errors":"clean";break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if(rename) {
                bool matched=false;
                for(size_t at=0;(at=text.find(symbol,at))!=std::string::npos;at+=symbol.size()) {
                    if((at && identifierByte(text[at-1])) || (at+symbol.size()<text.size() && identifierByte(text[at+symbol.size()])))continue;
                    const auto position=sourcePosition(text,at);
                    const auto info=request("textDocument/symbolInfo",{{"textDocument",{{"uri",uri}}},{"position",position}});
                    const auto rows=info.is_array()?info:Json::array({info});
                    for(const auto &row:rows)if(row.is_object() && row.value("usr",Json()).is_string() && usrs.contains(row.at("usr").get<std::string>()))matched=true;
                    if(!matched)continue;
                    // Ask the real compiler at a confirmed occurrence in THIS
                    // current AST. Keep only this file's compiler-issued edits;
                    // other files are verified during their own rolling visit.
                    const auto edits=request("textDocument/rename",{{"textDocument",{{"uri",uri}}},{"position",position},{"newName",params.at("newName")}});
                    Json own=Json::array();
                    if(edits.is_object() && edits.contains("changes"))for(const auto &[target,list]:edits.at("changes").items())
                        if(std::filesystem::weakly_canonical(LanguageClient::fromUri(target))==resolve(path))own=list;
                    if(edits.is_object() && edits.contains("documentChanges"))for(const auto &change:edits.at("documentChanges")) {
                        if(!change.contains("textDocument"))throw NativeError(ErrorCode::UnsupportedOperation,"Rename requires unsupported file operations");
                        if(std::filesystem::weakly_canonical(LanguageClient::fromUri(change.at("textDocument").at("uri")))==resolve(path)) {
                            if(!own.empty())throw NativeError(ErrorCode::OperationFailed,"Duplicate compiler rename document");
                            own=change.at("edits");
                        }
                    }
                    if(own.empty())throw NativeError(ErrorCode::OperationFailed,"Compiler omitted the confirmed current rename file; no partial preview issued");
                    for(const auto &edit:own)if(edit.contains("annotationId"))throw NativeError(ErrorCode::UnsupportedOperation,"Annotated C++ rename requires adapter support");
                    (void)applyTextEdits(text,own,symbol);
                    renameBytes+=text.size()*2+own.dump().size();
                    if(renameBytes>16*1024*1024)throw NativeError(ErrorCode::CapacityExceeded,"Rename transaction exceeds 16 MiB; no partial preview issued");
                    collected[uri]=own;baselines[path]=text;
                    break;
                }
            }
            {
                std::lock_guard lock(mutex_);guard();
                const auto &doc=documents_.at(path);
                if(doc.identity!=identity || doc.revision!=revision)throw NativeError(ErrorCode::RevisionConflict,"An analysis document was replaced");
            }
            work.records.push_back({{"path",path},{"revision",revision},{"diagnostics",diagnostic},{"errors",errorCount}});
            const auto count=diagnostic=="clean"?"diagnosticClean":diagnostic=="errors"?"diagnosticErrors":"diagnosticUnknown";
            work.summary[count]=work.summary.value(count,size_t{0})+1;
            cleanup();publish("analysing");
        }
        if(analysisCompilerContext()!=work.compilerContext)throw NativeError(ErrorCode::RevisionConflict,"Compiler database changed during preparation");
        {
            std::lock_guard lock(mutex_);guard();
            if(rename) {
                if(work.summary.value("diagnosticErrors",size_t{0}) || work.summary.value("diagnosticUnknown",size_t{0}))
                    throw NativeError(ErrorCode::BackendUnavailable,"Some rename candidates lack clean version-bound compiler diagnostics; inspect analysis.results. No partial preview issued",{{"partialPreviewIssued",false},{"session",work.id},{"coverage",work.summary}});
                // Reopen only affected native buffers; validate the exact text
                // used by the per-file compiler, then let the normal transaction
                // validate all disk baselines before ANY apply.
                for(const auto &[path,before]:baselines) {
                    auto &doc=openLocked(path);work.generation=nextRevision_;
                    if(doc.text!=before)throw NativeError(ErrorCode::RevisionConflict,"A prepared rename file changed before preview");
                    if(!doc.diskConflict.empty())throw NativeError(ErrorCode::DiskConflict,"A prepared rename file conflicts with disk");
                }
                *renameEdits={{"changes",std::move(collected)}};
            }
            if(issuedGeneration)*issuedGeneration=work.generation;
        }
        const bool complete=work.records.size()==work.paths.size();
        publish(complete?"completed":"pending");checkpoint();
        std::lock_guard lock(analysisStateMutex_);
        analysisState_["state"]=complete?"completed":"pending";
        if(complete)analysisState_["progress"]=100;
        auto result=analysisState_;
        result["documents"]=Json::array();
        for(size_t i=0;i<std::min<size_t>(128,work.records.size());++i)result["documents"].push_back(work.records.at(i));
        result["scope"]=work.scope;result["wholeProjectIndexCompleteness"]="unknown";
        result["continuation"]=complete?"":work.id;
        result["nextOffset"]=result.at("documents").size();
        return result;
    } catch(const std::exception &error) {
        cleanup();
        if(!work.id.empty()){publish("failed");checkpoint();}
        std::lock_guard lock(analysisStateMutex_);
        const auto *native=dynamic_cast<const NativeError *>(&error);
        analysisState_["state"]=native && native->code()==ErrorCode::Cancelled?"cancelled":"failed";
        analysisState_["phase"]=analysisState_["state"];analysisState_["error"]=error.what();
        throw;
    }
}
} // namespace cycode

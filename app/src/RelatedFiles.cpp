#include "cycode/RelatedFiles.h"
#include "cycode/IncludeDirective.h"
#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include "cycode/TextEdits.h"
#include <algorithm>
#include <cwchar>
#include <set>
#include <vector>
namespace cycode {
namespace {
struct RelatedProbe {std::filesystem::path path;std::string relation;};
class RelatedProbes {
  public:
    RelatedProbes(std::filesystem::path root,const RelatedFileSnapshot &index)
        :truncated(index.state!=RelatedIndexState::ready),root_(std::move(root)){}
    void add(std::filesystem::path candidate,const char *relation) {
        candidate=candidate.lexically_normal();
        if(!containsPath(root_,candidate) || planned_.contains(candidate))return;
        if(!relatedProbeFits(probes_.size())){truncated=true;return;}
        planned_.insert(candidate);probes_.push_back({std::move(candidate),relation});
    }
    Json report(const RelatedFileSnapshot &index) const {
        namespace fs=std::filesystem;Json candidates=Json::array();size_t unreadable=0;
        std::set<fs::path,RelatedPathLess> seen;
        for(const auto &probe:probes_) {
            std::error_code error;const auto status=fs::symlink_status(probe.path,error);
            if(error) {if(error!=std::errc::no_such_file_or_directory && error!=std::errc::not_a_directory)++unreadable;continue;}
            if(!fs::is_regular_file(status) || fs::is_symlink(status))continue;
            const auto canonical=fs::canonical(probe.path,error);
            if(error){++unreadable;continue;}
            if(!containsPath(root_,canonical) || !seen.insert(canonical).second)continue;
            candidates.push_back({{"path",pathUtf8(canonical.lexically_relative(root_))},{"relation",probe.relation}});
        }
        return {{"candidates",std::move(candidates)},{"checked",probes_.size()},{"unreadable",unreadable},{"truncated",truncated},
                {"index",{{"state",relatedIndexName(index.state)},{"revision",index.revision},{"generation",index.generation}}}};
    }
    bool truncated;
  private:
    std::filesystem::path root_;
    std::vector<RelatedProbe> probes_;
    std::set<std::filesystem::path,RelatedPathLess> planned_;
};
bool includeSuffix(const std::filesystem::path &path,const std::filesystem::path &target) {
    const auto candidate=path.lexically_normal(),suffix=target.lexically_normal();
    auto a=candidate.end(),b=suffix.end();const RelatedPathLess less;
    while(b!=suffix.begin()) {
        if(a==candidate.begin())return false;
        --a;--b;
        if(*b==".." || less(*a,*b) || less(*b,*a))return false;
    }
    return !suffix.empty() && suffix!=".";
}
}
bool RelatedPathLess::operator()(const std::filesystem::path &a,const std::filesystem::path &b) const {
#ifdef _WIN32
        return _wcsicmp(a.c_str(),b.c_str())<0;
#else
        return a<b;
#endif
}
void RelatedFileIndex::rebuild(const nlohmann::json &paths) {
    RelatedFileIndex next;
    for(size_t i=0;i<paths.size();++i) {
        if(!paths[i].is_string()){next.incomplete_=true;continue;}
        std::filesystem::path path;
        try {path=utf8Path(paths[i].get_ref<const std::string &>());}
        catch(const std::exception &){next.incomplete_=true;continue;}
        if(path.empty() || paths[i].get_ref<const std::string &>().find('\0')!=std::string::npos || path.has_root_path() ||
           std::any_of(path.begin(),path.end(),[](const auto &part){return part=="..";})) {
            next.incomplete_=true;continue;
        }
        const auto kind=cppFileKind(pathUtf8(path.extension()));
        auto &group=next.paths_[path.stem()];
        auto &bucket=kind==CppFileKind::header?group.headers:kind==CppFileKind::source?group.sources:group.other;
        if(relatedProbeFits(bucket.offsets.size()))bucket.offsets.push_back(i);
        else bucket.truncated=true;
    }
    *this=std::move(next);
}
const RelatedFileIndex::Entries &RelatedFileIndex::entries(const Bucket &bucket,CppFileKind kind) {
    return kind==CppFileKind::header?bucket.headers:kind==CppFileKind::source?bucket.sources:bucket.other;
}
RelatedFileSnapshot RelatedFileIndex::find(const nlohmann::json &paths,const std::filesystem::path &source) const {
    RelatedFileSnapshot result;result.state=incomplete_?RelatedIndexState::partial:RelatedIndexState::ready;
    const auto kind=cppFileKind(pathUtf8(source.extension()));
    if(kind==CppFileKind::none || relatedGeneratedStem(pathUtf8(source.stem())))return result;
    const auto found=paths_.find(source.stem());if(found==paths_.end())return result;
    const auto &bucket=entries(found->second,kind==CppFileKind::header?CppFileKind::source:CppFileKind::header);
    if(bucket.truncated)result.state=RelatedIndexState::partial;
    for(auto offset:bucket.offsets)result.paths.push_back(utf8Path(paths.at(offset).get_ref<const std::string &>()));
    return result;
}
RelatedFileSnapshot RelatedFileIndex::findInclude(const nlohmann::json &paths,const std::filesystem::path &target) const {
    RelatedFileSnapshot result;result.state=incomplete_?RelatedIndexState::partial:RelatedIndexState::ready;
    if(target.has_root_path())return result;
    const auto normalized=target.lexically_normal();
    if(std::any_of(target.begin(),target.end(),[](const auto &part){return part=="..";}))return result;
    const auto found=paths_.find(normalized.stem());if(found==paths_.end())return result;
    const auto &bucket=entries(found->second,cppFileKind(pathUtf8(normalized.extension())));
    if(bucket.truncated)result.state=RelatedIndexState::partial;
    for(auto offset:bucket.offsets) {
        auto path=utf8Path(paths.at(offset).get_ref<const std::string &>());
        if(includeSuffix(path,normalized))result.paths.push_back(std::move(path));
    }
    return result;
}
nlohmann::json cppCounterparts(const std::filesystem::path &workspace,const std::filesystem::path &source,
                              const RelatedFileSnapshot &index) {
    namespace fs=std::filesystem;using Json=nlohmann::json;
    const auto root=fs::canonical(workspace),file=fs::canonical(source);
    const auto kind=cppFileKind(pathUtf8(file.extension()));const auto stem=pathUtf8(file.stem());
    if(!containsPath(root,file) || !fs::is_regular_file(file) || kind==CppFileKind::none || relatedGeneratedStem(stem))
        throw std::runtime_error("Header/source switching requires a regular non-generated C/C++ workspace file");
    RelatedProbes probes(root,index);
    const auto add=[&](const fs::path &directory,const char *relation) {
        for(auto extension:{".cpp",".cc",".cxx",".c",".mm",".m",".h",".hh",".hpp",".hxx",".inl",
                            ".CPP",".CC",".CXX",".C",".MM",".M",".H",".HH",".HPP",".HXX",".INL"}) {
            if(cppFileKind(extension)==kind)continue;
            probes.add(directory/utf8Path(stem+extension),relation);
        }
    };
    add(file.parent_path(),"sibling");
    auto directory=file.parent_path();size_t levels=0;bool pairedLayout=false;
    while(containsPath(root,directory) && levels++<16) {
        const auto name=pathUtf8(directory.filename());
        std::vector<const char*> targets;
        if(relatedNameEquals(name,"public") || relatedNameEquals(name,"classes"))targets={"Private","private"};
        else if(relatedNameEquals(name,"private"))targets={"Public","public","Classes","classes"};
        else if(relatedNameEquals(name,"include") || relatedNameEquals(name,"includes"))targets={"src","Src","source","Source"};
        else if(relatedNameEquals(name,"src") || relatedNameEquals(name,"source"))targets={"include","Include","includes","Includes"};
        const auto suffix=file.parent_path().lexically_relative(directory);
        for(auto target:targets) {
            const auto paired=directory.parent_path()/target;
            add(paired/suffix,"paired-directory");
            if(suffix!="." && !suffix.empty())add(paired,"paired-root");
        }
        if(!targets.empty()){pairedLayout=true;break;}
        if(directory==root)break;
        const auto parent=directory.parent_path();if(parent==directory)break;directory=parent;
    }
    if(!pairedLayout && directory!=root && levels>=16)probes.truncated=true;
    if(relatedIndexCurrent(index.state))for(const auto &path:index.paths) {
        if(path.has_root_path())continue;
        const auto candidateKind=cppFileKind(pathUtf8(path.extension()));
        const RelatedPathLess less;const auto candidateStem=path.stem(),sourceStem=file.stem();
        if(candidateKind==CppFileKind::none || candidateKind==kind || less(candidateStem,sourceStem) || less(sourceStem,candidateStem))continue;
        probes.add(root/path,"workspace-index");
    }
    return probes.report(index);
}
Json cppIncludedFiles(const std::filesystem::path &workspace,const std::filesystem::path &source,
                      std::string_view target,bool angled,const RelatedFileSnapshot &index) {
    namespace fs=std::filesystem;const auto root=fs::canonical(workspace),file=fs::canonical(source);
    if(!containsPath(root,file) || !fs::is_regular_file(file) || cppFileKind(pathUtf8(file.extension()))==CppFileKind::none)
        throw std::runtime_error("Include navigation requires a regular C/C++ workspace file");
    const auto path=utf8Path(std::string(target));RelatedProbes probes(root,index);
    if(!includeTargetFits(target.size()) || path.has_root_path() ||
       std::any_of(target.begin(),target.end(),[](unsigned char c){return c<32 || c==127 || c==':' || c=='\\';}))
        throw std::runtime_error("Include navigation requires a relative literal header path");
    if(!angled)probes.add(file.parent_path()/path,"source-relative");
    if(relatedIndexCurrent(index.state))for(const auto &candidate:index.paths)
        if(!candidate.has_root_path() && includeSuffix(candidate,path))probes.add(root/candidate,"workspace-index");
    return probes.report(index);
}
Json Engine::relatedFiles(const Json &params,bool included) {
    std::unique_lock lock(mutex_);const auto requested=params.at("path").get<std::string>();
    if(requested.find("://")!=requested.npos || isUntitledDocument(requested))
        throw std::runtime_error("Header/source switching requires an open saved workspace file");
    const auto absolute=resolve(requested);const auto path=pathUtf8(absolute.lexically_relative(root_));
    const auto found=documents_.find(path);
    if(found==documents_.end() || params.at("expectedRevision")!=found->second.revision ||
        params.at("expectedDocumentId")!=found->second.identity)
        throw std::runtime_error("RevisionConflict: header/source navigation context changed");
    const auto revision=found->second.revision,identity=found->second.identity;
    constexpr size_t prefixBudget=1024*1024+8192+1;
    const auto text=included?found->second.text.substr(0,prefixBudget):std::string();
    lock.unlock();
    Json directive;std::string target;bool angled=false;
    if(included) {
        const auto caret=utf16Offset(text,params.at("position"));const auto parsed=includeDirectiveAt(text,caret);
        if(!parsed)throw std::runtime_error("No supported literal #include in this directive (1 MiB prefix / 8 KiB directive limits)");
        target=parsed->target;angled=parsed->angled;
        const auto column=[&](size_t byte){return LanguageClient::utf16Length(text.substr(parsed->lineStart,byte-parsed->lineStart));};
        const auto line=std::count(text.begin(),text.begin()+parsed->lineStart,'\n');
        directive={{"target",target},{"angled",angled},{"range",{{"start",{{"line",line},{"character",column(parsed->start)}}},
                                                            {"end",{{"line",line},{"character",column(parsed->end)}}}}}};
    }
    RelatedFileSnapshot index;
    {
        std::unique_lock cacheLock(filesMutex_,std::try_to_lock);
        if(!cacheLock.owns_lock())index.state=RelatedIndexState::busy;
        else if(bool(filesSnapshot_)) {
            index.revision=filesIndexRevision_;index.generation=filesCacheGeneration_;
            if(filesIndexRevision_!=monitor_.snapshot().indexRevision)index.state=RelatedIndexState::stale;
            else {
                auto matches=included?filesSnapshot_->related.findInclude(filesSnapshot_->files,utf8Path(target)):
                                      filesSnapshot_->related.find(filesSnapshot_->files,absolute);
                index.paths=std::move(matches.paths);index.state=matches.state;
                if(!filesSnapshot_->complete)index.state=RelatedIndexState::partial;
            }
        }
    }
    auto result=included?cppIncludedFiles(root_,absolute,target,angled,index):cppCounterparts(root_,absolute,index);
    lock.lock();
    const auto current=documents_.find(path);
    if(current==documents_.end() || current->second.revision!=revision || current->second.identity!=identity)
        throw std::runtime_error("RevisionConflict: header/source source was changed or closed");
    if(relatedIndexCurrent(index.state)) {
        std::unique_lock cacheLock(filesMutex_,std::try_to_lock);
        if(!cacheLock.owns_lock() || !relatedIndexFresh(index.state,index.revision,index.generation,
                                                      monitor_.snapshot().indexRevision,filesCacheGeneration_)) {
            result["index"]["state"]="changed";result["truncated"]=true;
        }
    }
    result["path"]=path;result["revision"]=revision;result["documentId"]=identity;
    if(included){result["include"]=std::move(directive);result["position"]=params.at("position");}
    return result;
}
}

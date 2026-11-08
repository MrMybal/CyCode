#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>
namespace cycode {
enum class CppFileKind { none, header, source };
constexpr bool relatedNameEquals(std::string_view a,std::string_view b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i) {
        auto c=a[i];if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        if(c!=b[i])return false;
    }
    return true;
}
constexpr CppFileKind cppFileKind(std::string_view extension) {
    for(auto x:{".h",".hh",".hpp",".hxx",".inl"})if(relatedNameEquals(extension,x))return CppFileKind::header;
    for(auto x:{".cpp",".cc",".cxx",".c",".mm",".m"})if(relatedNameEquals(extension,x))return CppFileKind::source;
    return CppFileKind::none;
}
constexpr bool relatedGeneratedStem(std::string_view stem) {
    return (stem.size()>=10 && relatedNameEquals(stem.substr(stem.size()-10),".generated")) ||
           (stem.size()>=4 && relatedNameEquals(stem.substr(stem.size()-4),".gen"));
}
constexpr bool relatedProbeFits(size_t count) {return count<128;}
struct RelatedPathLess {
    bool operator()(const std::filesystem::path &,const std::filesystem::path &) const;
};
enum class RelatedIndexState { missing, busy, stale, ready, partial, changed };
constexpr std::string_view relatedIndexName(RelatedIndexState state) {
    switch(state) {
        case RelatedIndexState::missing:return "missing";
        case RelatedIndexState::busy:return "busy";
        case RelatedIndexState::stale:return "stale";
        case RelatedIndexState::ready:return "ready";
        case RelatedIndexState::partial:return "partial";
        case RelatedIndexState::changed:return "changed";
    }
    return "missing";
}
constexpr bool relatedIndexCurrent(RelatedIndexState state) {
    return state==RelatedIndexState::ready || state==RelatedIndexState::partial;
}
constexpr bool fileIndexPageMatches(uint64_t revision,uint64_t generation,uint64_t observedRevision,uint64_t observedGeneration) {
    return revision!=0 && generation!=0 && revision==observedRevision && generation==observedGeneration;
}
constexpr bool relatedIndexFresh(RelatedIndexState state,uint64_t revision,uint64_t generation,
                                 uint64_t observedRevision,uint64_t observedGeneration) {
    return relatedIndexCurrent(state) && fileIndexPageMatches(revision,generation,observedRevision,observedGeneration);
}
constexpr bool relatedAutoOpen(size_t candidates,size_t unreadable,bool truncated,bool choose=false) {
    return !choose && candidates==1 && unreadable==0 && !truncated;
}
constexpr std::string_view relatedHelpAnchor(bool included,bool choose=false) {
    return included?"open-include":choose?"related-results":"header-source";
}
struct RelatedFileSnapshot {
    RelatedIndexState state=RelatedIndexState::missing;
    uint64_t revision=0,generation=0;
    std::vector<std::filesystem::path> paths;
};
// Offsets into the existing sorted workspace file cache; never owns another scan.
class RelatedFileIndex {
  public:
    void rebuild(const nlohmann::json &paths);
    RelatedFileSnapshot find(const nlohmann::json &paths,const std::filesystem::path &source) const;
    RelatedFileSnapshot findInclude(const nlohmann::json &paths,const std::filesystem::path &target) const;
  private:
    struct Entries {std::vector<size_t> offsets;bool truncated=false;};
    struct Bucket {Entries headers,sources,other;};
    static const Entries &entries(const Bucket &,CppFileKind);
    std::map<std::filesystem::path,Bucket,RelatedPathLess> paths_;
    bool incomplete_=false;
};
// Bounded metadata probes plus an optional cached-index snapshot; no recursive scan.
nlohmann::json cppCounterparts(const std::filesystem::path &workspace,const std::filesystem::path &source,
                              const RelatedFileSnapshot &index={});
nlohmann::json cppIncludedFiles(const std::filesystem::path &workspace,const std::filesystem::path &source,
                               std::string_view target,bool angled,const RelatedFileSnapshot &index={});
}

#include "cycode/text/Search.h"
#include <algorithm>
#include <stdexcept>
namespace cycode::text {
namespace {
unsigned char fold(unsigned char c) { return c>='A' && c<='Z' ? c+('a'-'A') : c; }
bool word(unsigned char c) { return c>=128 || c=='_' || (c>='0' && c<='9') || (fold(c)>='a' && fold(c)<='z'); }
}
TextMatches findTextMatches(std::string_view text,std::string_view query,bool sensitive,bool words,size_t limit,size_t begin,size_t end) {
    TextMatches result;if (query.empty()) return result;
    if (query.size()>4096) throw std::runtime_error("Search query must contain 1 to 4096 bytes");
    end=std::min(end,text.size());begin=std::min(begin,end);size_t line=1,column=1,scanned=0;
    auto equal=[&](char a,char b) { return sensitive ? a==b : fold(a)==fold(b); };
    std::vector<size_t> prefix(query.size());
    for (size_t i=1,j=0;i<query.size();++i) { while (j && !equal(query[i],query[j])) j=prefix[j-1];if (equal(query[i],query[j])) ++j;prefix[i]=j; }
    for (size_t at=begin,matched=0;at<end;++at) {
        while (matched && !equal(text[at],query[matched])) matched=prefix[matched-1];
        if (equal(text[at],query[matched])) ++matched;
        if (matched!=query.size()) continue;
        const auto offset=at+1-query.size();matched=0;
        if (!words || ((!offset || !word(text[offset-1])) && (offset+query.size()==text.size() || !word(text[offset+query.size()])))) {
            if (result.matches.size()>=limit) { result.truncated=true;break; }
            while (scanned<offset) { if (text[scanned++]=='\n') { ++line;column=1; } else ++column; }
            result.matches.push_back({offset,query.size(),line,column});
        }
    }
    return result;
}
std::string expandedSearchText(std::string_view text,bool extended) {
    if (!extended) return std::string(text);
    std::string out;
    for (size_t i=0;i<text.size();++i) {
        if (text[i]!='\\' || i+1==text.size()) { out+=text[i];continue; }
        const auto c=text[++i];
        if (c=='n') out+='\n';else if (c=='r') out+='\r';else if (c=='t') out+='\t';else if (c=='\\') out+='\\';
        else { out+='\\';out+=c; }
    }
    return out;
}
}

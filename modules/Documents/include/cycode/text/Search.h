#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace cycode::text {
struct TextMatch { size_t offset,length,line,column; };
struct TextMatches { std::vector<TextMatch> matches; bool truncated=false; };
TextMatches findTextMatches(std::string_view text,std::string_view query,bool sensitive,bool words,size_t limit=2000,size_t begin=0,size_t end=std::string_view::npos);
std::string expandedSearchText(std::string_view text,bool extended);
}

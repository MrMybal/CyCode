#include "cycode/Snippet.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
struct Node { std::string text;std::optional<uint32_t> index;std::vector<Node> children;std::vector<std::string> choices;bool definition=false; };
struct Parser {
    std::string source;size_t at=0,nodes=0;
    std::vector<Node> parse(int depth=0,bool nested=false) {
        if (depth>16) throw std::runtime_error("Snippet nesting exceeds 16");
        std::vector<Node> output;std::string text;
        auto flush=[&] { if (!text.empty()) { Node n;n.text=std::move(text);output.push_back(std::move(n));text.clear(); } };
        while (at<source.size()) {
            auto c=source[at++];
            if (c=='}' && nested) { flush();return output; }
            if (c=='\\') {
                if (at<source.size() && (source[at]=='\\' || source[at]=='$' || source[at]=='}')) text+=source[at++];
                else text+='\\';
                continue;
            }
            if (c!='$') { text+=c;continue; }
            flush();if (++nodes>2048) throw std::runtime_error("Too many snippet nodes");
            const bool braced=at<source.size() && source[at]=='{';if (braced) ++at;
            const auto start=at;uint32_t index=0;
            while (at<source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) {
                index=index*10+source[at++]-'0';if (index>65535) throw std::runtime_error("Snippet index exceeds 65535");
            }
            if (at==start) throw std::runtime_error("Snippet variables/transforms are not supported");
            Node n;n.index=index;
            if (braced) {
                if (at==source.size()) throw std::runtime_error("Unclosed snippet field");
                const auto kind=source[at++];
                if (kind==':') { n.definition=true;n.children=parse(depth+1,true); }
                else if (kind=='|') {
                    n.definition=true;std::string choice;bool closed=false;
                    while (at<source.size()) {
                        const auto ch=source[at++];
                        if (ch=='\\' && at<source.size() && (source[at]=='\\' || source[at]==',' || source[at]=='|')) choice+=source[at++];
                        else if (ch==',') { n.choices.push_back(std::move(choice));choice.clear(); }
                        else if (ch=='|' && at<source.size() && source[at]=='}') { ++at;n.choices.push_back(std::move(choice));closed=true;break; }
                        else choice+=ch;
                        if (n.choices.size()>64) throw std::runtime_error("Too many snippet choices");
                    }
                    if (!closed || n.choices.size()>64) throw std::runtime_error("Invalid snippet choice");
                } else if (kind!='}') throw std::runtime_error("Unsupported snippet transform");
            }
            output.push_back(std::move(n));
        }
        if (nested) throw std::runtime_error("Unclosed snippet placeholder");flush();return output;
    }
};
}
std::optional<Snippet> expandSnippet(const std::string &source) {
    if (source.size()>8192) return {};
    try {
        Parser parser{normalizeEditorText(source)};const auto tree=parser.parse();
        std::map<uint32_t,const Node*> definitions;
        auto collect=[&](auto &&self,const std::vector<Node> &nodes) -> void {
            for (const auto &n : nodes) { if (n.index && n.definition) definitions.try_emplace(*n.index,&n);self(self,n.children); }
        };collect(collect,tree);
        Snippet result;std::map<uint32_t,SnippetField> fields;std::set<uint32_t> expanding;std::vector<uint32_t> parents;size_t occurrences=0;bool final=false;
        auto render=[&](auto &&self,const std::vector<Node> &nodes,int depth) -> void {
            if (depth>16) throw std::runtime_error("Snippet expansion nesting exceeds 16");
            for (const auto &node : nodes) {
                if (!node.index) result.text+=node.text;
                else {
                    const auto index=*node.index;const auto start=result.text.size();
                    if (!expanding.insert(index).second) throw std::runtime_error("Recursive snippet field");
                    const auto def=definitions.find(index);const auto &value=def==definitions.end() ? node : *def->second;
                    parents.push_back(index);
                    if (!value.choices.empty()) result.text+=value.choices.front();else self(self,value.children,depth+1);
                    parents.pop_back();
                    expanding.erase(index);
                    if (index) {
                        auto &field=fields[index];field.index=index;field.choices=value.choices;field.ranges.push_back({start,result.text.size(),parents});
                        if (++occurrences>256 || field.ranges.size()>128 || fields.size()>128) throw std::runtime_error("Too many snippet fields");
                    } else if (!final) { final=true;result.finalByte=start; }
                }
                if (result.text.size()>65536) throw std::runtime_error("Snippet expansion exceeds 64 KiB");
            }
        };render(render,tree,0);
        if (!final) result.finalByte=result.text.size();
        for (auto &[index,field] : fields) {
            std::sort(field.ranges.begin(),field.ranges.end(),[](const auto &a,const auto &b) { return a.start<b.start; });
            for (size_t i=1;i<field.ranges.size();++i) if (field.ranges[i].start<field.ranges[i-1].end || (field.ranges[i].start==field.ranges[i-1].start && field.ranges[i].end==field.ranges[i-1].end)) throw std::runtime_error("Coincident snippet mirrors are unsupported");
            result.fields.push_back(std::move(field));
        }
        return result;
    } catch (const std::exception &) { return {}; }
}
void indentSnippet(Snippet &snippet,const std::string &indent) {
    const auto &text=snippet.text;if (text.find('\n')==std::string::npos) return;
    std::optional<std::string> common;
    for (size_t start=text.find('\n')+1;start<text.size();) {
        auto end=text.find('\n',start);if (end==std::string::npos) end=text.size();
        auto body=start;while (body<end && (text[body]==' ' || text[body]=='\t')) ++body;
        if (body<end) {
            const auto leading=text.substr(start,body-start);
            if (!common) common=leading;else { size_t n=0;while (n<common->size() && n<leading.size() && (*common)[n]==leading[n]) ++n;common->resize(n); }
        }
        start=end+1;
    }
    std::vector<size_t> mapping(text.size()+1);std::string after;
    for (size_t start=0;start<text.size();) {
        auto end=text.find('\n',start);if (end==std::string::npos) end=text.size();
        const auto remove=start && common && text.compare(start,common->size(),*common)==0 ? std::min(common->size(),end-start) : 0;
        if (start) after+=indent;
        for (size_t i=start;i<=start+remove;++i) mapping[i]=after.size();
        for (size_t i=start+remove;i<end;++i) { mapping[i]=after.size();after+=text[i];mapping[i+1]=after.size(); }
        if (end<text.size()) { after+='\n';mapping[end+1]=after.size(); }
        if (after.size()>65536) throw std::runtime_error("Indented snippet exceeds 64 KiB");start=end+1;
    }
    for (auto &field : snippet.fields) for (auto &range : field.ranges) { range.start=mapping.at(range.start);range.end=mapping.at(range.end); }
    snippet.finalByte=mapping.at(snippet.finalByte);snippet.text=std::move(after);
}
nlohmann::json snippetMetadata(const Snippet &snippet,size_t base) {
    nlohmann::json fields=nlohmann::json::array();
    for (const auto &field : snippet.fields) {
        nlohmann::json ranges=nlohmann::json::array();for (const auto &range : field.ranges) ranges.push_back({{"start",base+range.start},{"end",base+range.end},{"parents",range.parents}});
        fields.push_back({{"index",field.index},{"ranges",std::move(ranges)},{"choices",field.choices}});
    }
    return {{"positionEncoding","utf-8-bytes"},{"fields",std::move(fields)},{"final",base+snippet.finalByte}};
}
}

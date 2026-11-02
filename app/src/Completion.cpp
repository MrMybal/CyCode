#include "cycode/Completion.h"
#include "cycode/Snippet.h"
#include <cctype>
#include <map>
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
namespace cycode {
std::optional<std::string> plainSnippet(const std::string &source) {
    const auto snippet=expandSnippet(source);return snippet ? std::optional<std::string>(snippet->text) : std::nullopt;
}
using Json=nlohmann::json;
Json completionItems(const Json &result,bool *truncated) {
    if (truncated) *truncated=false;
    if (result.is_null()) return Json::array();
    const auto &source=result.is_array() ? result : result.at("items");
    if (!source.is_array()) throw std::runtime_error("Invalid completion list");
    const auto defaults=result.is_object() ? result.value("itemDefaults",Json::object()) : Json::object();
    if (!defaults.is_object()) throw std::runtime_error("Invalid completion defaults");
    Json items=Json::array();size_t bytes=2;
    for (const auto &entry : source) {
        if (items.size()>=2000) { if (truncated) *truncated=true;break; }
        auto item=entry;
        if (!item.is_object() || !item.contains("label") || !item.at("label").is_string()) throw std::runtime_error("Invalid completion item");
        for (const auto *key : {"commitCharacters","insertTextFormat","insertTextMode","data"}) if (!item.contains(key) && defaults.contains(key)) item[key]=defaults.at(key);
        if (!item.contains("textEdit") && defaults.contains("editRange")) {
            const auto &range=defaults.at("editRange");
            const auto text=item.value("textEditText",item.value("insertText",item.at("label").get<std::string>()));
            if (range.contains("insert") && range.contains("replace")) item["textEdit"]={{"insert",range.at("insert")},{"replace",range.at("replace")},{"newText",text}};
            else item["textEdit"]={{"range",range},{"newText",text}};
        }
        const auto size=item.dump().size()+1;
        if (size>256*1024) { if (truncated) *truncated=true;continue; }
        if (bytes+size>4*1024*1024) { if (truncated) *truncated=true;break; }
        bytes+=size;items.push_back(std::move(item));
    }
    return items;
}
bool completionCommandAllowed(const Json &item,const std::string &backend) {
    if (!item.contains("command") || item.at("command").is_null()) return true;
    if (!item.at("command").is_object()) return false;
    const auto command=item.at("command").value("command","");
    return command=="editor.action.triggerSuggest" || command=="editor.action.triggerParameterHints" ||
           (backend=="java" && command=="java.completion.onDidSelect");
}
static Json completionPosition(const std::string &text,size_t offset) {
    const auto lineStart=offset ? text.rfind('\n',offset-1) : std::string::npos;
    const auto start=lineStart==std::string::npos ? 0 : lineStart+1;
    return {{"line",std::count(text.begin(),text.begin()+offset,'\n')},{"character",LanguageClient::utf16Length(text.substr(start,offset-start))}};
}
CompletionEdit completionEdit(const std::string &before,const Json &item,const Json &position,bool replace) {
    const auto at=utf16Offset(before,position);
    size_t begin=at,end=at;
    auto word=[](unsigned char c) { return c>=128 || std::isalnum(c) || c=='_' || c=='$'; };
    while (begin && word(static_cast<unsigned char>(before[begin-1]))) --begin;
    if (replace) while (end<before.size() && word(static_cast<unsigned char>(before[end]))) ++end;
    Json range={{"start",completionPosition(before,begin)},{"end",completionPosition(before,end)}};
    auto text=item.value("insertText",item.value("label",std::string{}));
    if (item.contains("textEdit")) {
        const auto &edit=item.at("textEdit");text=edit.at("newText");
        if (edit.contains("range")) range=edit.at("range");
        else {
            const auto &insert=edit.at("insert"),&replacement=edit.at("replace");
            const auto a=utf16Offset(before,insert.at("start")),b=utf16Offset(before,insert.at("end"));
            const auto c=utf16Offset(before,replacement.at("start")),d=utf16Offset(before,replacement.at("end"));
            if (a!=c || b>d || a>at || at>b || insert.at("start").at("line")!=insert.at("end").at("line") || replacement.at("start").at("line")!=replacement.at("end").at("line")) throw std::runtime_error("Invalid insert/replace completion range");
            range=replace ? replacement : insert;
        }
    }
    begin=utf16Offset(before,range.at("start"));end=utf16Offset(before,range.at("end"));
    if (begin>at || end<at || range.at("start").at("line")!=position.at("line") || range.at("end").at("line")!=position.at("line")) throw std::runtime_error("Completion range does not contain the request position");
    const auto mode=item.value("insertTextMode",1);
    if (mode!=1 && mode!=2) throw std::runtime_error("Invalid completion indentation mode");
    std::optional<Snippet> snippet;
    if (item.value("insertTextFormat",1)==2) { snippet=expandSnippet(text);if (!snippet) throw std::runtime_error("Unsupported completion snippet");text=snippet->text; }
    else if (item.value("insertTextFormat",1)!=1) throw std::runtime_error("Invalid completion insert format");
    text=normalizeEditorText(text);if (text.size()>65536) throw std::runtime_error("Completion insertion exceeds 64 KiB");
    if (mode==2 && text.find('\n')!=std::string::npos) {
        const auto newline=at ? before.rfind('\n',at-1) : std::string::npos;const auto lineStart=newline==std::string::npos ? 0 : newline+1;
        auto prefixEnd=lineStart;while (prefixEnd<at && (before[prefixEnd]==' ' || before[prefixEnd]=='\t')) ++prefixEnd;
        auto adjusted=snippet.value_or(Snippet{text,{},text.size()});indentSnippet(adjusted,before.substr(lineStart,prefixEnd-lineStart));
        text=adjusted.text;if (snippet) snippet=std::move(adjusted);
    }
    auto edits=item.value("additionalTextEdits",Json::array());
    if (!edits.is_array() || edits.size()>256) throw std::runtime_error("Completion exceeds 256 additional edits");
    int64_t base=static_cast<int64_t>(begin);size_t bytes=text.size();
    for (auto &edit : edits) {
        auto extra=normalizeEditorText(edit.at("newText"));bytes+=extra.size();edit["newText"]=extra;
        if (bytes>1024*1024) throw std::runtime_error("Completion edits exceed 1 MiB");
        const auto start=utf16Offset(before,edit.at("range").at("start")),finish=utf16Offset(before,edit.at("range").at("end"));
        if (finish<start) throw std::runtime_error("Reversed completion edit range");
        if (finish<=begin && start!=begin) base+=static_cast<int64_t>(extra.size())-static_cast<int64_t>(finish-start);
    }
    edits.push_back({{"range",range},{"newText",text}});
    auto applied=applyTextEdits(before,edits);applied.text=normalizeEditorText(applied.text);
    const auto cursor=base+static_cast<int64_t>(snippet ? snippet->finalByte : text.size());
    if (base<0 || cursor<0 || static_cast<size_t>(cursor)>applied.text.size()) throw std::runtime_error("Invalid completion cursor");
    return {applied.edits,completionPosition(applied.text,static_cast<size_t>(cursor)),std::move(applied.text),snippet ? snippetMetadata(*snippet,static_cast<size_t>(base)) : Json()};
}
}

#include "cycode/Highlighting.h"
#include "cycode/Engine.h"
#include <limits>
#include <string_view>
namespace cycode {
using Json=nlohmann::json;
static uint32_t integer(const Json &value) {
    if (!value.is_number_integer() || value.get<int64_t>()<0 || value.get<uint64_t>()>2147483647) throw std::runtime_error("Invalid semantic token integer");
    return value.get<uint32_t>();
}
Json decodeHighlights(const std::string &text,const Json &reply) {
    Json output={{"supported",reply.at("supported")},{"generation",reply.at("generation")},{"tokens",Json::array()},{"truncated",false}};
    if (!reply.at("supported").get<bool>()) return output;
    const auto &legend=reply.at("legend"); const auto &types=legend.at("tokenTypes"), &modifiers=legend.at("tokenModifiers");
    if (!types.is_array() || types.size()>256 || !modifiers.is_array() || modifiers.size()>31) throw std::runtime_error("Invalid semantic token legend");
    for (const auto &names : {types,modifiers}) for (const auto &name : names)
        if (!name.is_string() || name.get_ref<const std::string&>().empty() || name.get_ref<const std::string&>().size()>128) throw std::runtime_error("Invalid semantic token name");
    const auto result=reply.value("result",Json());
    const auto data=result.is_null() ? Json::array() : result.at("data");
    if (!data.is_array() || data.size()%5 || data.size()>1000000) throw std::runtime_error("Invalid semantic token data");
    std::vector<std::string_view> lines; size_t start=0;
    for (size_t i=0;i<=text.size();++i) if (i==text.size() || text[i]=='\n') { lines.emplace_back(text.data()+start,i-start); start=i+1; }
    uint64_t line=0, character=0, previousEnd=0; size_t cursorLine=std::numeric_limits<size_t>::max(), byte=0, units=0, glyph=0, budget=0;
    for (size_t i=0;i<data.size();i+=5) {
        const auto delta=integer(data[i]); line+=delta; character=delta ? integer(data[i+1]) : character+integer(data[i+1]);
        const auto length=integer(data[i+2]), type=integer(data[i+3]), bits=integer(data[i+4]);
        if (!length || line>=lines.size() || type>=types.size() || (modifiers.size()<31 && (bits>>modifiers.size()))) throw std::runtime_error("Invalid semantic token range/type");
        if (cursorLine!=line) { cursorLine=size_t(line); byte=units=glyph=0; previousEnd=0; }
        if (character<previousEnd) throw std::runtime_error("Overlapping semantic tokens");
        const auto source=lines[size_t(line)];
        auto advance=[&](uint64_t target) {
            while (units<target && byte<source.size()) {
                const auto c=static_cast<unsigned char>(source[byte]); const size_t size=c<0x80 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
                if (byte+size>source.size()) throw std::runtime_error("Invalid source UTF-8");
                units+=c>=0xf0 ? 2 : 1; byte+=size; ++glyph;
            }
            if (units!=target) throw std::runtime_error("Semantic token splits UTF-16 or exceeds a line");
        };
        advance(character); const auto first=glyph; advance(character+length); previousEnd=character+length;
        Json names=Json::array(); for (size_t bit=0;bit<modifiers.size();++bit) if (bits&(uint32_t(1)<<bit)) names.push_back(modifiers[bit]);
        Json token={{"line",line},{"character",character},{"length",length},{"glyphStart",first},{"glyphEnd",glyph},{"type",types[type]},{"modifiers",names}};
        const auto bytes=token.dump().size();
        if (output.at("tokens").size()>=50000 || budget+bytes>4*1024*1024) { output["truncated"]=true; break; }
        budget+=bytes; output["tokens"].push_back(std::move(token));
    }
    return output;
}
Json Engine::highlight(const Json &params) {
    const auto path=params.at("path").get<std::string>();
    if (!LanguageService::supports(path)) throw std::runtime_error("No language backend for this file type");
    std::string text; uint64_t revision, snapshot; std::future<Json> pending;
    {
        std::lock_guard lock(mutex_); const auto [document,full]=semanticDocumentLocked(path);
        revision=document->revision; snapshot=nextRevision_;
        if (params.at("expectedRevision").get<uint64_t>()!=revision) throw std::runtime_error("RevisionConflict: document changed");
        text=document->text; pending=language_.request("cycode/semanticTokens",{{"textDocument",{{"uri",LanguageClient::uri(full)}}}});
    }
    auto result=decodeHighlights(text,pending.get());
    {
        std::lock_guard lock(mutex_); const auto [document,full]=semanticDocumentLocked(path);
        if (document->revision!=revision || snapshot!=nextRevision_) throw std::runtime_error("RevisionConflict: documents changed during highlighting");
    }
    result["path"]=path; result["revision"]=revision; return result;
}
}

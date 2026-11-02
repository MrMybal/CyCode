#include "cycode/LanguageInfo.h"
#include "cycode/TextEdits.h"
#include <algorithm>
namespace cycode {
namespace {
using Json=nlohmann::json;
struct TextBudget {
    size_t remaining=65536;
    bool truncated=false;
    std::string take(const Json &value,size_t maximum=65536) {
        if (!value.is_string()) throw std::runtime_error("Invalid language information text");
        const auto &source=value.get_ref<const std::string &>();
        (void)Json(source).dump(); // Reject malformed UTF-8 from custom backends.
        size_t count=std::min({source.size(),maximum,remaining});
        if (count<source.size()) {
            truncated=true;
            while (count && (static_cast<unsigned char>(source[count])&0xc0)==0x80) --count;
        }
        auto text=source.substr(0,count);remaining-=count;
        for (char &c : text) if ((static_cast<unsigned char>(c)<32 && c!='\n' && c!='\r' && c!='\t') || c==127) c=' ';
        return text;
    }
    Json sections(const Json &value,const char *stringKind) {
        Json output=Json::array();
        auto append=[&](const Json &part) {
            std::string kind=stringKind,language;Json text;
            if (part.is_null()) return;
            if (part.is_string()) text=part;
            else if (part.is_object() && part.contains("value")) {
                text=part.at("value");
                if (part.contains("kind")) {
                    kind=part.at("kind").get<std::string>();
                    if (kind!="plaintext" && kind!="markdown") throw std::runtime_error("Invalid language documentation format");
                } else if (part.contains("language")) { kind="code";language=take(part.at("language"),64); }
                else throw std::runtime_error("Invalid language documentation content");
            } else throw std::runtime_error("Invalid language documentation content");
            auto content=take(text);if (!content.empty()) output.push_back({{"kind",kind},{"language",language},{"text",std::move(content)}});
        };
        if (value.is_array()) {
            size_t visited=0;
            for (const auto &part : value) { if (visited++>=32 || !remaining) { truncated=true;break; } append(part); }
        } else append(value);
        return output;
    }
};
size_t labelByte(const std::string &label,const Json &value) {
    if (!value.is_number_integer() || value.get<int64_t>()<0) throw std::runtime_error("Invalid signature parameter offset");
    const auto wanted=value.get<uint64_t>();size_t at=0,units=0;
    while (units<wanted && at<label.size()) {
        const auto c=static_cast<unsigned char>(label[at]);units+=c>=0xf0 ? 2 : 1;
        at+=c<0x80 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
    }
    if (units!=wanted) throw std::runtime_error("Signature parameter offset is outside label or splits a UTF-16 surrogate pair");
    return at;
}
int activeIndex(const Json &object,const char *key,size_t count,int fallback=0) {
    if (!count) return -1;
    const auto index=object.value(key,Json(fallback));
    if (!index.is_number_integer() || index.get<int64_t>()<0 || index.get<uint64_t>()>=count) return 0;
    return index.get<int>();
}
}
nlohmann::json hoverInformation(const nlohmann::json &reply,const std::string &document) {
    TextBudget budget;Json result={{"available",false},{"contents",Json::array()},{"range",nullptr},{"truncated",false}};
    if (reply.is_null()) return result;
    if (!reply.is_object() || !reply.contains("contents")) throw std::runtime_error("Invalid hover response");
    result["contents"]=budget.sections(reply.at("contents"),"markdown");
    if (reply.contains("range") && !reply.at("range").is_null()) {
        const auto &range=reply.at("range");
        const auto start=utf16Offset(document,range.at("start")),end=utf16Offset(document,range.at("end"));
        if (end<start) throw std::runtime_error("Hover range is reversed");
        auto position=[](const Json &value) { return Json{{"line",value.at("line")},{"character",value.at("character")}}; };
        result["range"]={{"start",position(range.at("start"))},{"end",position(range.at("end"))}};
    }
    result["available"]=!result.at("contents").empty();result["truncated"]=budget.truncated;return result;
}
nlohmann::json signatureInformation(const nlohmann::json &reply) {
    TextBudget budget;Json signatures=Json::array();
    if (!reply.is_null()) {
        if (!reply.is_object() || !reply.contains("signatures") || !reply.at("signatures").is_array()) throw std::runtime_error("Invalid signature help response");
        for (const auto &source : reply.at("signatures")) {
            if (signatures.size()>=32 || !budget.remaining) { budget.truncated=true;break; }
            const auto &full=source.at("label").get_ref<const std::string &>();
            const auto label=budget.take(source.at("label"),16384);
            Json parameters=Json::array();
            const auto supplied=source.value("parameters",Json::array());if (!supplied.is_array()) throw std::runtime_error("Invalid signature parameters");
            size_t search=0;
            for (const auto &parameter : supplied) {
                if (parameters.size()>=128 || !budget.remaining) { budget.truncated=true;break; }
                const auto &raw=parameter.at("label");size_t start=std::string::npos,end=std::string::npos;std::string name;
                if (raw.is_string()) {
                    name=budget.take(raw,16384);start=label.find(name,search);if (start!=std::string::npos) end=start+name.size();
                } else if (raw.is_array() && raw.size()==2) {
                    start=labelByte(full,raw[0]);end=labelByte(full,raw[1]);
                    if (end<start) throw std::runtime_error("Signature parameter range is reversed");
                    if (end>label.size()) { budget.truncated=true;break; }
                    name=budget.take(label.substr(start,end-start),16384);
                } else throw std::runtime_error("Invalid signature parameter label");
                if (end!=std::string::npos) search=end;
                parameters.push_back({{"label",name},{"startByte",start==std::string::npos ? -1 : static_cast<int64_t>(start)},
                    {"endByte",end==std::string::npos ? -1 : static_cast<int64_t>(end)},
                    {"documentation",budget.sections(parameter.value("documentation",Json()),"plaintext")}});
            }
            // Preserve an unreported/null parameter instead of highlighting a
            // fabricated first argument (notably csharp-ls 0.28's last argument).
            const auto reported=source.contains("activeParameter") ? source.at("activeParameter") : reply.value("activeParameter",Json());
            const auto selected=reported.is_null() ? -1 : activeIndex(Json{{"index",reported}},"index",parameters.size());
            signatures.push_back({{"label",label},{"documentation",budget.sections(source.value("documentation",Json()),"plaintext")},
                {"parameters",parameters},{"activeParameter",selected}});
        }
    }
    const auto active=activeIndex(reply.is_object() ? reply : Json::object(),"activeSignature",signatures.size());
    return {{"available",!signatures.empty()},{"signatures",signatures},{"activeSignature",active},{"truncated",budget.truncated}};
}
}

#include "cycode/text/Encoding.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace cycode::text {
namespace {
constexpr std::array<unsigned,32> cp1252={0x20ac,0,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0,0x17d,0,0,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0,0x17e,0x178};
void append(std::string &out,unsigned cp) {
    if (cp<0x80) out+=char(cp);
    else if (cp<0x800) { out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63)); }
    else if (cp<0x10000) { out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63)); }
    else { out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63)); }
}
}
const char *encodingLabel(const std::string &encoding) {
    if (encoding=="utf-8") return "UTF-8";if (encoding=="utf-8-bom") return "UTF-8 BOM";
    if (encoding=="utf-16le") return "UTF-16 LE BOM";if (encoding=="utf-16be") return "UTF-16 BE BOM";
    if (encoding=="windows-1252") return "Windows-1252";throw std::runtime_error("Unsupported text encoding");
}
DecodedText decodeText(const std::string &bytes,const std::string &requested) {
    auto encoding=requested;size_t start=0;
    if (encoding=="auto") encoding=bytes.starts_with("\xff\xfe") ? "utf-16le" : bytes.starts_with("\xfe\xff") ? "utf-16be" : bytes.starts_with("\xef\xbb\xbf") ? "utf-8-bom" : "utf-8";
    encodingLabel(encoding);std::string text;
    if (encoding=="utf-8" || encoding=="utf-8-bom") { text=bytes;if (text.starts_with("\xef\xbb\xbf")) text.erase(0,3); }
    else if (encoding=="windows-1252") {
        for (unsigned char c : bytes) { unsigned cp=c;if (c>=0x80 && c<0xa0) { cp=cp1252[c-0x80];if (!cp) throw std::runtime_error("Undefined Windows-1252 byte"); }append(text,cp); }
    } else {
        const bool little=encoding=="utf-16le";if ((little && bytes.starts_with("\xff\xfe")) || (!little && bytes.starts_with("\xfe\xff"))) start=2;
        if ((bytes.size()-start)%2) throw std::runtime_error("Truncated UTF-16 code unit");
        auto unit=[&](size_t at) { return unsigned(static_cast<unsigned char>(bytes[at+(little ? 0 : 1)])) | (unsigned(static_cast<unsigned char>(bytes[at+(little ? 1 : 0)]))<<8); };
        for (size_t i=start;i<bytes.size();i+=2) {
            unsigned cp=unit(i);
            if (cp>=0xd800 && cp<=0xdbff) { if (i+3>=bytes.size()) throw std::runtime_error("Truncated UTF-16 surrogate pair");const auto low=unit(i+2);if (low<0xdc00 || low>0xdfff) throw std::runtime_error("Invalid UTF-16 surrogate pair");cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);i+=2; }
            else if (cp>=0xdc00 && cp<=0xdfff) throw std::runtime_error("Unexpected UTF-16 low surrogate");append(text,cp);
        }
    }
    validateEditorText(text);return {text,encoding};
}
std::string encodeText(const std::string &text,const std::string &encoding) {
    validateEditorText(text);encodingLabel(encoding);
    if (encoding=="utf-8") return text;if (encoding=="utf-8-bom") { if (text.size()+3>maxTextFileBytes) throw std::runtime_error("Encoded file exceeds 4 MiB");return "\xef\xbb\xbf"+text; }
    std::string bytes;if (encoding=="utf-16le") bytes="\xff\xfe";else if (encoding=="utf-16be") bytes="\xfe\xff";
    const bool little=encoding=="utf-16le";
    auto unit=[&](unsigned value) { bytes+=char(little ? value&255 : value>>8);bytes+=char(little ? value>>8 : value&255); };
    for (size_t i=0;i<text.size();) {
        const auto c=static_cast<unsigned char>(text[i++]);unsigned cp=c;const auto tail=c>=0xf0 ? 3 : c>=0xe0 ? 2 : c>=0xc0 ? 1 : 0;
        if (tail) { cp=c&((1u<<(6-tail))-1);for (int j=0;j<tail;++j) cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&63); }
        if (encoding=="windows-1252") {
            if (cp<128 || (cp>=0xa0 && cp<=255)) bytes+=char(cp);
            else { auto found=std::find(cp1252.begin(),cp1252.end(),cp);if (found==cp1252.end() || !cp) throw std::runtime_error("Character cannot be represented in Windows-1252");bytes+=char(0x80+(found-cp1252.begin())); }
        } else if (cp<0x10000) unit(cp);
        else { cp-=0x10000;unit(0xd800+(cp>>10));unit(0xdc00+(cp&1023)); }
    }
    if (bytes.size()>maxTextFileBytes) throw std::runtime_error("Encoded file exceeds 4 MiB");return bytes;
}
}

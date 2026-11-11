#pragma once
#include <optional>
#include <string_view>
namespace cycode {
constexpr bool includePrefixFits(size_t bytes){return bytes<=1024*1024;}
constexpr bool includeLineFits(size_t bytes){return bytes<=8192;}
constexpr bool includeTargetFits(size_t bytes){return bytes>0 && bytes<=1024;}
constexpr bool includeSpace(char c){return c==' ' || c=='\t' || c=='\r' || c=='\v' || c=='\f';}
// Physical offsets are retained; escaped newlines are skipped only outside raw strings.
constexpr size_t includeLogicalIndex(std::string_view text,size_t at) {
    while(at<text.size() && text[at]=='\\') {
        if(at+1<text.size() && text[at+1]=='\n')at+=2;
        else if(at+2<text.size() && text[at+1]=='\r' && text[at+2]=='\n')at+=3;
        else break;
    }
    return at;
}
constexpr bool includePrefixIsCode(std::string_view text,size_t end) {
    enum class State {code,block,line,string,character,raw};
    State state=State::code;bool whitespace=true;std::string_view delimiter;
    for(size_t i=0;i<end;) {
        if(state==State::raw) {
            if(text[i]==')' && i+delimiter.size()+1<text.size() &&
               text.substr(i+1,delimiter.size())==delimiter && text[i+delimiter.size()+1]=='"') {
                i+=delimiter.size()+2;state=State::code;
            } else ++i;
            continue;
        }
        i=includeLogicalIndex(text,i);if(i>=end)break;
        const char c=text[i];const auto next=includeLogicalIndex(text,i+1);
        const char n=next<text.size()?text[next]:'\0';
        if(state==State::line) {if(c=='\n'){state=State::code;whitespace=true;}++i;continue;}
        if(state==State::block) {
            if(c=='*' && n=='/'){state=State::code;i=next+1;}
            else {if(c=='\n')whitespace=true;++i;}
            continue;
        }
        if(state==State::string || state==State::character) {
            if(c=='\n')return false;
            if(c=='\\'){i=next<text.size()?next+1:text.size();continue;}
            if(c==(state==State::string?'"':'\''))state=State::code;
            ++i;continue;
        }
        if(c=='\n'){whitespace=true;++i;continue;}
        if(includeSpace(c)){++i;continue;}
        if(c=='/' && (n=='*' || n=='/')){state=n=='*'?State::block:State::line;i=next+1;continue;}
        whitespace=false;
        if(c=='R' && n=='"') {
            // Conservative raw-delimiter subset; an unknown spelling refuses navigation.
            const auto begin=next+1;auto open=begin;
            while(open<text.size() && text[open]!='(' && open-begin<=16) {
                const auto ch=static_cast<unsigned char>(text[open]);
                if(ch<=32 || ch>126 || ch==')' || ch=='\\' || ch=='"')return false;
                ++open;
            }
            if(open>=text.size() || open-begin>16 || text[open]!='(')return false;
            delimiter=text.substr(begin,open-begin);state=State::raw;i=open+1;continue;
        }
        if(c=='"' || c=='\''){state=c=='"'?State::string:State::character;++i;continue;}
        if(c>='0' && c<='9') {
            // A preprocessing number owns its apostrophe digit separators.
            char previous=c;++i;
            while(i<end) {
                i=includeLogicalIndex(text,i);if(i>=end)break;
                const char digit=text[i];
                const bool number=(digit>='0'&&digit<='9') || (digit>='A'&&digit<='Z') ||
                    (digit>='a'&&digit<='z') || digit=='_' || digit=='.' || digit=='\'' ||
                    ((digit=='+'||digit=='-')&&(previous=='e'||previous=='E'||previous=='p'||previous=='P'));
                if(!number)break;
                previous=digit;++i;
            }
            continue;
        }
        ++i;
    }
    return state==State::code && whitespace;
}
struct IncludeDirective {std::string_view target;size_t start=0,end=0,lineStart=0;bool angled=false;};
constexpr size_t includePhysicalBegin(std::string_view text,size_t at) {
    const auto previous=at?text.rfind('\n',at-1):text.npos;
    return previous==text.npos?0:previous+1;
}
constexpr std::optional<size_t> includeDirectiveBegin(std::string_view text,size_t caret) {
    auto begin=includePhysicalBegin(text,caret);
    while(begin>0) {
        auto previous=begin-1;
        if(previous>0 && text[previous-1]=='\r')--previous;
        if(previous==0 || text[previous-1]!='\\')break;
        const auto parent=includePhysicalBegin(text,begin-1);
        if(!includeLineFits(caret-parent))return {};
        begin=parent;
    }
    return begin;
}
// Comments may separate tokens; only escaped physical newlines are crossed.
constexpr std::optional<size_t> includeSkipSpace(std::string_view line,size_t at,bool trailing=false) {
    while(at<line.size()) {
        at=includeLogicalIndex(line,at);if(at>=line.size())break;
        if(includeSpace(line[at])){++at;continue;}
        const auto next=includeLogicalIndex(line,at+1);
        if(line[at]!='/' || next>=line.size())break;
        if(line[next]=='/')return line.size();
        if(line[next]!='*')break;
        auto end=next+1;bool closed=false;
        while(end<line.size()) {
            end=includeLogicalIndex(line,end);if(end>=line.size())break;
            const auto after=includeLogicalIndex(line,end+1);
            if(line[end]=='*' && after<line.size() && line[after]=='/') {
                at=after+1;closed=true;break;
            }
            if(line[end]=='\n')return {};
            ++end;
        }
        if(!closed)return trailing?std::optional<size_t>(line.size()):std::nullopt;
    }
    return at;
}
// One bounded logical directive; the header-name itself remains a physical literal.
constexpr std::optional<IncludeDirective> includeDirectiveAt(std::string_view text,size_t caret) {
    if(caret>text.size())return {};
    const auto origin=includeDirectiveBegin(text,caret);if(!origin)return {};
    const auto begin=*origin;auto end=begin;
    while(end<text.size() && includeLineFits(end-begin)) {
        end=includeLogicalIndex(text,end);
        if(end>=text.size() || text[end]=='\n')break;
        ++end;
    }
    if(!includePrefixFits(begin) || !includeLineFits(end-begin) || text.substr(0,end).find('\0')!=text.npos ||
       caret>end || !includePrefixIsCode(text,begin))return {};
    const auto line=text.substr(begin,end-begin);
    if(!line.empty() && (line.back()=='\\' || line.ends_with("\\\r")))return {};
    const auto first=includeSkipSpace(line,0);if(!first)return {};
    size_t at=*first;
    if(at==line.size() || line[at++]!='#')return {};
    const auto keyword=includeSkipSpace(line,at);if(!keyword)return {};
    at=*keyword;
    for(char expected:std::string_view("include")) {
        at=includeLogicalIndex(line,at);
        if(at>=line.size() || line[at++]!=expected)return {};
    }
    const auto header=includeSkipSpace(line,at);if(!header)return {};
    at=*header;
    if(at==line.size() || (line[at]!='"' && line[at]!='<'))return {};
    const bool angled=line[at++]=='<';const auto start=at;
    const auto close=line.find(angled?'>':'"',at);
    if(close==line.npos || !includeTargetFits(close-start))return {};
    const auto target=line.substr(start,close-start);
    if(target.front()=='/')return {};
    for(unsigned char c:target)if(c<32 || c==127 || c=='\\' || c==':')return {};
    const auto trailing=includeSkipSpace(line,close+1,true);
    if(!trailing || *trailing!=line.size())return {};
    return IncludeDirective{target,begin+start,begin+close,includePhysicalBegin(text,begin+start),angled};
}
}

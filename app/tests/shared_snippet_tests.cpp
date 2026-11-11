#include "cycode/Snippet.h"
#include <stdexcept>
using namespace cycode;
namespace {void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}}
namespace cycode {void sharedSnippetContracts(){
        auto s=expandSnippet("$2 = ${1:🛠}; ${2|first,second|} $1$0");
        require(s && s->text=="first = 🛠; first 🛠" && s->fields.size()==2,"Forward/default/choice expansion");
        require(s->fields[0].index==1 && s->fields[0].ranges.size()==2 && s->fields[1].choices.size()==2,"Ordered linked fields");
        for (const auto &field : s->fields) for (const auto &r : field.ranges) require(s->text.substr(r.start,r.end-r.start)==(field.index==1 ? "🛠" : "first"),"UTF-8 field offsets");
        s=expandSnippet("${1:outer ${2:inner}} $1$0");require(s && s->text=="outer inner outer inner" && s->fields[1].ranges.size()==2,"Nested mirrors");
        require(s->fields[1].ranges[0].parents==std::vector<uint32_t>{1},"Nested field ancestry");
        s=expandSnippet("call(${1:name})$0; trailing");require(s && s->finalByte==10,"Explicit final cursor");
        s=expandSnippet("${1|a\\,b,c\\|d,back\\\\slash|}");require(s && s->fields[0].choices==std::vector<std::string>{"a,b","c|d","back\\slash"},"Choice escapes");
        s=expandSnippet("fn() {\r\n  ${1:🛠}\r\n}\r\n$0");require(s.has_value(),"CRLF snippet normalization");indentSnippet(*s,"\t");
        require(s->text=="fn() {\n\t  🛠\n\t}\n","Indentation result");
        const auto r=s->fields[0].ranges[0];require(s->text.substr(r.start,r.end-r.start)=="🛠" && s->finalByte==s->text.size(),"Indentation range mapping");
        const auto meta=snippetMetadata(*s,20);require(meta.at("fields")[0].at("ranges")[0].at("start")==20+r.start,"Document byte offsets");
        for (const auto *unsupported : {"${1:$1}","${1:$2}${2:$1}","${TM_FILENAME}","${1/old/new/}","${1:bad","${1|bad}","$99999999999","$1$1"}) require(!expandSnippet(unsupported),"Unsupported or recursive snippet accepted");
        require(!expandSnippet(std::string(8193,'a')),"Input bound");
        std::string huge;for (int i=1;i<131;++i) huge+="${"+std::to_string(i)+":value} ";require(!expandSnippet(huge),"Field bound");

}}

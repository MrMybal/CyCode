#include "cycode/Completion.h"
#include <stdexcept>
#include <iostream>
using namespace cycode;
using Json=nlohmann::json;
static void require(bool value) { if (!value) throw std::runtime_error("Completion edit assertion failed"); }
template<class F> static void rejects(F f) { bool refused=false;try { f(); } catch (...) { refused=true; }require(refused); }
static Json pos(int line,int character) { return {{"line",line},{"character",character}}; }
static Json range(int a,int b) { return {{"start",pos(1,a)},{"end",pos(1,b)}}; }
int main() {
    try {
    for (const auto &[source,expected] : {std::pair{"<div>$0</div>","<div></div>"},
        {"${1:name} = $1$0","name = name"},{"${1|red,green,blue|}","red"},
        {"${1:${2:hello}}","hello"},{"\\$value","$value"}})
        if (plainSnippet(source)!=expected) throw std::runtime_error("Snippet default conversion failed");
    for (const auto source : {"${TM_FILENAME}","${1/foo/bar/}","${1:unclosed","${1|missing}","$"})
        if (plainSnippet(source)) throw std::runtime_error("Unsupported snippet accepted");
    const auto list=completionItems({{"itemDefaults",{{"data",{{"ticket",7}}},{"insertTextFormat",2},{"editRange",{{"insert",range(1,4)},{"replace",range(1,8)}}}}},{"items",Json::array({{{"label","greet"},{"textEditText","greet(${1:value})$0"}}})}});
    auto item=list[0];require(item.at("data").at("ticket")==7 && item.at("textEdit").contains("insert"));
    item["additionalTextEdits"]=Json::array({{{"range",{{"start",pos(0,0)},{"end",pos(0,0)}}},{"newText","import { greet } from './model';\r\n"}}});
    const std::string before="// 🛠\n\tgreRest\n";
    auto edit=completionEdit(before,item,pos(1,4));
    require(edit.after=="import { greet } from './model';\n// 🛠\n\tgreet(value)\n" && edit.cursor==pos(2,13));
    edit=completionEdit(before,item,pos(1,4),false);require(edit.after.find("greet(value)Rest")!=std::string::npos && edit.cursor==pos(2,13));
    auto overlap=item;overlap["additionalTextEdits"][0]["range"]=range(1,3);rejects([&] { completionEdit(before,overlap,pos(1,4)); });
    auto invalid=item;invalid["textEdit"]["insert"]["start"]=pos(1,2);rejects([&] { completionEdit(before,invalid,pos(1,4)); });
    rejects([&] { completionEdit("🛠",{{"label","x"}},pos(0,1)); });
    require(completionEdit("\n",{{"label","x"}},pos(0,0)).after=="x\n");
    require(completionEdit("// 🛠\n\tgreRest\n",{{"label","greet"}},pos(1,4)).after=="// 🛠\n\tgreet\n");
    require(!completionCommandAllowed({{"command",{{"command","workspace.delete"}}}},"typescript"));
    invalid=item;invalid["insertTextMode"]=3;rejects([&] { completionEdit(before,invalid,pos(1,4)); });
    const auto multiline=completionEdit("\tgre\n",{{"label","greet"},{"insertText","greet() {\n    body();\n  }"},{"insertTextMode",2}},pos(0,4));
    require(multiline.after=="\tgreet() {\n\t  body();\n\t}\n");
    auto many=Json::array();for (int i=0;i<2001;++i) many.push_back({{"label","symbol"}});
    bool truncated=false;const auto bounded=completionItems(many,&truncated);require(truncated && bounded.size()==2000);
    const auto oversized=completionItems({{"itemDefaults",{{"data",std::string(300000,'x')}}},{"items",Json::array({{{"label","large"}}})}},&truncated);
    require(truncated && oversized.empty());
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}

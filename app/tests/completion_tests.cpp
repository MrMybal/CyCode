#include "cycode/Completion.h"
#include "cycode/CompletionVariables.h"
#include "cycode/TextFile.h"
#include "cycode/Process.h"
#include <stdexcept>
#include <iostream>
using namespace cycode;
using Json=nlohmann::json;
static void require(bool value) { if (!value) throw std::runtime_error("Completion edit assertion failed"); }
template<class F> static void rejects(F f) { bool refused=false;try { f(); } catch (...) { refused=true; }require(refused); }
static Json pos(int line,int character) { return {{"line",line},{"character",character}}; }
static Json range(int a,int b) { return {{"start",pos(1,a)},{"end",pos(1,b)}}; }
void completionTransformContracts();
void snippetCatalogueContracts();
void completionSelectionContracts();
void userSnippetContracts();
namespace {
constexpr auto variables = [](std::string_view name) -> std::optional<std::string_view> {
    if (name == "FILE") return "Actor.cpp";
    if (name == "EMPTY") return "";
    if (name == "DANGER") return R"($1\})";
    if (name == "UNICODE") return "été 🛠";
    return {};
};
constexpr bool substitutes(std::string_view source, std::string_view expected) {
    const auto result = completion_detail::prepareSnippetVariables(source, variables);
    return result && result->text() == expected;
}
constexpr bool refuses(std::string_view source) {
    return !completion_detail::prepareSnippetVariables(source, variables);
}
static_assert(substitutes("$FILE", "Actor.cpp"));
static_assert(substitutes("${FILE}", "Actor.cpp"));
static_assert(substitutes("${FILE:ignored}", "Actor.cpp"));
static_assert(substitutes("${FILE:${UNKNOWN}}", "Actor.cpp"));
static_assert(substitutes("${EMPTY:fallback}", "fallback"));
static_assert(substitutes("$EMPTY", ""));
static_assert(substitutes("${UNKNOWN:${1:value}} $1$0", "${1:value} $1$0"));
static_assert(substitutes("${UNKNOWN:${FILE}}", "Actor.cpp"));
static_assert(substitutes("${1:$FILE} $1$0", "${1:Actor.cpp} $1$0"));
static_assert(substitutes("${1|$FILE,a\\,b,c\\|d|}", "${1|$FILE,a\\,b,c\\|d|}"));
static_assert(substitutes(R"(\$FILE \\ $1 ${2:plain})", R"(\$FILE \\ $1 ${2:plain})"));
static_assert(substitutes("$DANGER", R"(\$1\\\})"));
static_assert(substitutes("${1:$UNICODE}", "${1:été 🛠}"));
static_assert(substitutes("$1 $23 ${4}", "$1 $23 ${4}"));
static_assert(substitutes("x\r\ny", "x\r\ny"));
static_assert(refuses("$UNKNOWN"));
static_assert(refuses("${UNKNOWN}"));
static_assert(refuses("${FILE/foo/bar/}"));
static_assert(refuses("${1/foo/bar/}"));
static_assert(refuses("${FILE:unclosed"));
static_assert(refuses("${1:unclosed"));
static_assert(refuses("${1|unclosed}"));
static_assert(refuses("$"));
static_assert(refuses("${}"));
constexpr bool rejectsInputBudget() {
    std::array<char,8193> source{};
    return refuses({source.data(),source.size()});
}
static_assert(rejectsInputBudget());
constexpr bool rejectsSubstitutionBudget() {
    std::array<char,8192> value{};
    value.fill('$');
    auto lookup = [&](std::string_view) -> std::optional<std::string_view> { return std::string_view(value.data(),value.size()); };
    return !completion_detail::prepareSnippetVariables("$LARGE",lookup);
}
static_assert(rejectsSubstitutionBudget());
constexpr bool rejectsNestingBudget() {
    std::array<char,120> source{};size_t at=0;
    for(int i=0;i<17;++i) for(char c:std::string_view("${1:"))source[at++]=c;
    source[at++]='x';for(int i=0;i<17;++i)source[at++]='}';
    return refuses({source.data(),at});
}
static_assert(rejectsNestingBudget());
constexpr bool rejectsNodeBudget() {
    auto lookup=variables;
    completion_detail::VariableSubstitution<decltype(lookup)> finalNode{"$1",lookup};
    finalNode.nodes=2047;
    if(!finalNode.fragment(0,false,true) || finalNode.nodes!=2048)return false;
    completion_detail::VariableSubstitution<decltype(lookup)> overflow{"$1",lookup};
    overflow.nodes=2048;
    return !overflow.fragment(0,false,true);
}
static_assert(rejectsNodeBudget());
}
int main() {
    try {
    completionTransformContracts();
    snippetCatalogueContracts();
        completionSelectionContracts();
        userSnippetContracts();
    for (const auto &[source,expected] : {std::pair{"<div>$0</div>","<div></div>"},
        {"${1:name} = $1$0","name = name"},{"${1|red,green,blue|}","red"},
        {"${1:${2:hello}}","hello"},{"\\$value","$value"}})
        if (plainSnippet(source)!=expected) throw std::runtime_error("Snippet default conversion failed");
    for (const auto source : {"${UNKNOWN_VARIABLE}","${1/foo/bar/}","${1:unclosed","${1|missing}","$"})
        if (plainSnippet(source)) throw std::runtime_error("Unsupported snippet accepted");
    const auto list=completionItems({{"itemDefaults",{{"data",{{"ticket",7}}},{"insertTextFormat",2},{"editRange",{{"insert",range(1,4)},{"replace",range(1,8)}}}}},{"items",Json::array({{{"label","greet"},{"textEditText","greet(${1:value})$0"}}})}});
    auto item=list[0];require(item.at("data").at("ticket")==7 && item.at("textEdit").contains("insert"));
    item["additionalTextEdits"]=Json::array({{{"range",{{"start",pos(0,0)},{"end",pos(0,0)}}},{"newText","import { greet } from './model';\r\n"}}});
    const std::string before="// 🛠\n\tgreRest\n";
    const auto workspace=std::filesystem::absolute("workspace");
    const CompletionSnippetContext context{workspace/"include/Actor.test.cpp",workspace,before,10};
    require(plainSnippet("$TM_FILENAME $TM_FILENAME_BASE $WORKSPACE_NAME",context)=="Actor.test.cpp Actor.test workspace");
    require(plainSnippet("$TM_FILEPATH",context)==pathUtf8(context.file));
    require(plainSnippet("$TM_DIRECTORY",context)==pathUtf8(context.file.parent_path()));
    require(plainSnippet("$RELATIVE_FILEPATH",context)==pathUtf8(context.file.lexically_relative(workspace)));
    require(plainSnippet("$TM_LINE_INDEX $TM_LINE_NUMBER $CURSOR_INDEX $CURSOR_NUMBER",context)=="1 2 0 1");
    require(plainSnippet("$TM_CURRENT_WORD",context)=="greRest");
    require(plainSnippet("$TM_CURRENT_LINE",context)=="\tgreRest");
    require(!plainSnippet("$CLIPBOARD",context));
    require(plainSnippet("${CLIPBOARD:${1:fallback}}",context)=="fallback");
    auto variableItem=item;variableItem["textEdit"]["newText"]="greet(${1:$TM_FILENAME_BASE}, $1, $TM_LINE_NUMBER)$0";
    const auto variableEdit=completionEdit(before,variableItem,pos(1,4),true,context);
    require(variableEdit.after=="import { greet } from './model';\n// 🛠\n\tgreet(Actor.test, Actor.test, 2)\n");
    require(variableEdit.snippet.at("fields").size()==1);
    for(const auto &fieldRange:variableEdit.snippet.at("fields")[0].at("ranges"))
        require(variableEdit.after.substr(fieldRange.at("start"),fieldRange.at("end").get<size_t>()-fieldRange.at("start").get<size_t>())=="Actor.test");
    auto literalContext=context;literalContext.file=workspace/"A$1}.cpp";
    require(plainSnippet("${1:$TM_FILENAME_BASE} $1",literalContext)=="A$1} A$1}");
    auto longContext=context;const std::string longLine(9000,'x');longContext.text=longLine;longContext.byteOffset=0;
    require(plainSnippet("$TM_FILENAME",longContext)=="Actor.test.cpp");
    require(!plainSnippet("$TM_CURRENT_LINE",longContext));
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

#include "cycode/Completion.h"
#include "cycode/CompletionTransforms.h"
#include <stdexcept>
using namespace cycode;
using namespace cycode::completion_detail;
namespace {
constexpr TransformCaptures groups() {
    TransformCaptures values{};
    values[0]="Actor.cpp";values[1]="Actor";values[3]="";values[4]="été 🛠";
    return values;
}
constexpr auto caseDispatch = [](std::string_view value,std::string_view method) -> std::optional<PreparedSnippet> {
    if(value!="Actor" || method!="upcase")return {};
    PreparedSnippet result;if(!result.append("case-callback",true))return {};return result;
};
constexpr bool formats(std::string_view format,std::string_view expected) {
    const auto result=formatTransform(format,groups(),caseDispatch);
    return result && result->text()==expected;
}
static_assert(formats("$0 $1 ${1}","Actor.cpp Actor Actor"));
static_assert(formats("[$2][$3]","[][]"));
static_assert(formats("${1:+yes}","yes"));
static_assert(formats("${2:+no}",""));
static_assert(formats("${1:-no}","Actor"));
static_assert(formats("${2:-yes}","yes"));
static_assert(formats("${3:fallback}","fallback"));
static_assert(formats("${1:fallback}","Actor"));
static_assert(formats("${1:?yes:no}","yes"));
static_assert(formats("${2:?no:yes}","yes"));
static_assert(formats("${3:?no:empty}","empty"));
static_assert(formats("${2:?no:${1}}","Actor"));
static_assert(formats("${1:+${4}}","été 🛠"));
static_assert(formats("${2:+${1:/downcase}}","")); // Unselected case callback is never called.
static_assert(formats("${1:/upcase}","case-callback")); // Dispatch, not Unicode case execution.
static_assert(formats(R"(\$1 \/ \} \\)",R"($1 / } \)"));
static_assert(formats("${1:?a\\:b:c}","a:b"));
static_assert(validTransformFormat("${1:/upcase}${1:/downcase}${1:/capitalize}"));
static_assert(validTransformFormat("${1:/camelcase}${1:/pascalcase}${1:/snakecase}${1:/kebabcase}"));
static_assert(!validTransformFormat("${1:/execute}"));
static_assert(!validTransformFormat("${1:?missing}"));
static_assert(!validTransformFormat("${17}"));
static_assert(!validTransformFormat("$UNKNOWN"));
static_assert(!validTransformFormat("${1"));
static_assert(transformOptions("gimsu"));
static_assert(transformOptions(""));
static_assert(!transformOptions("gg"));
static_assert(!transformOptions("y"));
static_assert(!transformOptions("v"));
static_assert(TransformLimits::patternBytes==256 && TransformLimits::captures==16);
static_assert(TransformLimits::transformations==8 && TransformLimits::matchAttempts==64);
constexpr auto lookup = [](std::string_view name)->std::optional<std::string_view> {
    return name=="FILE" ? std::optional<std::string_view>("Actor.cpp") : std::nullopt;
};
// Exercise the real request scanner/formatter without substituting for regex matching.
constexpr auto syntaxOnly = [](std::string_view input,std::string_view pattern,std::string_view format,
                               std::string_view options,bool emit)->std::optional<PreparedSnippet> {
    if(pattern.size()>TransformLimits::patternBytes || !transformOptions(options) || !validTransformFormat(format))return {};
    PreparedSnippet result;
    if(emit && !result.append(input,true))return {};
    return result;
};
constexpr bool scans(std::string_view source,bool expected) {
    return bool(prepareSnippetVariables(source,lookup,syntaxOnly))==expected;
}
static_assert(scans("${FILE/(.*)\\.cpp$/${1:/upcase}/}",true));
static_assert(scans(R"(${FILE/\//\//g})",true));
static_assert(scans("${FILE/(a)/${1:?yes:no}/i}",true));
static_assert(scans("${FILE//x/g}",true));
static_assert(scans("${FILE/a/${1}/gg}",false));
static_assert(scans("${FILE/a/${1:?bad}/}",false));
static_assert(scans("${FILE/a/x/",false));
static_assert(scans("${1/a/x/}",false)); // Interactive field transforms remain unsupported.
void require(bool result) {if(!result)throw std::runtime_error("Completion transform contract failed");}
}
void completionTransformContracts() {
    auto transformed=[](std::string_view input,std::string_view pattern,std::string_view format,std::string_view options="") {
        TransformBudget budget;const auto result=transformSnippetVariable(input,pattern,format,options,budget);
        return result ? std::optional<std::string>(result->text()) : std::nullopt;
    };
    require(transformed("Actor.cpp",R"((.*)\.cpp$)","$1")=="Actor");
    require(transformed("Actor.cpp",R"((.*)\.cpp$)","${1:/upcase}")=="ACTOR");
    require(transformed("a.b.c","\\.","_","g")=="a_b_c");
    require(transformed("aa","a","x")=="xa");
    require(transformed("aa","A","x","gi")=="xx");
    require(transformed("a","(?:|a)","x","g")=="xax");
    require(transformed("été","","x","gu")=="xéxtxéx");
    require(transformed("value","absent","$0")=="value");
    require(transformed("b","(a)?b","${1:?yes:no}")=="no");
    require(transformed("ab","(a)?b","${1:+found}")=="found");
    require(transformed("ab","(.*)",R"(\$1)")=="$1");
    require(!transformed("value","[","x"));
    require(!transformed("value","a","${17}"));
    require(!transformed("value","a","x","gg"));
    require(!transformed("value",std::string(257,'a'),"x"));
    require(!transformed("aaaaaaaaaaaaaaaaa","(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)","x"));
    require(!transformed(std::string(8193,'a'),"a","x"));
    require(!transformed("aa","a",std::string(8192,'x'),"g"));
    TransformBudget spent;spent.transformations=TransformLimits::transformations;
    require(!transformSnippetVariable("x","x","y","",spent));
    TransformBudget matches;matches.matches=TransformLimits::matchAttempts;
    require(!transformSnippetVariable("x","x","y","",matches));
    TransformBudget calls;calls.callouts=TransformLimits::callouts;
    require(!transformSnippetVariable("x","x","y","",calls));
    TransformBudget expired;expired.deadline=std::chrono::steady_clock::now()-std::chrono::seconds(1);
    require(!transformSnippetVariable("x","x","y","",expired));
    for(const auto &[method,expected]:{std::pair{"upcase","ÉTÉ ACTOR"},
            {"downcase","été actor"},{"capitalize","Été Actor"},
            {"camelcase","étéActor"},{"pascalcase","ÉtéActor"},
            {"snakecase","été_actor"},{"kebabcase","été-actor"}}) {
        const auto result=transformSnippetCase("été Actor",method);require(result && result->text()==expected);
    }
    const auto acronym=transformSnippetCase("HTTPServer2Value","snakecase");
    require(acronym && acronym->text()=="http_server2_value");
    CompletionSnippetContext context{std::filesystem::absolute("Actor.cpp"),std::filesystem::current_path()};
    require(plainSnippet(R"(${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/})",context)=="ACTOR");
    bool deferred=false;
    require(plainSnippet("${TM_FILENAME/[/bad/}",context,&deferred).has_value() && deferred);
    require(!plainSnippet("${TM_FILENAME/[/bad/}",context));
    deferred=true;
    require(plainSnippet("$TM_FILENAME",context,&deferred)=="Actor.cpp" && !deferred);
    auto item=nlohmann::json{{"label","actor"},{"insertTextFormat",2},
        {"insertText",R"(${1:${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}} $1$0)"}};
    const auto result=completionEdit("gre",item,{{"line",0},{"character",3}},true,context);
    require(result.after=="ACTOR ACTOR" && result.snippet.at("fields").size()==1);
}

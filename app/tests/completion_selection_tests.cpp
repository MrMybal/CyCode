#include "cycode/Completion.h"
#include "cycode/Engine.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <array>
#include <fstream>
#include <stdexcept>
using namespace cycode;
namespace {
constexpr bool selected(std::string_view text,size_t caret,CompletionSelection range,std::string_view expected) {
    const auto value=selectedSnippetText(text,caret,range);return value && *value==expected;
}
static_assert(selected("abc",3,{0,3},"abc"));
static_assert(selected("abc",0,{0,3},"abc"));
static_assert(selected("abc",2,{1,2},"b"));
static_assert(selected("abc",1,{1,2},"b"));
static_assert(selected("abc",1,{1,1},""));
static_assert(selected("a\nb\n",3,{0,3},"a\nb"));
static_assert(selected("é🛠x",6,{0,6},"é🛠"));
static_assert(selected("é🛠x",2,{2,6},"🛠"));
static_assert(!selectedSnippetText("abc",1,CompletionSelection{0,3}));
static_assert(!selectedSnippetText("abc",0,CompletionSelection{2,1}));
static_assert(!selectedSnippetText("abc",3,CompletionSelection{0,4}));
static_assert(!selectedSnippetText("abc",4,CompletionSelection{0,3}));
static_assert(!selectedSnippetText("é🛠x",1,CompletionSelection{1,6}));
static_assert(!selectedSnippetText("é🛠x",6,CompletionSelection{0,3}));
static_assert(!selectedSnippetText("é🛠x",3,std::nullopt));
static_assert(selectedSnippetText("abc",0,std::nullopt)->empty());
static_assert(selectedSnippetText("",0,CompletionSelection{0,0})->empty());
static_assert(snippetByteBoundary("é🛠x",0) && snippetByteBoundary("é🛠x",2) && snippetByteBoundary("é🛠x",6));
static_assert(!snippetByteBoundary("é🛠x",1) && !snippetByteBoundary("é🛠x",4));
static_assert(maxCompletionSelectionBytes==8192);
constexpr bool selectionBudget() {
    std::array<char,8193> value{};
    const std::string_view text(value.data(),value.size());
    return selectedSnippetText(text,8192,CompletionSelection{0,8192}).has_value() &&
        !selectedSnippetText(text,8193,CompletionSelection{0,8193});
}
static_assert(selectionBudget());
void require(bool result) {if(!result)throw std::runtime_error("Completion selection contract failed");}
template<class F> void rejects(F callback) {bool rejected=false;try{callback();}catch(...){rejected=true;}require(rejected);}
Json point(size_t line,size_t character) {return {{"line",line},{"character",character}};}
Json range(Json start,Json end) {return {{"start",start},{"end",end}};}
}
void completionSelectionContracts() {
    const std::string before="prefix é🛠$1\\} suffix";
    const size_t start=7,end=17;
    const auto selection=CompletionSelection{start,end};
    const auto captured=before.substr(start,end-start);
    CompletionSnippetContext context{{},{},before,start,selection};
    bool used=false;
    require(plainSnippet("<$TM_SELECTED_TEXT>",context,nullptr,&used)=="<"+captured+">" && used);
    require(plainSnippet("${1:$TM_SELECTED_TEXT} $1$0",context)==captured+" "+captured);
    require(plainSnippet("$TM_SELECTED_TEXT")=="");
    require(plainSnippet("${TM_SELECTED_TEXT:fallback}")=="fallback");
    used=true;require(plainSnippet("${TM_FILENAME:unused}",context,nullptr,&used)=="unused" && !used);
    require(!plainSnippet("$TM_SELECTED_TEXT",{{},{},before,8,CompletionSelection{8,end}}));
    require(utf16Offset("é🛠x",point(0,3))==6);
    rejects([&]{utf16Offset("é🛠x",point(0,2));});
    rejects([&]{utf16Offset("text",{{"line",0.0},{"character",1}});});
    rejects([&]{utf16Offset("text",{{"line",0},{"character",1.5}});});
    rejects([&]{utf16Offset("text",{{"line",uint64_t(-1)},{"character",0}});});

    const std::string lines="    one();\n\t🛠();\n";
    const Json selectedRange=range(point(0,0),point(1,6));
    const auto selectedEnd=utf16Offset(lines,selectedRange.at("end"));
    const auto body="begin\n${1:$TM_SELECTED_TEXT}\nend$0";
    Json item={{"label","wrap"},{"insertTextFormat",2},{"insertTextMode",2},
        {"textEdit",{{"range",selectedRange},{"newText",body}}}};
    const CompletionSnippetContext multiline{{},{},lines,selectedEnd,CompletionSelection{0,selectedEnd}};
    const auto edit=completionEdit(lines,item,selectedRange.at("end"),true,multiline);
    require(edit.after=="begin\n"+lines.substr(0,selectedEnd)+"\nend\n");
    require(edit.snippet.at("fields").size()==1);
    require(completionEdit(lines,item,point(0,0),true,multiline).after==edit.after);
    auto bad=item;bad["textEdit"]["newText"]="replacement";
    rejects([&]{completionEdit(lines,bad,selectedRange.at("end"),true,multiline);});
    bad["textEdit"]["newText"]="${TM_FILENAME:ignored ${TM_SELECTED_TEXT}}";
    auto filenameContext=multiline;filenameContext.file="present.cpp";
    rejects([&]{completionEdit(lines,bad,selectedRange.at("end"),true,filenameContext);});
    rejects([&]{completionEdit(lines,item,selectedRange.at("end"));}); // Server edits keep the same-line contract.

    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("cycode-wrap-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/".cycode");
    auto write=[](const fs::path &file,const std::string &text){std::ofstream out(file,std::ios::binary);out<<text;};
    const auto catalogue=root/".cycode/snippets.code-snippets";
    write(root/"main.verse",lines);
    write(catalogue,Json{{"Plain",{{"prefix","plain"},{"body","discard"}}},
        {"Wrap",{{"prefix","wrap"},{"body",body}}}}.dump());
    {
        Engine engine(root);
        auto doc=engine.dispatch("documents.read",{{"path","main.verse"}});
        const auto revision=doc.at("revision");
        Json request={{"path","main.verse"},{"expectedRevision",revision},{"position",selectedRange.at("end")},{"selection",selectedRange}};
        auto reply=engine.dispatch("snippets.complete",request);
        const auto &items=reply.at("result").at("items");
        require(items.size()==2 && items[0].at("cycodeSnippetAvailable")==false && items[1].at("cycodeSnippetAvailable")==true);
        Json params={{"sessionId",reply.at("sessionId")},{"item",0},{"expectedRevision",revision}};
        rejects([&]{engine.dispatch("language.completionPrepare",params);});
        params["item"]=1;
        const auto prepared=engine.dispatch("language.completionPrepare",params);
        const auto id=prepared.at("plan").at("id");
        require(engine.dispatch("changes.preview",{{"id",id},{"path","main.verse"}}).at("after")==edit.after);
        require(engine.dispatch("documents.read",{{"path","main.verse"}}).at("text")==lines);
        engine.dispatch("changes.apply",{{"id",id}});
        require(readTextFileBytes(root/"main.verse")==lines);
        engine.dispatch("changes.undo",{{"id",id}});
        rejects([&]{engine.dispatch("language.completionPrepare",params);});
        doc=engine.dispatch("documents.read",{{"path","main.verse"}});
        request["expectedRevision"]=doc.at("revision");request["position"]=point(0,1);
        rejects([&]{engine.dispatch("snippets.complete",request);});
        request["position"]=selectedRange.at("end");
        reply=engine.dispatch("snippets.complete",request);
        params["sessionId"]=reply.at("sessionId");params["expectedRevision"]=doc.at("revision");
        write(catalogue,"{}");rejects([&]{engine.dispatch("language.completionPrepare",params);});
    }
    fs::remove_all(root);
}

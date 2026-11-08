#include "cycode/SnippetCatalogue.h"
#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <stdexcept>

using namespace cycode;
namespace {
static_assert(snippetPrefixBytes("fo",2,"for")==2);
static_assert(snippetPrefixBytes("for",3,"for")==3);
static_assert(snippetPrefixBytes("food",4,"for")==0);
static_assert(snippetPrefixBytes("call foSuffix",7,"for")==2);
static_assert(snippetPrefixBytes("x.for",5,"for")==3);
static_assert(snippetPrefixBytes("Xfor",4,"for")==0);
static_assert(snippetPrefixBytes("Fo",2,"for")==0);
static_assert(snippetPrefixBytes("",0,"for")==0);
static_assert(snippetPrefixBytes(" ",1,"for")==0);
static_assert(snippetPrefixBytes("f",2,"for")==0);
static_assert(snippetPrefixBytes("é",2,"été")==2);
static_assert(snippetPrefixBytes("é",2,"")==0);
static_assert(snippetScopeMatches("","verse"));
static_assert(snippetScopeMatches("cpp,c","cpp"));
static_assert(snippetScopeMatches("cpp,c","c"));
static_assert(snippetScopeMatches(" cpp ,\tverse ","verse"));
static_assert(!snippetScopeMatches("cpp,c","csharp"));
static_assert(!snippetScopeMatches("Cpp","cpp"));
static_assert(!snippetScopeMatches(", ,", ""));
static_assert(SnippetCatalogueLimits::bytes==256*1024 && SnippetCatalogueLimits::definitions==256);
static_assert(SnippetCatalogueLimits::items==512 && SnippetCatalogueLimits::body==8192);
static_assert(snippetRegistryFits(0,0));
static_assert(snippetRegistryFits(1,256*1024));
static_assert(snippetRegistryFits(32,1024*1024));
static_assert(snippetRegistryFits(31,1024*1024-1));
static_assert(!snippetRegistryFits(33,0));
static_assert(!snippetRegistryFits(0,1024*1024+1));
static_assert(!snippetRegistryFits(size_t(-1),0));
static_assert(!snippetRegistryFits(0,size_t(-1)));
void require(bool value) { if(!value)throw std::runtime_error("Snippet catalogue contract failed"); }
template<class F> void rejects(F function) { bool failed=false;try{function();}catch(...){failed=true;}require(failed); }
}
void snippetCatalogueContracts() {
    using Json=nlohmann::json;
    auto data=Json{{"Actor",{{"prefix",Json::array({"actor","act"})},{"scope","cpp,verse"},
        {"description","Create an actor"},{"body",Json::array({"class ${1:$TM_FILENAME_BASE} {","\t$0","};"})}}},
        {"Everywhere",{{"prefix","hello"},{"body","Hello ${1:world}! $1$0"}}}};
    const auto result=snippetCatalogueItems(data.dump(),"verse");
    require(result.at("items").size()==3 && result.at("isIncomplete")==false);
    require(result.at("items")[0].at("insertText")=="class ${1:$TM_FILENAME_BASE} {\n\t$0\n};");
    require(snippetCatalogueItems(data.dump(),"java").at("items").size()==1);
    Json descriptor={{"id","fixture.native.snippets"},{"title","Fixture snippets"},{"definitions",data},{"order",-10}};
    validateSnippetContribution(descriptor);
    auto entry=descriptor;entry["plugin"]="fixture.native";
    const auto merged=mergedSnippetItems(data.dump(),Json::array({entry}),"verse");
    require(merged.at("items").size()==6 && !merged.at("isIncomplete").get<bool>());
    require(merged.at("items")[0].at("cycodeSnippetSource").at("kind")=="workspace");
    require(merged.at("items")[3].at("cycodeSnippetSource").at("plugin")=="fixture.native");
    require(merged.at("items")[3].at("label").get<std::string>().ends_with("[fixture.native]"));
    require(mergedSnippetItems("{}",Json::array({entry}),"java").at("items").size()==1);
    for (const auto *key:{"path","callback","when","context","plugin"}) {
        auto invalid=descriptor;invalid[key]="unsupported";
        rejects([&]{validateSnippetContribution(invalid);});
    }
    for (const auto &order:{Json(-10001),Json(10001),Json(0.5),Json(uint64_t(-1))}) {
        auto invalid=descriptor;invalid["order"]=order;
        rejects([&]{validateSnippetContribution(invalid);});
    }
    auto forged=entry;forged["plugin"]="another.owner";
    rejects([&]{mergedSnippetItems("{}",Json::array({forged}),"cpp");});
    rejects([&]{mergedSnippetItems("{}",Json::object(),"cpp");});
    require(snippetLanguage("file.verse")=="verse" && snippetLanguage("code.TSX")=="typescriptreact");
    require(snippetLanguage("note.txt")=="plaintext" && snippetLanguage("README.md")=="markdown");
    for (const auto &invalid:{Json::array(),Json{{"Bad",{{"body","x"}}}},
            Json{{"Bad",{{"prefix",""},{"body","x"}}}},
            Json{{"Bad",{{"prefix",Json::array()},{"body","x"}}}},
            Json{{"Bad",{{"prefix","x"},{"body",7}}}},
            Json{{"Bad",{{"prefix","x"},{"body","x"},{"command","run"}}}},
            Json{{"Bad",{{"prefix","x"},{"body","x"},{"isFileTemplate",true}}}}})
        rejects([&]{snippetCatalogueItems(invalid.dump(),"cpp");});
    rejects([&]{snippetCatalogueItems("{", "cpp");});
    rejects([&]{snippetCatalogueItems(std::string(32,'[')+"0"+std::string(32,']'),"cpp");});
    rejects([&]{snippetCatalogueItems(std::string(SnippetCatalogueLimits::bytes+1,' '),"cpp");});
    auto oversized=data;oversized["Actor"]["body"]=std::string(8193,'x');
    rejects([&]{snippetCatalogueItems(oversized.dump(),"java");}); // Validate even excluded scopes.
    Json many=Json::object();
    for(size_t i=0;i<256;++i) many["entry"+std::to_string(i)]={{"prefix",Json::array({"a","b","c"})},{"body","$0"}};
    const auto limited=snippetCatalogueItems(many.dump(),"cpp");
    require(limited.at("items").size()==512 && limited.at("isIncomplete")==true);
    const auto all=mergedSnippetItems(many.dump(),Json::array({entry}),"cpp");
    require(all.at("items").size()==512 && all.at("isIncomplete")==true);
    auto malformed=entry;malformed["definitions"]["Bad"]={{"prefix","bad"},{"body",7},{"scope","rust"}};
    rejects([&]{mergedSnippetItems(many.dump(),Json::array({malformed}),"cpp");});
    many["tooMany"]={{"prefix","x"},{"body","y"}};
    rejects([&]{snippetCatalogueItems(many.dump(),"cpp");});

    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("cycode-snippet-catalogue-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/".cycode");
    auto write=[](const fs::path &file,const std::string &text) {std::ofstream out(file,std::ios::binary);out<<text;};
    const auto catalogue=root/utf8Path(std::string(snippetCataloguePath));
    write(catalogue,Json{{"Greeting",{{"prefix","gre"},{"scope","verse"},{"body","greet(${1:$TM_FILENAME_BASE}, $1)$0"}}}}.dump());
    write(root/"main.verse","greSuffix\n");
    {
        Engine engine(root); // Verse has no configured backend; no fake server is supplied.
        rejects([&]{engine.dispatch("snippets.complete",{{"path","main.verse"},{"expectedRevision",1},
            {"position",{{"line",0},{"character",0}}}});});
        auto doc=engine.dispatch("documents.read",{{"path","main.verse"}});
        const Json request={{"path","main.verse"},{"expectedRevision",doc.at("revision")},
            {"position",{{"line",0},{"character",3}}}};
        auto issued=engine.dispatch("snippets.complete",request);
        require(issued.at("language")=="verse" && issued.at("result").at("items").size()==1);
        Json parameters={{"sessionId",issued.at("sessionId")},{"item",0},{"expectedRevision",doc.at("revision")}};
        rejects([&]{engine.dispatch("language.completionResolve",parameters);});
        auto prepared=engine.dispatch("language.completionPrepare",parameters);
        require(prepared.at("snippet").at("fields").size()==1);
        const auto id=prepared.at("plan").at("id");
        const auto preview=engine.dispatch("changes.preview",{{"id",id},{"path","main.verse"}});
        require(preview.at("after")=="greet(main, main)Suffix\n");
        require(engine.dispatch("documents.read",{{"path","main.verse"}}).at("text")=="greSuffix\n");
        engine.dispatch("changes.apply",{{"id",id}});
        require(readTextFileBytes(root/"main.verse")=="greSuffix\n");
        engine.dispatch("changes.undo",{{"id",id}});
        rejects([&]{engine.dispatch("language.completionPrepare",parameters);});
        doc=engine.dispatch("documents.read",{{"path","main.verse"}});
        auto fresh=request;fresh["expectedRevision"]=doc.at("revision");
        issued=engine.dispatch("snippets.complete",fresh);
        parameters["sessionId"]=issued.at("sessionId");parameters["expectedRevision"]=doc.at("revision");
        const auto original=readTextFileBytes(catalogue);write(catalogue,"{}");
        rejects([&]{engine.dispatch("language.completionPrepare",parameters);});
        write(catalogue,original);write(root/"main.verse","outside\n");
        rejects([&]{engine.dispatch("language.completionPrepare",parameters);});
    }
    fs::remove_all(root);
}

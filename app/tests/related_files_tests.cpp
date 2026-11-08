#include <source_location>
#include "cycode/RelatedFiles.h"
// Exercise a real compiler directive in addition to the reader's string fixtures.
# /* tokens */ in\
clude /* continuation */ \
"cycode/IncludeDirective.h"
#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <stdexcept>
using namespace cycode;
namespace {
constexpr auto includeOnLine(std::string_view text){return includeDirectiveAt(text,text.find("#include"));}
static_assert(cppFileKind(".h")==CppFileKind::header);
static_assert(cppFileKind(".H")==CppFileKind::header);
static_assert(cppFileKind(".hh")==CppFileKind::header);
static_assert(cppFileKind(".hpp")==CppFileKind::header);
static_assert(cppFileKind(".hxx")==CppFileKind::header);
static_assert(cppFileKind(".inl")==CppFileKind::header);
static_assert(cppFileKind(".cpp")==CppFileKind::source);
static_assert(cppFileKind(".C")==CppFileKind::source);
static_assert(cppFileKind(".cc")==CppFileKind::source);
static_assert(cppFileKind(".cxx")==CppFileKind::source);
static_assert(cppFileKind(".mm")==CppFileKind::source);
static_assert(cppFileKind(".m")==CppFileKind::source);
static_assert(cppFileKind(".cs")==CppFileKind::none);
static_assert(cppFileKind(".glsl")==CppFileKind::none);
static_assert(cppFileKind("")==CppFileKind::none);
static_assert(cppFileKind(".cppm")==CppFileKind::none);
static_assert(relatedGeneratedStem("Actor.generated"));
static_assert(relatedGeneratedStem("Actor.GENERATED"));
static_assert(relatedGeneratedStem("Actor.gen"));
static_assert(!relatedGeneratedStem("ActorGeneration"));
static_assert(relatedProbeFits(0));
static_assert(relatedProbeFits(127));
static_assert(!relatedProbeFits(128));
static_assert(!relatedProbeFits(size_t(-1)));
static_assert(!relatedIndexCurrent(RelatedIndexState::missing));
static_assert(!relatedIndexCurrent(RelatedIndexState::busy));
static_assert(!relatedIndexCurrent(RelatedIndexState::stale));
static_assert(relatedIndexCurrent(RelatedIndexState::ready));
static_assert(relatedIndexCurrent(RelatedIndexState::partial));
static_assert(!relatedIndexCurrent(RelatedIndexState::changed));
static_assert(relatedIndexName(RelatedIndexState::partial)=="partial");
static_assert(relatedIndexName(RelatedIndexState::changed)=="changed");
static_assert(relatedIndexFresh(RelatedIndexState::ready,7,2,7,2));
static_assert(relatedIndexFresh(RelatedIndexState::partial,7,2,7,2));
static_assert(!relatedIndexFresh(RelatedIndexState::ready,7,2,8,2));
static_assert(!relatedIndexFresh(RelatedIndexState::ready,7,2,7,3));
static_assert(!relatedIndexFresh(RelatedIndexState::stale,7,2,7,2));
static_assert(!relatedIndexFresh(RelatedIndexState::ready,0,0,0,0));
static_assert(relatedAutoOpen(1,0,false));
static_assert(!relatedAutoOpen(0,0,false));
static_assert(!relatedAutoOpen(2,0,false));
static_assert(!relatedAutoOpen(1,1,false));
static_assert(!relatedAutoOpen(1,0,true));
static_assert(includePrefixFits(1024*1024));
static_assert(!includePrefixFits(1024*1024+1));
static_assert(includeLineFits(8192));
static_assert(!includeLineFits(8193));
static_assert(includeTargetFits(1024));
static_assert(!includeTargetFits(1025));
static_assert(!includeTargetFits(0));
static_assert(includeDirectiveAt("#include \"Foo.h\"",0)->target=="Foo.h");
static_assert(includeDirectiveAt(" \t# include <library/Foo.inc> // comment",5)->angled);
static_assert(includeDirectiveAt("#include<algorithm>",0)->target=="algorithm");
static_assert(includeDirectiveAt("#include \"été.h\" /* ok */",0)->target=="été.h");
static_assert(includeDirectiveAt("#include \"../Public/Foo.h\"",0)->start==10);
static_assert(!includeDirectiveAt("#include HEADER",0));
static_assert(!includeDirectiveAt("#include_next \"Foo.h\"",0));
static_assert(!includeDirectiveAt("#import \"Foo.h\"",0));
static_assert(!includeDirectiveAt("//#include \"Foo.h\"",0));
static_assert(!includeDirectiveAt("/*\n#include \"Foo.h\"",3));
static_assert(!includeOnLine("auto x=R\"tag(\n#include \"Foo.h\"\n)tag\";"));
static_assert(includeOnLine("auto x=R\"tag(foo)tag\";\n#include \"Foo.h\"").has_value());
static_assert(includeOnLine("auto x=1'000;\n#include \"Foo.h\"").has_value());
static_assert(includeOnLine("auto x='\\\'';\n#include \"Foo.h\"").has_value());
static_assert(!includeDirectiveAt("// continued\\\n#include \"Foo.h\"",14));
static_assert(!includeDirectiveAt("#define X \\\n#include \"Foo.h\"",12));
static_assert(includeDirectiveAt("/* done */\n#include \"Foo.h\"",11).has_value());
static_assert(!includeDirectiveAt("#include \"Foo.h\" junk",0));
static_assert(!includeDirectiveAt("#include \"Foo.h\" /*ok*/ junk",0));
static_assert(!includeDirectiveAt("#include \"/outside.h\"",0));
static_assert(!includeDirectiveAt("#include \"C:/outside.h\"",0));
static_assert(!includeDirectiveAt("#include \"folder\\Foo.h\"",0));
static_assert(!includeDirectiveAt("#include \"\"",0));
static_assert(!includeDirectiveAt("#include \"Foo.h",0));
static_assert(!includeDirectiveAt("#include \"Foo.h\"",100));
static_assert(includeLogicalIndex("\\\r\nA",0)==3);
static_assert(includeLogicalIndex("\\\n\\\nA",0)==4);
static_assert(!includeOnLine("/\\\n*\n#include \"Foo.h\""));
static_assert(includeOnLine("/* finished *\\\n/\n#include \"Foo.h\"").has_value());
static_assert(includeDirectiveAt("/* before */ # /* key */ include /* header */ \"Foo.h\"",0)->target=="Foo.h");
static_assert(includeDirectiveAt("#include/**/<Foo.h>/**/",0)->angled);
static_assert(!includeDirectiveAt("#in/**/clude \"Foo.h\"",0));
static_assert(!includeDirectiveAt("#include /* unfinished \"Foo.h\"",0));
static_assert(!includeDirectiveAt("#include // no header",0));
static_assert(!includeDirectiveAt("#include /* first\nsecond */ \"Foo.h\"",0));
constexpr std::string_view continuedInclude="# /*a*/ in\\\nclude /*é*/ \\\n  \"été.h\"\n";
static_assert(includeDirectiveAt(continuedInclude,0)->target=="été.h");
static_assert(includeDirectiveAt(continuedInclude,15)->target=="été.h");
static_assert(includeDirectiveAt(continuedInclude,continuedInclude.find("été"))->target=="été.h");
static_assert(includeDirectiveAt(continuedInclude,0)->start==continuedInclude.find("été"));
static_assert(includeDirectiveAt(continuedInclude,0)->lineStart==continuedInclude.rfind('\n',continuedInclude.find("été")) +1);
static_assert(includeDirectiveAt("#\\\r\ninclude\\\r\n <Foo.h>",0)->angled);
static_assert(!includeDirectiveAt("#include \"foo\\\nbar.h\"",0));
static_assert(!includeDirectiveAt("#include_next\\\n \"Foo.h\"",0));
static_assert(!includeDirectiveAt("#include \"Foo.h\"\\\n junk",0));
static_assert(!includeDirectiveAt("code\\\n#include \"Foo.h\"",6));
static_assert(!relatedAutoOpen(1,0,false,true));
static_assert(relatedAutoOpen(1,0,false,false));
static_assert(fileIndexPageMatches(4,2,4,2));
static_assert(!fileIndexPageMatches(4,2,4,3));
static_assert(!fileIndexPageMatches(4,2,5,2));
static_assert(!fileIndexPageMatches(0,2,0,2));
static_assert(!fileIndexPageMatches(4,0,4,0));
static_assert(relatedHelpAnchor(true)=="open-include");
static_assert(relatedHelpAnchor(false)=="header-source");
static_assert(relatedHelpAnchor(false,true)=="related-results");
static_assert(relatedHelpAnchor(true,true)=="open-include");
void require(bool test, const std::source_location location = std::source_location::current()) {
    if (!test) throw std::runtime_error("Header/source navigation contract failed at line " + std::to_string(location.line()));
}
template<class F> void rejects(F action){bool failed=false;try{action();}catch(...){failed=true;}require(failed);}
}
void relatedFileContracts(const std::filesystem::path &base) {
    namespace fs=std::filesystem;const auto root=base/utf8Path("related files été");
    const auto write=[&](const char *path){const auto file=root/utf8Path(path);fs::create_directories(file.parent_path());std::ofstream(file,std::ios::binary)<<"// fixture\n";};
    write("Source/Module/Public/Audio/Actor.h");write("Source/Module/Private/Audio/Actor.cpp");
    write("Source/Module/Private/Actor.cc");write("Source/Module/Public/Audio/Actor.cpp");
    const auto header=root/"Source/Module/Public/Audio/Actor.h";
    const auto result=cppCounterparts(root,header);require(result.at("candidates").size()==3);
    require(result.at("candidates")[0].at("relation")=="sibling");
    require(result.at("candidates")[1].at("relation")=="paired-directory");
    require(result.at("candidates")[2].at("relation")=="paired-root");
    require(result.at("checked").get<size_t>()<=128 && result.at("unreadable")==0 && result.at("truncated")==true);
    require(result.at("index").at("state")=="missing");
    require(cppCounterparts(root,root/"Source/Module/Private/Audio/Actor.cpp").at("candidates").size()==1);
    write("include/library/été.hpp");write("src/library/été.cpp");write("src/été.cc");
    require(cppCounterparts(root,root/utf8Path("include/library/été.hpp")).at("candidates").size()==2);
    write("Alone.h");require(cppCounterparts(root,root/"Alone.h").at("candidates").empty());
    write("Actor.generated.h");rejects([&]{cppCounterparts(root,root/"Actor.generated.h");});
    write("main.cs");rejects([&]{cppCounterparts(root,root/"main.cs");});
    {
        write("src/include-driver.cpp");write("include/lib/Widget.inc");write("alt/lib/Widget.inc");
        write("include/lib/Widget.h");write("include/lib/Widget.cpp");write("include/algorithm");
        write("include/Widget.generated.h");
        const Json paths=Json::array({"include/lib/Widget.inc","alt/lib/Widget.inc","include/lib/Widget.h",
            "include/lib/Widget.cpp","include/algorithm","include/Widget.generated.h"});
        RelatedFileIndex index;index.rebuild(paths);
        const auto matches=index.findInclude(paths,"lib/Widget.inc");
        require(matches.paths.size()==2 && matches.state==RelatedIndexState::ready);
        require(index.findInclude(paths,"Widget.h").paths.size()==1);
        require(index.findInclude(paths,"Widget.generated.h").paths.size()==1);
        require(index.findInclude(paths,"algorithm").paths.size()==1);
        require(index.findInclude(paths,"Widget.cpp").paths.size()==1);
        require(index.findInclude(paths,"lib/Widget.c").paths.empty());
        require(index.findInclude(paths,"../include/lib/Widget.inc").paths.empty());
        require(cppIncludedFiles(root,root/"src/include-driver.cpp","lib/Widget.inc",true,matches).at("candidates").size()==2);
        const auto local=cppIncludedFiles(root,root/"include/lib/Widget.h","Widget.inc",false,index.findInclude(paths,"Widget.inc"));
        require(local.at("candidates").size()==2 && local.at("candidates")[0].at("relation")=="source-relative");
        require(cppIncludedFiles(root,root/"src/include-driver.cpp","../include/lib/Widget.inc",false).at("candidates").size()==1);
        require(cppIncludedFiles(root,root/"src/include-driver.cpp","../../outside-related.cpp",false).at("candidates").empty());
        rejects([&]{cppIncludedFiles(root,root/"src/include-driver.cpp","/outside.h",false);});
        const auto disk="#include \"../include/lib/Widget.inc\"\n";
        std::ofstream(root/"src/include-driver.cpp",std::ios::binary)<<disk;
        Engine engine(root);const auto source="src/include-driver.cpp";
        const auto doc=engine.dispatch("documents.read",{{"path",source}});
        Json parameters={{"path",source},{"expectedRevision",doc.at("revision")},{"expectedDocumentId",doc.at("id")},
                         {"position",{{"line",0},{"character",12}}}};
        const auto result=engine.dispatch("workspace.include",parameters);
        require(result.at("include").at("target")=="../include/lib/Widget.inc" && result.at("candidates").size()==1);
        require(result.at("include").at("range").at("start").at("character")==10);
        require(result.at("index").at("state")=="missing" && result.at("truncated")==true);
        const auto changed=engine.dispatch("documents.replace",{{"path",source},{"expectedRevision",doc.at("revision")},
            {"text","// 😀\n# include <lib/Widget.inc>\n"}});
        rejects([&]{engine.dispatch("workspace.include",parameters);});
        parameters["expectedRevision"]=changed.at("revision");parameters["position"]={{"line",1},{"character",0}};
        require(engine.dispatch("workspace.include",parameters).at("include").at("angled")==true);
        parameters["position"]={{"line",0},{"character",4}}; // Between the emoji surrogate halves.
        rejects([&]{engine.dispatch("workspace.include",parameters);});
        const auto continued=engine.dispatch("documents.replace",{{"path",source},{"expectedRevision",changed.at("revision")},
                                                                  {"text",std::string(continuedInclude)}});
        parameters["expectedRevision"]=continued.at("revision");parameters["position"]={{"line",0},{"character",0}};
        const auto continuedResult=engine.dispatch("workspace.include",parameters);
        require(continuedResult.at("include").at("target")=="été.h" && continuedResult.at("position").at("line")==0);
        require(continuedResult.at("include").at("range")==Json({{"start",{{"line",2},{"character",3}}},{"end",{{"line",2},{"character",8}}}}));
        require(readTextFileBytes(root/"src/include-driver.cpp")==disk);
    }
    {
        // Uses an injected cache, never workspace.files/ripgrep or a test process.
        write("src/Flat.cpp");write("include/library/Flat.hpp");write("other/Flat.hh");
        write("include/library/Flat.generated.h");write("other/Flat.cpp");
        Json paths=Json::array({"include/library/Flat.hpp","other/Flat.hh","other/Flat.cpp",
                                "include/library/Flat.generated.h","src/Flat.cpp"});
        RelatedFileIndex index;index.rebuild(paths);
        auto snapshot=index.find(paths,root/"src/Flat.cpp");
        require(snapshot.state==RelatedIndexState::ready && snapshot.paths.size()==2);
        snapshot.revision=7;snapshot.generation=2;
        const auto report=cppCounterparts(root,root/"src/Flat.cpp",snapshot);
        require(report.at("candidates").size()==2 && report.at("truncated")==false);
        require(report.at("candidates")[0].at("relation")=="workspace-index");
        require(report.at("index").at("generation")==2);
        snapshot.state=RelatedIndexState::stale;
        require(cppCounterparts(root,root/"src/Flat.cpp",snapshot).at("candidates").empty());
        require(index.find(paths,root/"src/Absent.cpp").paths.empty());
        auto reverse=index.find(paths,root/"include/library/Flat.hpp");
        require(reverse.paths.size()==2);
        paths=Json::array({"Source/Module/Private/Audio/Actor.cpp","Source/Module/Private/Actor.cc"});
        index.rebuild(paths);snapshot=index.find(paths,header);
        require(cppCounterparts(root,header,snapshot).at("candidates").size()==3); // Local relations retained.
        require(index.find(paths,root/"src/Flat.cpp").paths.empty()); // Rebuild drops old offsets.
        paths=Json::array({"include/library/été.hpp","../outside.cpp",17,"/absolute.cpp",std::string("bad\0path",8)});
        index.rebuild(paths);snapshot=index.find(paths,root/utf8Path("src/été.cc"));
        require(snapshot.state==RelatedIndexState::partial && snapshot.paths.size()==1);
        paths=Json::array();for(size_t i=0;i<129;++i)paths.push_back(std::to_string(i)+"/Many.hpp");
        index.rebuild(paths);snapshot=index.find(paths,root/"Many.cpp");
        require(snapshot.paths.size()==128 && snapshot.state==RelatedIndexState::partial);
        write("Many.cpp");
        const auto bounded=cppCounterparts(root,root/"Many.cpp",snapshot);
        require(bounded.at("checked")==128 && bounded.at("truncated")==true);
        paths=Json::array({"include/flat.HpP"});index.rebuild(paths);
        require(index.find(paths,root/"src/flat.cpp").paths.size()==1);
#ifdef _WIN32
        require(index.find(paths,root/"src/FLAT.cpp").paths.size()==1);
#else
        require(index.find(paths,root/"src/FLAT.cpp").paths.empty());
#endif
    }
    {
        Engine engine(root);const auto path="Source/Module/Public/Audio/Actor.h";
        Json params={{"path",path},{"expectedRevision",1},{"expectedDocumentId",0}};
        rejects([&]{engine.dispatch("workspace.counterparts",params);}); // No implicit open.
        const auto doc=engine.dispatch("documents.read",{{"path",path}});
        params["expectedRevision"]=doc.at("revision");params["expectedDocumentId"]=doc.at("id");
        const auto report=engine.dispatch("workspace.counterparts",params);
        require(report.at("documentId")==doc.at("id") && report.at("candidates").size()==3);
        require(report.at("index").at("state")=="missing" && report.at("truncated")==true);
        engine.dispatch("documents.replace",{{"path",path},{"expectedRevision",doc.at("revision")},{"text","// dirty\n"}});
        rejects([&]{engine.dispatch("workspace.counterparts",params);});
        require(readTextFileBytes(header)=="// fixture\n");
    }
    const auto outside=base/"outside-related.cpp";std::ofstream(outside,std::ios::binary)<<"// outside\n";
    rejects([&]{cppCounterparts(root,outside);});
    std::error_code error;fs::create_symlink(outside,root/"Alone.cpp",error);
    if(!error) {
        const Json paths=Json::array({"Alone.cpp"});RelatedFileIndex index;index.rebuild(paths);
        const auto snapshot=index.find(paths,root/"Alone.h");
        require(cppCounterparts(root,root/"Alone.h",snapshot).at("candidates").empty());
    }
}

#include "cycode/Engine.h"
#include "cycode/Highlighting.h"
#include "cycode/TextEdits.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
namespace fs=std::filesystem;
static void require(bool condition,const std::string &message) { if (!condition) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream out(path,std::ios::binary); out<<text; }
static Json token(const Json &reply,const std::string &text,const std::string &name) {
    for (const auto &entry : reply.at("tokens")) {
        auto at=utf16Offset(text,{{"line",entry.at("line")},{"character",entry.at("character")}});
        auto end=utf16Offset(text,{{"line",entry.at("line")},{"character",entry.at("character").get<size_t>()+entry.at("length").get<size_t>()}});
        if (text.substr(at,end-at)==name) return entry;
    }
    throw std::runtime_error("Missing real semantic token: "+name+" "+reply.dump());
}
static bool typeToken(const Json &entry) { const auto type=entry.at("type"); return type=="type" || type=="class" || type=="struct"; }
static void decoderTests() {
    Json reply={{"supported",true},{"generation",2},{"legend",{{"tokenTypes",Json::array({"method","variable"})},{"tokenModifiers",Json::array({"readonly"})}}},{"result",{{"data",Json::array({0,3,5,1,1})}}}};
    const auto decoded=decodeHighlights("🛠 value",reply);
    require(decoded.at("tokens")[0].at("glyphStart")==2 && decoded.at("tokens")[0].at("type")=="variable","Legend and UTF-16 decoding");
    for (const auto &bad : {Json::array({0,1,1,0,0}),Json::array({0,3,0,0,0}),Json::array({0,3,50,0,0}),Json::array({0,3,1,99,0}),Json::array({0,3,1,0,4}),Json::array({-1,0,1,0,0}),Json::array({0,3,2,0,0,0,1,1,0,0}),Json::array({0,3})}) {
        auto malformed=reply; malformed["result"]["data"]=bad; bool rejected=false;
        try { (void)decodeHighlights("🛠 value",malformed); } catch (...) { rejected=true; }
        require(rejected,"Malformed semantic token accepted: "+bad.dump());
    }
    require(!decodeHighlights("",{{"supported",false},{"generation",0}}).at("supported").get<bool>(),"Unsupported server fallback");
}
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    const auto root=fs::temp_directory_path()/utf8Path("cycode-highlight-é-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    try {
        decoderTests();
        const std::string header="#pragma once\nstruct Widget { int Field; int Compute(int Parameter) { int FLocal = Parameter; return FLocal + Field; } };\nusing WidgetAlias = Widget;\nenum class State { Ready, Done };\n";
        const std::string source="#include \"types.hpp\"\n#define MY_SCALE(V) ((V)*2)\nint main() { const char* s = \"🛠\"; WidgetAlias Counter{}; int FValue = 3; return Counter.Compute(MY_SCALE(FValue)); }\n#if 0\nint Disabled = 1;\n#endif\n";
        write(root/"types.hpp",header); write(root/"main.cpp",source);
        write(root/"compile_commands.json",Json::array({{{"directory",pathUtf8(root)},{"file",pathUtf8(root/"main.cpp")},{"arguments",Json::array({"clang++","-std=c++20","-I"+pathUtf8(root),"-c",pathUtf8(root/"main.cpp")})}}}).dump());
        {
            Engine engine(root); engine.configureLanguage(argv[1],true,true);
            auto main=engine.dispatch("documents.read",{{"path","main.cpp"}});
            auto types=engine.dispatch("documents.read",{{"path","types.hpp"}});
            auto query=[&](const std::string &path,const Json &doc) { return engine.dispatch("language.highlight",{{"path",path},{"expectedRevision",doc.at("revision")}}); };
            const auto result=query("main.cpp",main), declarations=query("types.hpp",types);
            auto publishedRevision=[&](const Json &document) {
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                while(std::chrono::steady_clock::now()<deadline) {
                    const auto status=engine.languageStatus().at("servers").at("cpp");
                    const auto revisions=status.at("diagnosticRevisions");
                    if(revisions.contains("main.cpp")&&revisions.at("main.cpp").at("revision")==document.at("revision")) {
                        require(status.at("diagnostics").contains(revisions.at("main.cpp").at("uri").get<std::string>()),"Eligible diagnostic publication has no source data");
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                throw std::runtime_error("clangd did not publish diagnostics for the current native revision");
            };
            publishedRevision(main);
            require(result.at("supported") && !result.at("truncated"),"clangd full-token capability");
            require(typeToken(token(result,source,"WidgetAlias")),"Actual alias classification");
            require(token(result,source,"Compute").at("type")=="method","Actual method classification");
            require(token(result,source,"FValue").at("type")=="variable","Locals override Unreal naming guesses");
            require(token(result,source,"MY_SCALE").at("type")=="macro","Actual macro classification");
            require(token(declarations,header,"Field").at("type")=="property","Actual field classification");
            require(token(declarations,header,"Parameter").at("type")=="parameter","Actual parameter classification");
            require(token(declarations,header,"Ready").at("type")=="enumMember","Actual enum member classification");
            require(std::any_of(result.at("tokens").begin(),result.at("tokens").end(),[](const Json &t) { return t.at("type")=="comment" && t.at("line")==4; }),"Inactive code semantic classification");
            auto value=token(result,source,"Counter"); require(value.at("character").get<size_t>()>value.at("glyphStart").get<size_t>(),"Supplementary Unicode columns");
            const auto originalRevision=main.at("revision");
            const auto live=source+"using LocalAlias = Widget;\n";
            main=engine.dispatch("documents.replace",{{"path","main.cpp"},{"expectedRevision",originalRevision},{"text",live}});
            (void)query("main.cpp",main);publishedRevision(main);
            require(typeToken(token(query("main.cpp",main),live,"LocalAlias")),"Unsaved aliases use live document");
            bool stale=false;
            try { engine.dispatch("language.highlight",{{"path","main.cpp"},{"expectedRevision",originalRevision}}); } catch (const std::exception &e) { stale=std::string(e.what()).starts_with("RevisionConflict"); }
            require(stale,"Stale highlight revision rejection");
            types=engine.dispatch("documents.replace",{{"path","types.hpp"},{"expectedRevision",types.at("revision")},{"text",header+"using AnotherAlias = Widget;\n"}});
            const auto refreshed=query("main.cpp",main);
            require(refreshed.at("generation").get<size_t>()>result.at("generation").get<size_t>(),"Header overlay invalidates semantic generation");
            engine.configureLanguage(argv[1],true,true);
            const auto restarted=query("main.cpp",main);
            require(restarted.at("generation").get<size_t>()>refreshed.at("generation").get<size_t>(),"Restart invalidates semantic generation");
            require(typeToken(token(restarted,live,"LocalAlias")),"Restart replays unsaved highlights");
            require(engine.dispatch("documents.read",{{"path","main.cpp"}}).at("text")==live,"Highlighting never changes live text");
        }
        std::ifstream disk(root/"main.cpp",std::ios::binary); const std::string unchanged((std::istreambuf_iterator<char>(disk)),{}); disk.close();
        require(unchanged==source,"Highlighting never saves source"); fs::remove_all(root);
        std::cout << "Real clangd semantic colors: aliases/methods/macros/fields/parameters/enums, inactive code, Unicode, live overlays, revision guards, header invalidation and restart passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; fs::remove_all(root); return 1; }
}

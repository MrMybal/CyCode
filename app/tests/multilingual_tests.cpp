#include "cycode/Engine.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs=std::filesystem;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream stream(path); stream<<text; }
static bool wait(const std::function<bool()> &predicate,int seconds=30) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    do { if (predicate()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(50)); } while (std::chrono::steady_clock::now()<end);
    return false;
}
int main(int argc,char **argv) {
    const std::string mode=argc>1 ? argv[1] : "typescript";
    const std::string backend=mode=="javascript" ? "typescript" : mode;
    const auto root=fs::temp_directory_path()/("cycode-languages-"+backend+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto launch=defaultServerLaunch(root,backend);
    if (launch.argv.empty()) { std::cout<<"Install "<<backend<<" with app/scripts/setup_languages.py\n"; fs::remove_all(root); return 77; }
    try {
        std::string path,text,completionText,prefix,expected;
        Json position;
        if (mode=="javascript") {
            write(root/"jsconfig.json",R"({"compilerOptions":{"target":"ES2020","checkJs":true},"include":["*.js"]})");
            path="main.js";text="const message = 'hello';\nfunction greet(value) { return message; }\ngreet(1);\n";
            completionText=text+"message.toU\n";expected="toUpperCase";position={{"line",3},{"character",11}};
        } else if (backend=="typescript") {
            write(root/"tsconfig.json",R"({"compilerOptions":{"target":"ES2020","strict":true},"include":["*.ts","*.js"]})");
            write(root/"model.ts","export function greet(name: string): string { return name; }\n");
            write(root/"consumer.ts","import { greet } from './model';\nexport const value = greet('consumer');\n");
            path="main.ts";text="import { greet } from './model';\nconst answer: string = greet('hello');\nconst broken: number = 'bad';\n";
            completionText=text+"answer.toU\n";prefix="answer.toU";expected="toUpperCase";position={{"line",3},{"character",10}};
        } else if (backend=="html") {
            path="index.html";text="<main class=\"card\"><h1>Hello</h1></main>\n";
            completionText="<main><di";expected="div";position={{"line",0},{"character",9}};
        } else if (backend=="css") {
            path="style.css";text=".card{ color: red; unknown-property: 4; }\n";
            completionText=".card { col";expected="color";position={{"line",0},{"character",11}};
        } else if (backend=="rust") {
            write(root/"Cargo.toml","[package]\nname = \"cycode_test\"\nversion = \"0.1.0\"\nedition = \"2021\"\n");
            fs::create_directory(root/"src");path="src/main.rs";
            text="fn greet(value: i32) -> i32 { value + 1 }\nfn main() { let answer = greet(2); }\n";
            completionText="fn greet(value: i32) -> i32 { value + 1 }\nfn main() { gre }\n";expected="greet";position={{"line",1},{"character",15}};
        } else {
            path="Main.java";text="class Main {\n static int greet(int value) { return value + 1; }\n void run() { int answer = greet(2); }\n}\n";
            completionText="class Main {\n static int greet(int value) { return value + 1; }\n void run() { gre }\n}\n";expected="greet";position={{"line",2},{"character",17}};
        }
        write(root/path,text);
        {
            Engine engine(root); engine.configureLanguage("",false,false);
            auto doc=engine.dispatch("documents.read",{{"path",path}});
            require(!engine.languageStatus().at("servers").at(backend).at("enabled").get<bool>(),"Inspection enabled a backend");
            engine.dispatch("language.configure",{{"language",backend}});
            require(wait([&] { return engine.languageStatus().at("servers").at(backend).at("ready").get<bool>(); },backend=="java" ? 80 : 30),"Server never ready: "+engine.languageStatus().dump());
            const auto symbols=engine.dispatch("language.documentSymbols",{{"path",path},{"expectedRevision",doc.at("revision")}});
            require(!symbols.at("symbols").empty(),"Document symbols missing: "+symbols.dump());
            const auto hoverName=backend=="html" ? "main" : backend=="css" ? "color" : "greet";
            const auto offset=text.rfind(hoverName)+1;
            auto atPosition=[&](size_t at) {
                const auto before=text.substr(0,at); const auto newline=before.rfind('\n');
                return Json{{"line",std::count(before.begin(),before.end(),'\n')},{"character",LanguageClient::utf16Length(before.substr(newline==std::string::npos ? 0 : newline+1))}};
            };
            Json informationQuery={{"path",path},{"expectedRevision",doc.at("revision")},{"position",atPosition(offset)}};
            const auto methods=engine.languageStatus().at("servers").at(backend).at("methods");
            require(methods.at("hover").get<bool>(),"Installed server unexpectedly omitted hover support");
            Json hovered; require(wait([&] { hovered=engine.dispatch("language.hover",informationQuery);return hovered.at("available").get<bool>(); }),"Actual symbol documentation missing: "+hovered.dump());
            if (methods.at("signature").get<bool>()) {
                const auto call=text.rfind("greet(");
                if (call!=std::string::npos) {
                    informationQuery["position"]=atPosition(call+7);
                    Json signature;
                    const bool available=wait([&] { signature=engine.dispatch("language.signature",informationQuery);return signature.at("available").get<bool>(); });
                    require(available,"Actual callable signature missing: "+signature.dump());
                } else require(!engine.dispatch("language.signature",informationQuery).at("available").get<bool>(),"Tag/property unexpectedly returned callable information");
            } else {
                bool unsupported=false;try { engine.dispatch("language.signature",informationQuery); } catch (const std::exception &e) { unsupported=std::string(e.what()).find("does not support")!=std::string::npos; }
                require(unsupported,"Unsupported signature operation was not rejected explicitly");
            }
            auto formatted=engine.dispatch("language.format",{{"path",path},{"expectedRevision",doc.at("revision")}});
            require(formatted.is_object(),"Format preview missing");
            if (backend=="rust" || backend=="java") {
                const auto usage=backend=="rust" ? Json{{"line",1},{"character",26}} : Json{{"line",2},{"character",27}};
                const Json query={{"path",path},{"expectedRevision",doc.at("revision")},{"position",usage},{"includeDeclaration",true}};
                Json definition;
                require(wait([&] { definition=engine.dispatch("language.definition",query); return !definition.at("locations").empty(); }),"Definition missing: "+definition.dump());
                const auto references=engine.dispatch("language.references",query);
                require(references.at("locations").size()>=2,"References missing: "+references.dump());
                auto renameQuery=query; renameQuery["newName"]="welcome";
                const auto rename=engine.dispatch("language.rename",renameQuery);
                require(rename.at("editCount").get<int>()>0,"Rename missing: "+rename.dump());
                const auto preview=engine.dispatch("changes.preview",{{"id",rename.at("id")},{"path",path}});
                auto renamed=text; for (size_t at=0;(at=renamed.find("greet",at))!=std::string::npos;at+=7) renamed.replace(at,5,"welcome");
                require(preview.at("after")==renamed,"Rename changed more than the selected symbol: "+preview.dump());
                engine.dispatch("changes.apply",{{"id",rename.at("id")}});
                engine.dispatch("changes.undo",{{"id",rename.at("id")}});
                doc=engine.dispatch("documents.read",{{"path",path}});
                const auto highlight=engine.dispatch("language.highlight",{{"path",path},{"expectedRevision",doc.at("revision")}});
                require(!highlight.at("supported").get<bool>() || !highlight.at("tokens").empty(),"Advertised semantic colors missing");
            }
            if (backend=="css") {
                const auto rename=engine.dispatch("language.rename",{{"path",path},{"expectedRevision",doc.at("revision")},{"position",{{"line",0},{"character",2}}},{"newName","card-new"}});
                const auto preview=engine.dispatch("changes.preview",{{"id",rename.at("id")},{"path",path}});
                require(preview.at("after").get<std::string>().starts_with(".card-new"),"CSS selector rename failed");
            }
            if (mode=="typescript") {
                Json location;
                require(wait([&] { location=engine.dispatch("language.definition",{{"path",path},{"expectedRevision",doc.at("revision")},{"position",{{"line",1},{"character",23}}}}); return location.at("locations").dump().find("model.ts")!=std::string::npos; }),"Cross-file definition missing: "+location.dump()+" status: "+engine.languageStatus().dump());
                const auto references=engine.dispatch("language.references",{{"path",path},{"expectedRevision",doc.at("revision")},{"position",{{"line",1},{"character",23}}},{"includeDeclaration",true}});
                require(references.at("locations").size()>=2,"References missing");
                const auto model=engine.dispatch("documents.read",{{"path","model.ts"}});
                const auto rename=engine.dispatch("language.rename",{{"path","model.ts"},{"expectedRevision",model.at("revision")},{"position",{{"line",0},{"character",17}}},{"newName","welcome"}});
                require(rename.at("files").size()==3,"Cross-file rename preview missing: "+rename.dump());
                engine.dispatch("changes.apply",{{"id",rename.at("id")}});
                require(engine.dispatch("documents.read",{{"path","model.ts"}}).at("text").get<std::string>().find("welcome")!=std::string::npos,"Rename not applied to live buffer");
                engine.dispatch("changes.undo",{{"id",rename.at("id")}});
                doc=engine.dispatch("documents.read",{{"path",path}});
                require(wait([&] { return engine.languageStatus().at("servers").at(backend).at("diagnostics").dump().find("2322")!=std::string::npos; }),"Type diagnostics missing");
            }
            doc=engine.dispatch("documents.replace",{{"path",path},{"expectedRevision",doc.at("revision")},{"text",completionText}});
            Json completion;
            require(wait([&] {
                completion=engine.dispatch("language.complete",{{"path",path},{"expectedRevision",doc.at("revision")},{"position",position}});
                return completion.dump().find(expected)!=std::string::npos;
            },30),"Completion missing "+expected+": "+completion.dump());
            const auto &items=completion.at("result").at("items");
            auto selected=std::find_if(items.begin(),items.end(),[&](const Json &item) { return item.at("label").get<std::string>().starts_with(expected); });
            require(selected!=items.end(),"Completion insertion candidate missing");
            const auto prepared=engine.dispatch("language.completionPrepare",{{"sessionId",completion.at("sessionId")},{"item",std::distance(items.begin(),selected)},{"expectedRevision",doc.at("revision")}});
            require(prepared.at("plan").at("editCount").get<int>()>0,"Completion insertion preview empty");
            const auto preview=engine.dispatch("changes.preview",{{"id",prepared.at("plan").at("id")},{"path",path}});
            require(preview.at("after").get<std::string>().find(expected)!=std::string::npos,"Prepared completion omitted selected symbol");
            engine.dispatch("changes.apply",{{"id",prepared.at("plan").at("id")}});engine.dispatch("changes.undo",{{"id",prepared.at("plan").at("id")}});
            require(std::ifstream(root/path).good(),"Live update damaged file");
            engine.dispatch("language.configure",{{"language",backend},{"enabled",false}});
            require(wait([&] { return !engine.languageStatus().at("servers").at(backend).at("running").get<bool>(); },5),"Disable retained child process");
            bool disabled=false;try { engine.dispatch("language.complete",{{"path",path},{"expectedRevision",doc.at("revision")},{"position",position}}); } catch (...) { disabled=true; }
            require(disabled,"Disabled server accepted request");
            std::cout<<backend<<": actual server symbols, formatting, live completion and shutdown passed\n";
        }
        // Windows may briefly retain a just-terminated JVM's mapped cache files.
        std::error_code cleanup;
        require(wait([&] { cleanup.clear(); fs::remove_all(root,cleanup); return !cleanup; },3),"Language workspace cleanup failed: "+cleanup.message());
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}

#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <cctype>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace cycode;
namespace fs=std::filesystem;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f,const std::string &message) { bool refused=false;try { f(); } catch (...) { refused=true; }require(refused,message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream stream(path,std::ios::binary);stream<<text; }
static Json pos(int character) { return {{"line",0},{"character",character}}; }
static Json range(int a,int b) { return {{"start",pos(a)},{"end",pos(b)}}; }
static int fake() {
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    std::map<std::string,int> versions;
    auto send=[](const Json &message) { const auto body=message.dump();std::cout<<"Content-Length: "<<body.size()<<"\r\n\r\n"<<body<<std::flush; };
    for (;;) {
        std::string header;size_t size=0;while (std::getline(std::cin,header) && header!="\r" && !header.empty()) if (header.starts_with("Content-Length:")) size=std::stoull(header.substr(15));
        if (!std::cin || size>4*1024*1024) return 0;std::string body(size,'\0');std::cin.read(body.data(),size);if (!std::cin) return 0;
        const auto query=Json::parse(body);const auto method=query.value("method","");const auto params=query.value("params",Json::object());Json result=nullptr;
        if (method=="textDocument/didOpen" || method=="textDocument/didChange") {
            const auto &doc=params.at("textDocument");const auto uri=doc.at("uri").get<std::string>();versions[uri]=doc.at("version");
            if (uri.ends_with("main.cpp")) {
                Json diagnostics={{"uri",uri},{"diagnostics",Json::array({{{"range",range(4,9)},{"message","Typo"},{"code",1}}})}};
                diagnostics["version"]=versions[uri]-(fs::exists("stale-diagnostic") ? 1 : 0);
                auto alias=uri;
#ifdef _WIN32
                if (alias.starts_with("file:///") && alias.size()>9 && alias[9]==':') { alias[8]=char(std::tolower(static_cast<unsigned char>(alias[8])));alias.replace(9,1,"%3A"); }
#endif
                diagnostics["uri"]=alias;
                send({{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},{"params",diagnostics}});
            }
        }
        if (!query.contains("id")) continue;
        if (method=="initialize") {
            require(params.at("capabilities").at("textDocument").at("codeAction").at("dataSupport")==true,"Missing code action capabilities");
            require(params.at("capabilities").at("workspace").at("workspaceEdit").at("changeAnnotationSupport").at("groupsOnLabel")==false,"Missing annotation capability");
            require(params.at("capabilities").at("textDocument").at("codeAction").at("honorsChangeAnnotations")==true &&
                    params.at("capabilities").at("textDocument").at("rename").at("honorsChangeAnnotations")==true,"Annotations were not negotiated for actions/rename");
            result={{"capabilities",{{"codeActionProvider",{{"resolveProvider",true}}}}}};
        } else if (method=="workspace/executeCommand") { write("command-executed",""); }
        else if (method=="textDocument/codeAction") {
            const auto main=params.at("textDocument").at("uri").get<std::string>(),other=LanguageClient::uri(fs::current_path()/"other.hpp");
            write("request-context",params.at("context").dump());
            const auto edits=Json{{"changes",{{main,Json::array({{{"range",range(4,9)},{"newText","value"}}})},{other,Json::array({{{"range",range(4,7)},{"newText","new"}}})}}}};
            result=Json::array({{{"title","Fix two files"},{"kind","quickfix"},{"edit",edits}},
                {{"title","Lazy fix"},{"kind","quickfix"},{"data",{{"id",7}}}},
                {{"title","Command action"},{"kind","quickfix"},{"command",{{"command","danger.command"}}}},
                {{"title","Disabled action"},{"kind","quickfix"},{"disabled",{{"reason","Not here"}}}},
                {{"title","Create file"},{"kind","quickfix"},{"edit",{{"documentChanges",Json::array({{{"kind","create"},{"uri",other}}})}}}},
                {{"title","Outside edit"},{"kind","quickfix"},{"edit",{{"changes",{{LanguageClient::uri(fs::current_path().parent_path()/"outside.cpp"),Json::array()}}}}}},
                {{"title","Overlapping edit"},{"kind","quickfix"},{"edit",{{"changes",{{main,Json::array({{{"range",range(4,8)},{"newText","one"}},{{"range",range(5,9)},{"newText","two"}}})}}}}}},
                {{"title","Lazy target fix"},{"kind","quickfix"},{"data",{{"id",8}}}}});
            Json annotated={{"changeAnnotations",{{"server-confirm",{{"label","Rename both files"},{"description","Explicit review"},{"needsConfirmation",true}}}}},
                            {"documentChanges",Json::array()}};
            for(const auto &[uri,entries]:edits.at("changes").items()) {
                auto tagged=entries;for(auto &entry:tagged)entry["annotationId"]="server-confirm";
                annotated["documentChanges"].push_back({{"textDocument",{{"uri",uri},{"version",versions.contains(uri)?Json(versions.at(uri)):Json()}}},{"edits",tagged}});
            }
            result.push_back({{"title","Annotated fix"},{"kind","quickfix"},{"edit",annotated}});
            if (fs::exists("many-actions")) for (int i=0;i<300;++i) result.push_back(result[0]);
        } else if (method=="codeAction/resolve") {
            result=params;require(!result.contains("cycodeLanguage"),"Internal language route leaked");
            const auto uri=LanguageClient::uri(fs::current_path()/"main.cpp");
            result["edit"]={{"documentChanges",Json::array({{{"textDocument",{{"uri",uri},{"version",versions[uri]-(fs::exists("stale-version") ? 1 : 0)}}},{"edits",Json::array({{{"range",range(4,9)},{"newText","value"}}})}}})}};
            if (params.at("data").at("id")==8) result["edit"]={{"changes",{{LanguageClient::uri(fs::current_path()/"lazy.hpp"),Json::array({{{"range",range(4,7)},{"newText","new"}}})}}}};
            if (fs::exists("mutate-title")) result["title"]="Wrong action";
            if (fs::exists("delay-resolve")) { write("resolve-received","");const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while (!fs::exists("release-resolve") && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
        }
        send({{"jsonrpc","2.0"},{"id",query.at("id")},{"result",result}});
    }
}
int main(int argc,char **argv) {
    if (argc>1 && std::string(argv[1])=="--background-index") return fake();
    const std::string mode=argc>1 ? argv[1] : "fake";const bool actual=mode!="fake";
    const auto root=fs::temp_directory_path()/("cycode-actions-"+mode+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    try {
        const auto path=mode=="java" ? "Main.java" : mode=="rust" ? "src/main.rs" : mode=="typescript" ? "main.ts" : "main.cpp";
        const std::string source=mode=="cpp" ? "int main() { int value = 1 return value; }\n" : mode=="typescript" ? "const answer = greet('hello');\n" : mode=="java" ? "class Main { void run() { ArrayList<String> values; } }\n" : mode=="rust" ? "fn main() { let value = 1; let answer = value; }\n" : "int vlaue;\n";
        if (mode=="rust") { fs::create_directory(root/"src");write(root/"Cargo.toml","[package]\nname=\"actions\"\nversion=\"0.1.0\"\nedition=\"2021\"\n"); }
        if (mode=="typescript") { write(root/"model.ts","export function greet(name: string) { return name; }\n");write(root/"tsconfig.json",R"({"compilerOptions":{"strict":true},"include":["*.ts"]})"); }
        write(root/path,source);write(root/"other.hpp","int old;\n");write(root/"lazy.hpp","int old;\n");
        if (!actual) {
            LanguageClient transport;require(transport.start(root,pathUtf8(fs::absolute(utf8Path(argv[0])))),"Transport did not start");
            auto waitFor=[&](auto predicate) { const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);while (!predicate() && std::chrono::steady_clock::now()<until) { transport.poll();std::this_thread::sleep_for(std::chrono::milliseconds(5)); }require(predicate(),"Transport publication timed out"); };
            waitFor([&] { return transport.initialized(); });const auto uri=LanguageClient::uri(root/path);
            transport.notify("textDocument/didOpen",{{"textDocument",{{"uri",uri},{"languageId","cpp"},{"version",1},{"text",source}}}});
            waitFor([&] { return !transport.diagnosticVersions().empty(); });
            transport.notify("textDocument/didChange",{{"textDocument",{{"uri",uri},{"version",2}}},{"contentChanges",Json::array({{{"text",source+"\n"}}})}});
            require(!transport.diagnostics().empty() && transport.diagnosticVersions().empty(),"Typing must retain display diagnostics while invalidating action context");transport.stop();
        }
        {
            Engine engine(root);if (!actual) engine.configureLanguage(pathUtf8(fs::absolute(utf8Path(argv[0]))),true,true);
            auto doc=engine.dispatch("documents.read",{{"path",path}});
            if (actual && mode!="cpp") engine.dispatch("language.configure",{{"language",mode}});
            const auto kind=mode=="rust" ? "refactor.rewrite" : mode=="java" ? "source.organizeImports" : "quickfix";
            auto list=[&] { return engine.dispatch("language.codeActions",{{"path",path},{"range",range(0,static_cast<int>(source.size()-1))},{"expectedRevision",doc.at("revision")},{"only",Json::array({kind})}}); };
            Json actions;size_t selected=0;const auto title=mode=="cpp" ? ";" : mode=="typescript" ? "Add import" : mode=="java" ? "Organize imports" : mode=="rust" ? "Change visibility to pub(crate)" : "Fix two files";
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(100);bool found=false;
            do { actions=list();for (const auto &item : actions.at("actions")) if (item.at("title").get<std::string>().find(title)!=std::string::npos && item.at("available")==true) { selected=item.at("index");found=true;break; }if (!found) std::this_thread::sleep_for(std::chrono::milliseconds(100)); } while (!found && std::chrono::steady_clock::now()<end);
            require(found,"Actual action missing: "+actions.dump()+" server: "+engine.languageStatus().dump());
            auto query=Json{{"sessionId",actions.at("sessionId")},{"item",selected},{"expectedRevision",doc.at("revision")}};
            const auto plan=engine.dispatch("language.codeActionPrepare",query);require(plan.at("editCount").get<int>()>0,"Empty fix plan: "+plan.dump());
            const auto preview=engine.dispatch("changes.preview",{{"id",plan.at("id")},{"path",path}});
            require(engine.dispatch("documents.read",{{"path",path}}).at("text")==source,"Prepare changed source");
            Json confirmations=Json::array();
            for(const auto &annotation:plan.at("annotations"))if(annotation.at("needsConfirmation")==true)confirmations.push_back(annotation.at("id"));
            engine.dispatch("changes.apply",{{"id",plan.at("id")},{"confirmedAnnotations",confirmations}});require(engine.dispatch("documents.read",{{"path",path}}).at("text")==preview.at("after"),"Apply mismatch");
            const auto after=preview.at("after").get<std::string>();
            require(mode=="typescript" ? after.find("import { greet }")!=std::string::npos : mode=="java" ? after.find("import java.util.ArrayList;")!=std::string::npos : mode=="rust" ? after.starts_with("pub(crate)") : mode=="cpp" ? after=="int main() { int value = 1; return value; }\n" : after.find("vlaue")==std::string::npos,"Wrong actual correction: "+after);
            require(readTextFileBytes(root/path)==source,"Action saved disk source");
            engine.dispatch("changes.undo",{{"id",plan.at("id")}});doc=engine.dispatch("documents.read",{{"path",path}});require(doc.at("text")==source,"Undo mismatch");
            rejects([&] { engine.dispatch("language.codeActionPrepare",query); },"Old action ticket survived apply/undo");
            if (!actual) {
                require(plan.at("files").size()==2,"Multi-file plan missing");
                auto fresh=[&](size_t item) { const auto response=list();return Json{{"sessionId",response.at("sessionId")},{"item",item},{"expectedRevision",doc.at("revision")}}; };
                const auto annotatedPlan=engine.dispatch("language.codeActionPrepare",fresh(8));
                const auto secondPlan=engine.dispatch("language.codeActionPrepare",fresh(8));
                const auto annotationId=annotatedPlan.at("annotations")[0].at("id");
                require(annotationId!=secondPlan.at("annotations")[0].at("id"),"Annotation IDs survived a new plan");
                rejects([&]{engine.dispatch("changes.apply",{{"id",annotatedPlan.at("id")}});},"Required annotation was not confirmed");
                rejects([&]{engine.dispatch("changes.apply",{{"id",secondPlan.at("id")},{"confirmedAnnotations",{annotationId}}});},"Another plan's acknowledgement was accepted");
                require(engine.dispatch("documents.read",{{"path",path}}).at("text")==source,"Failed confirmation changed buffers");
                const auto annotatedPreview=engine.dispatch("changes.preview",{{"id",annotatedPlan.at("id")},{"path",path}});
                require(annotatedPreview.at("annotations")[0].at("files")==Json::array({path}),"Preview did not filter annotation files");
                engine.dispatch("changes.apply",{{"id",annotatedPlan.at("id")},{"confirmedAnnotations",{annotationId}}});
                engine.dispatch("changes.undo",{{"id",annotatedPlan.at("id")}});
                doc=engine.dispatch("documents.read",{{"path",path}});
                auto q=fresh(1);const auto lazy=engine.dispatch("language.codeActionPrepare",q);require(lazy.at("editCount")==1,"Versioned resolved edit missing");
                const auto diagnosticsDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
                while (Json::parse(readTextFileBytes(root/"request-context")).at("diagnostics").empty() && std::chrono::steady_clock::now()<diagnosticsDeadline) { std::this_thread::sleep_for(std::chrono::milliseconds(10));list(); }
                require(!Json::parse(readTextFileBytes(root/"request-context")).at("diagnostics").empty(),"Equivalent diagnostic URI was not matched");
                q=fresh(7);rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Unsynchronized resolve target accepted");
                rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Ticket survived new target synchronization");
                q=fresh(7);require(engine.dispatch("language.codeActionPrepare",q).at("editCount")==1,"Synchronized lazy target rejected");
                q=fresh(0);auto other=engine.dispatch("documents.read",{{"path","other.hpp"}});
                other=engine.dispatch("documents.replace",{{"path","other.hpp"},{"expectedRevision",other.at("revision")},{"text","int old;\n// live\n"}});
                rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Ticket survived another live document change");
                engine.dispatch("documents.replace",{{"path","other.hpp"},{"expectedRevision",other.at("revision")},{"text","int old;\n"}});
                for (size_t i : {2,3,4,5,6}) { q=fresh(i);rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Unsafe/disabled action accepted"); }
                require(!fs::exists(root/"command-executed"),"Server command ran");
                q=fresh(0);q["sessionId"]="forged";rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Forged ticket accepted");
                q=fresh(0);for (int i=0;i<9;++i) fresh(0);rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Evicted ticket accepted");
                q=fresh(1);write(root/"stale-version","");rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Stale server version accepted");fs::remove(root/"stale-version");
                q=fresh(1);write(root/"mutate-title","");rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Resolved action identity changed");fs::remove(root/"mutate-title");
                q=fresh(0);const auto conflict=engine.dispatch("language.codeActionPrepare",q);write(root/"other.hpp","// external\n");rejects([&] { engine.dispatch("changes.apply",{{"id",conflict.at("id")}}); },"Disk conflict partially applied");require(engine.dispatch("documents.read",{{"path",path}}).at("text")==source,"Partial multi-file application");write(root/"other.hpp","int old;\n");
                write(root/"many-actions","");require(list().at("truncated")==true,"Action list bound missing");fs::remove(root/"many-actions");
                q=fresh(1);write(root/"delay-resolve","");auto delayed=std::async(std::launch::async,[&] { return engine.dispatch("language.codeActionPrepare",q); });
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while (!fs::exists(root/"resolve-received") && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));require(fs::exists(root/"resolve-received"),"Delayed action not received");
                doc=engine.dispatch("documents.replace",{{"path",path},{"expectedRevision",doc.at("revision")},{"text",source+"\n"}});write(root/"release-resolve","");rejects([&] { delayed.get(); },"Obsolete resolved action accepted");fs::remove(root/"delay-resolve");
                write(root/"stale-diagnostic","");doc=engine.dispatch("documents.replace",{{"path",path},{"expectedRevision",doc.at("revision")},{"text",source}});list();require(Json::parse(readTextFileBytes(root/"request-context")).at("diagnostics").empty(),"Obsolete versioned diagnostics forwarded");
                q=fresh(0);engine.configureLanguage(pathUtf8(fs::absolute(utf8Path(argv[0]))),true,true);rejects([&] { engine.dispatch("language.codeActionPrepare",q); },"Action survived server restart");
            }
            engine.configureLanguage("",false,false);
            if (actual && mode!="cpp") { engine.dispatch("language.configure",{{"language",mode},{"enabled",false}});const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while (engine.languageStatus().at("servers").at(mode).at("running")==true && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
        }
        std::error_code cleanup;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);do { cleanup.clear();fs::remove_all(root,cleanup);if (!cleanup) break;std::this_thread::sleep_for(std::chrono::milliseconds(30)); } while (std::chrono::steady_clock::now()<deadline);require(!cleanup,cleanup.message());
        std::cout<<mode<<": actual action preview/apply/undo, unsaved source and guards passed\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}

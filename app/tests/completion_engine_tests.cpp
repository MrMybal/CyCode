#include "cycode/Engine.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace cycode;
namespace fs=std::filesystem;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f,const std::string &message) { bool refused=false;try { f(); } catch (...) { refused=true; }require(refused,message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream stream(path,std::ios::binary);stream<<text; }
static Json pos(int line,int character) { return {{"line",line},{"character",character}}; }
static int fake() {
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    for (;;) {
        std::string header;size_t size=0;
        while (std::getline(std::cin,header) && header!="\r" && !header.empty()) if (header.starts_with("Content-Length:")) size=std::stoull(header.substr(15));
        if (!std::cin || size>4*1024*1024) return 0;
        std::string body(size,'\0');std::cin.read(body.data(),size);if (!std::cin) return 0;
        const auto query=Json::parse(body);if (!query.contains("id")) continue;Json result=nullptr;
        const auto method=query.value("method","");
        if (method=="initialize") {
            const auto item=query.at("params").at("capabilities").at("textDocument").at("completion").at("completionItem");
            require(item.at("insertReplaceSupport")==true && item.at("resolveSupport").at("properties").dump().find("additionalTextEdits")!=std::string::npos,"Missing resolve client capabilities");
            result={{"capabilities",{{"completionProvider",{{"resolveProvider",true}}}}}};
        } else if (method=="textDocument/completion") result=Json::array({{{"label","greet"},{"insertText","greet(${1:🛠}, $1, ${2|one,two|})$0"},{"insertTextFormat",2},{"data",{{"id",1}}}}});
        else if (method=="completionItem/resolve") {
            result=query.at("params");require(!result.contains("cycodeLanguage"),"Internal route leaked to server");
            result["documentation"]="A resolved symbol";result["additionalTextEdits"]=Json::array({{{"range",{{"start",pos(0,0)},{"end",pos(0,0)}}},{"newText","// import greet\n"}}});
            if (fs::exists("mutate-completion")) result["insertText"]="wrong";
            if (fs::exists("delay-resolve")) {
                write("resolve-received","");const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
                while (!fs::exists("release-resolve") && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        const auto response=Json{{"jsonrpc","2.0"},{"id",query.at("id")},{"result",result}}.dump();
        std::cout<<"Content-Length: "<<response.size()<<"\r\n\r\n"<<response<<std::flush;
    }
}
int main(int argc,char **argv) {
    if (argc>1 && std::string(argv[1])=="--background-index") return fake();
    const std::string mode=argc>1 ? argv[1] : "fake";
    const auto root=fs::temp_directory_path()/("cycode-completion-"+mode+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
    try {
        const bool actual=mode!="fake";
        const std::string path=mode=="java" ? "Main.java" : mode=="typescript" ? "main.ts" : mode=="rust" ? "src/main.rs" : "main.cpp";
        const std::string source=mode=="java" ? "class Main {\n void run() { ArrayLis }\n}\n" : mode=="rust" ? "mod model;\nfn main() { gre }\n" : "// 🛠\n\tgre\n";
        const auto position=mode=="java" ? pos(1,22) : mode=="rust" ? pos(1,15) : pos(1,4);
        if (mode=="rust") { fs::create_directory(root/"src");write(root/"Cargo.toml","[package]\nname = \"completion_import\"\nversion = \"0.1.0\"\nedition = \"2021\"\n");write(root/"src/model.rs","pub fn greet() {}\n"); }
        write(root/path,source);
        if (mode=="typescript") { write(root/"model.ts","export function greet(name: string): string { return name; }\n");write(root/"tsconfig.json",R"({"compilerOptions":{"target":"ES2020","strict":true},"include":["*.ts"]})"); }
        {
            Engine engine(root);
            engine.configureLanguage(actual ? "" : pathUtf8(fs::absolute(utf8Path(argv[0]))),!actual,true);
            auto doc=engine.dispatch("documents.read",{{"path",path}});
            if (actual) engine.dispatch("language.configure",{{"language",mode}});
            Json completion;size_t selected=0;
            auto request=[&] { return engine.dispatch("language.complete",{{"path",path},{"position",position},{"expectedRevision",doc.at("revision")}}); };
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(mode=="java" ? 110 : 30);
            bool found=false;
            do {
                completion=request();const auto &items=completion.at("result").at("items");
                for (size_t i=0;i<items.size();++i) if (items[i].at("label").get<std::string>().starts_with(mode=="java" ? "ArrayList" : "greet")) { selected=i;found=true;break; }
                if (!found) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } while (!found && std::chrono::steady_clock::now()<deadline);
            require(found,"Missing completion: "+completion.dump());
            Json parameters={{"sessionId",completion.at("sessionId")},{"item",selected},{"expectedRevision",doc.at("revision")}};
            const auto resolved=engine.dispatch("language.completionResolve",parameters);
            require(resolved.at("resolved")==true,"Item was not resolved");
            require(!resolved.at("item").value("additionalTextEdits",Json::array()).empty(),"Actual auto-import edits missing: "+resolved.dump());
            const auto prepared=engine.dispatch("language.completionPrepare",parameters);const auto id=prepared.at("plan").at("id");
            require(prepared.at("plan").at("editCount").get<int>()>=2,"Symbol/import not combined");
            const auto preview=engine.dispatch("changes.preview",{{"id",id},{"path",path}});
            if (!actual) {
                const auto &snippet=prepared.at("snippet");require(snippet.at("fields").size()==2,"Native snippet fields missing");
                const auto after=preview.at("after").get<std::string>();
                for (const auto &range : snippet.at("fields")[0].at("ranges")) require(after.substr(range.at("start"),range.at("end").get<size_t>()-range.at("start").get<size_t>())=="🛠","Import shifted snippet field offsets");
            }
            require(preview.at("before")==source && preview.at("after").get<std::string>().find(mode=="java" ? "import java.util.ArrayList;" : mode=="rust" ? "model::greet" : actual ? "import { greet }" : "// import greet")!=std::string::npos,"Import preview incorrect: "+preview.dump());
            require(engine.dispatch("documents.read",{{"path",path}}).at("text")==source,"Prepare mutated buffer");
            engine.dispatch("changes.apply",{{"id",id}});
            require(engine.dispatch("documents.read",{{"path",path}}).at("text")==preview.at("after"),"Atomic apply failed");
            require(readTextFileBytes(root/path)==source,"Completion wrote disk");
            engine.dispatch("changes.undo",{{"id",id}});doc=engine.dispatch("documents.read",{{"path",path}});require(doc.at("text")==source,"Undo failed");
            rejects([&] { engine.dispatch("language.completionPrepare",parameters); },"Stale session accepted");
            parameters["expectedRevision"]=doc.at("revision");rejects([&] { engine.dispatch("language.completionPrepare",parameters); },"Reusing a session after undo accepted");
            if (!actual) {
                auto fresh=[&] { const auto value=request();return Json{{"sessionId",value.at("sessionId")},{"item",0},{"expectedRevision",doc.at("revision")}}; };
                auto bad=parameters;bad["sessionId"]="forged";rejects([&] { engine.dispatch("language.completionPrepare",bad); },"Forged session accepted");
                bad=fresh();bad["item"]=999;rejects([&] { engine.dispatch("language.completionPrepare",bad); },"Invalid item index accepted");
                { Engine other(root);rejects([&] { other.dispatch("language.completionPrepare",bad); },"Session accepted by a different Engine instance"); }
                auto old=fresh();for (int i=0;i<9;++i) fresh();rejects([&] { engine.dispatch("language.completionPrepare",old); },"Evicted session accepted");
                bad=fresh();write(root/"mutate-completion","");rejects([&] { engine.dispatch("language.completionResolve",bad); },"Changed insertion accepted on resolve");fs::remove(root/"mutate-completion");
                bad=fresh();write(root/path,source+"// outside\n");rejects([&] { engine.dispatch("language.completionPrepare",bad); },"Disk conflict accepted");write(root/path,source);
                bad=fresh();write(root/"delay-resolve","");
                auto delayed=std::async(std::launch::async,[&] { return engine.dispatch("language.completionResolve",bad); });
                const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while (!fs::exists(root/"resolve-received") && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                require(fs::exists(root/"resolve-received"),"No delayed resolve");
                doc=engine.dispatch("documents.replace",{{"path",path},{"expectedRevision",doc.at("revision")},{"text",source+"// live\n"}});write(root/"release-resolve","");
                rejects([&] { delayed.get(); },"Obsolete resolve result accepted");
                fs::remove(root/"delay-resolve");bad=fresh();engine.configureLanguage(pathUtf8(fs::absolute(utf8Path(argv[0]))),true,true);
                rejects([&] { engine.dispatch("language.completionPrepare",bad); },"Session survived a server restart");
            }
            engine.configureLanguage("",false,false);
            if (actual) {
                engine.dispatch("language.configure",{{"language",mode},{"enabled",false}});
                const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while (engine.languageStatus().at("servers").at(mode).at("running")==true && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(20));
                require(engine.languageStatus().at("servers").at(mode).at("running")==false,"Server retained workspace locks after disable");
            }
        }
        std::error_code cleanup;const auto cleanupDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        do { cleanup.clear();fs::remove_all(root,cleanup);if (!cleanup) break;std::this_thread::sleep_for(std::chrono::milliseconds(30)); } while (std::chrono::steady_clock::now()<cleanupDeadline);
        require(!cleanup,"Completion workspace cleanup failed: "+cleanup.message());
        std::cout<<mode<<": resolved completion, import preview, atomic apply/undo passed\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}

#include "cycode/Engine.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
using namespace cycode;
namespace fs=std::filesystem;
static void require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &value) { fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<value; }
void jvmBuildContracts(const std::filesystem::path &);
void shaderBuildContracts(const std::filesystem::path &);
void cargoBuildContracts(const std::filesystem::path &);
static bool wait(const std::function<bool()> &check,int seconds=15) { const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);do { if (check()) return true;std::this_thread::sleep_for(std::chrono::milliseconds(30)); } while (std::chrono::steady_clock::now()<end);return false; }
int main(int argc,char **argv) {
    const std::string kind=argc>1 ? argv[1] : "cargo";
    const auto root=fs::temp_directory_path()/utf8Path("cycode-task été # "+kind+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try {
        jvmBuildContracts(root);
        shaderBuildContracts(root);
        cargoBuildContracts(root);
        if (kind=="cargo") {
            write(root/"Cargo.toml","[package]\nname=\"workflow_test\"\nversion=\"0.1.0\"\nedition=\"2021\"\n");
            write(root/"src/main.rs","#[inline(never)]\nfn compute(value: i32) -> i32 { value + 1 }\nfn main() {\n let value = 41;\n let result = compute(value);\n println!(\"RESULT={} ARG={}\", result, std::env::args().nth(1).unwrap_or_default());\n}\n#[test] fn passes() { assert_eq!(compute(1), 2); }\n");
        } else if (kind=="npm") {
            write(root/"package.json",R"({"name":"workflow-test","version":"1.0.0","private":true,"scripts":{"build":"node build.cjs","test":"node test.cjs","start":"node app.cjs","long":"node long.cjs"}})");
            write(root/"build.cjs","require('node:fs').writeFileSync('build-result','built'); console.log('BUILD_OK');\n");
            write(root/"test.cjs","console.log('TEST_OK');\n");
            write(root/"app.cjs","console.log('RESULT=42 ARG='+process.argv[2]);\n");
            write(root/"long.cjs","require('node:fs').writeFileSync('started','yes');require('node:child_process').spawn(process.execPath,['-e',\"setTimeout(()=>require('node:fs').writeFileSync('survived','bad'),2000)\"],{stdio:'ignore'});setInterval(()=>{},1000);\n");
        } else write(root/"src/demo/Main.java","package demo;\npublic class Main { public static void main(String[] args) { System.out.println(\"RESULT=42 ARG=\" + args[0]); } }\n");
        {
            Engine engine(root);engine.configureLanguage("",false,false);
            Json parameters={{"system",kind},{"offline",true}};if (kind=="java") parameters["sourceDirectory"]="src";
            const auto inspected=engine.dispatch("project.inspect",parameters);
            const auto tools=inspected.at("tools");
            if (tools.at(kind=="cargo" ? "cargo" : kind=="npm" ? "npm" : "javac")=="") { std::cout<<"Required optional tool is unavailable\n";fs::remove_all(root);return 77; }
            require(inspected.at("available").get<bool>(),"Project not detected");require(!fs::exists(root/"build-result"),"Inspection executed a script");
            const auto configured=engine.dispatch("project.configure",parameters);require(configured.at("exitCode")==0,configured.dump());
            const auto taskOutput=engine.dispatch("project.output");
            require(!engine.dispatch("project.status").at("running").get<bool>()&&
                !taskOutput.at("running").get<bool>()&&taskOutput.at("context").at("system")==kind,
                "Completed project task lost its output context or reservation");
            if(kind=="cargo")require(taskOutput.at("byteCount")==0,"Cargo metadata leaked into live output");
            if (kind=="cargo") {
                require(!configured.at("project").at("targets").empty(),"Actual Cargo metadata missing");
                require(configured.at("project").at("packages").size()==1,"Cargo workspace packages missing");
                auto changed=parameters;changed["cargoFeatures"]={"changed"};
                require(!engine.dispatch("project.inspect",changed).at("configured").get<bool>(),"Changed Cargo feature reused cached artifact metadata");
                changed=parameters;changed["cargoToolchain"]="stable";
                const auto selected=engine.dispatch("project.inspect",changed);
                require(!selected.at("configured").get<bool>()&&selected.at("rustAnalysis").at("toolchain")=="stable","Cargo toolchain cache/analysis selection diverged");
            }
            if (kind=="npm") require(inspected.at("scripts").size()==4,"npm scripts missing");
            const auto built=engine.dispatch("project.build",parameters);require(built.at("exitCode")==0,built.dump());
            if (kind!="npm") require(!built.at("project").at("artifacts").empty(),"Compiled executable artifact missing");
            if (kind!="java") { const auto tested=engine.dispatch("project.test",parameters);require(tested.at("exitCode")==0,tested.dump()); }
            auto launch=parameters;launch["args"]=Json::array({"a & b é"});
            const auto started=engine.dispatch("project.launch",launch);require(started.at("exitCode")==0,started.dump());
            const auto id=started.at("terminal").at("id").get<std::string>();
            std::string output;
            require(wait([&] { const auto screen=engine.terminals().snapshot(id,0,100);output.clear();for (const auto &line : screen.lines) output+=Terminals::text(line)+"\n";return output.find("RESULT=42 ARG=a & b é")!=std::string::npos; }),"Run output missing or arguments changed: "+output);
            engine.terminals().close(id);
            if (kind=="cargo" && (!toolPath("codelldb").empty() || !toolPath("lldb-dap").empty())) {
                const auto target=engine.dispatch("project.debug",parameters);require(target.contains("program"),"Cargo debug executable missing");
                engine.dispatch("debug.configure",{{"program",target.at("program")},{"runtime","native"},{"cwd","."},{"terminal","integrated"}});
                engine.dispatch("debug.breakpoints",{{"path","src/main.rs"},{"breakpoints",Json::array({{{"line",5}}})}});
                engine.dispatch("debug.launch");
                Json state;
                require(wait([&] { state=engine.dispatch("debug.status"); if (!state.value("error","").empty()) throw std::runtime_error(state.dump());return state.value("state","")=="stopped" && !state.at("variables").empty(); }),"Cargo source breakpoint did not stop");
                bool found=false;for (const auto &variable : state.at("variables")) if (variable.at("name")=="value" && variable.at("value")=="41") found=true;
                require(found,"Rust local variable missing: "+state.dump());
                engine.dispatch("debug.next");
                require(wait([&] { state=engine.dispatch("debug.status"); for (const auto &variable : state.at("variables")) if (variable.at("name")=="result" && variable.at("value")=="42") return state.value("state","")=="stopped";return false; }),"Rust step did not expose the computed result: "+state.dump());
                engine.dispatch("debug.stop");require(engine.dispatch("debug.status").at("state")=="idle","Cargo debugger retained its process");
                std::cout<<"Cargo: actual source breakpoint, locals, step and stop passed\n";
            }
            if (kind=="npm") {
                auto operation=std::async(std::launch::async,[&] { return engine.dispatch("project.script",{{"system","npm"},{"script","long"},{"operationId","test-long"}}); });
                require(wait([&] { return fs::exists(root/"started"); }),"Long npm task did not start");
                require(engine.dispatch("project.status").at("operationId")=="test-long"&&
                    engine.dispatch("project.output").at("running"),"Live project task was hidden by execution locks");
                require(!engine.dispatch("project.cancel",{{"operationId","wrong"}}).at("cancelled").get<bool>(),"Cancellation stopped a different request");
                require(engine.dispatch("project.cancel",{{"operationId","test-long"}}).at("cancelled").get<bool>(),"Cancellation was not accepted");
                require(operation.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Cancelled project did not stop");
                require(operation.get().at("cancelled").get<bool>(),"Cancelled project result not marked");
                std::this_thread::sleep_for(std::chrono::milliseconds(2300));require(!fs::exists(root/"survived"),"A compiler/script descendant survived cancellation");
            } else {
                const auto source=kind=="cargo" ? "src/main.rs" : "src/demo/Main.java";
                auto document=engine.dispatch("documents.read",{{"path",source}});
                document=engine.dispatch("documents.replace",{{"path",source},{"expectedRevision",document.at("revision")},{"text","invalid source\n"}});
                require(engine.dispatch("project.output").contains("streamId")&&
                    !engine.dispatch("project.status").at("running").get<bool>(),"Dirty documents blocked read-only task controls");
                bool dirty=false;try { engine.dispatch("project.build",parameters); } catch (const std::exception &e) { dirty=std::string(e.what()).starts_with("UnsavedDocuments"); }
                require(dirty,"Build ignored unsaved live buffers");
                engine.dispatch("documents.save",{{"path",source},{"expectedRevision",document.at("revision")}});
                const auto failed=engine.dispatch("project.build",parameters);
                require(failed.at("exitCode")!=0 && !failed.at("diagnostics").empty(),"Compiler errors are not available as diagnostics: "+failed.dump());
                require(failed.at("project").at("artifacts").empty(),"Failed build retained runnable artifacts");
                require(!failed.at("project").at("configured").get<bool>(),"Failed build retained a completed project cache");
            }
            bool escaped=false;try { engine.dispatch("project.inspect",{{"directory",".."},{"system",kind}}); } catch (...) { escaped=true; }
            require(escaped,"Project path escaped workspace");
        }
        fs::remove_all(root);std::cout<<kind<<": actual configure/build/test/run, Unicode args, diagnostics and guards passed\n";return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<"\nArtifacts: "<<root<<'\n';return 1; }
}

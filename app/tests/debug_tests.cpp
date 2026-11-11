#include "cycode/Debugger.h"
#include "cycode/DebugAdapters.h"
#include "cycode/TextEdits.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
using Json = nlohmann::json;
static void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
static std::function<std::string()> diagnostic;
void debugInspectionContracts();
static void adapterContracts(Debugger &debugger,const std::filesystem::path &root,const std::filesystem::path &program) {
    const Json definition={{"id","fixture.debug.adapter"},{"plugin","fixture.debug"},{"title","Fixture DAP"},
        {"type","lldb"},{"arguments",{"uninstalled-adapter","${pluginDir}","${workspace}"}},
        {"requests",{"launch","attach"}},{"pidField","pid"},{"launch",{{"custom",true}}}};
    auto reject=[](const std::function<void()> &operation){bool failed=false;try{operation();}catch(const std::exception &){failed=true;}require(failed,"Invalid adapter contract was accepted");};
    const auto generation = debugger.status().at("sessionGeneration");
    require(generation.is_number_integer(),"Debug session generation was not exposed");
    reject([&]{debugger.execute("debug.hotReload",{{"sessionGeneration",generation}});});
    require(debugger.execute("debug.cancelReload",{{"operationId","unknown-reload"}}).at("sessionGeneration") == generation,
            "Reload cancellation changed the debug session");
    validateDebugAdapter(definition);
    for(auto bad:{Json{{"arguments",Json::array()}},Json{{"requests",{"attach","attach"}}},Json{{"requests",{"exec"}}},
        Json{{"environment",{{"BAD=NAME","value"}}}},Json{{"when",{{"project","unreal"}}}},Json{{"pidField","other"}},
        Json{{"launch",{{"program","forged"}}}}}) {auto candidate=definition;candidate.update(bad);reject([&]{validateDebugAdapter(candidate);});}
    Json deep=Json::object();for(int i=0;i<20;++i)deep={{"nested",deep}};
    reject([&]{validateDebugAdapterOptions(deep);});
    reject([&]{debugger.execute("debug.run",{{"program",pathUtf8(program)},{"unknown",deep}});});
    reject([&]{debugger.execute("debug.run",{{"program",pathUtf8(program)},{"args",Json::array({deep})}});});
    for(const auto *key:{"request","type","cwd","program","args","env","stopOnEntry","pid","processId","noDebug","javaAttach"})reject([&]{validateDebugAdapterOptions({{key,"forged"}});});
    const auto directory=root/"plugins/${workspace}/folder with spaces";
    const auto expanded=expandDebugAdapter(definition,directory,root);
    require(expanded.at("arguments")[1]==pathUtf8(directory)&&expanded.at("arguments")[2]==pathUtf8(root),"Plugin/workspace expansion was recursive or split argv");
    auto unknown=definition;unknown["arguments"][1]="${unknown}";reject([&]{expandDebugAdapter(unknown,directory,root);});
    const Json profile={{"program",pathUtf8(program)},{"args",{"arg with space","🛠"}},{"env",{{"REMOVE",nullptr}}},
        {"stopOnEntry",true},{"terminal","integrated"},{"adapterOptions",{{"custom",false},{"sourceMap",{{"old","new"}}}}}};
    auto launch=debugAdapterRequest(expanded,profile,root,false);
    require(launch.at("program")==pathUtf8(std::filesystem::weakly_canonical(program))&&launch.at("args")==profile.at("args")&&launch.at("env")==profile.at("env"),"DAP payload changed program, argv or environment");
    require(launch.at("request")=="launch"&&!launch.at("custom").get<bool>()&&launch.at("noDebug")==false,"DAP options or host-owned launch fields were lost");
    auto attached=debugAdapterRequest(expanded,profile,root,true,123);
    require(attached.at("pid")==123&&!attached.contains("processId")&&!attached.contains("program"),"Attach PID mapping or launch/attach separation failed");
    reject([&]{debugAdapterRequest(expanded,profile,root,true,0);});
    auto launchOnly=expanded;launchOnly["requests"]={"launch"};reject([&]{debugAdapterRequest(launchOnly,profile,root,true,123);});
    debugger.replaceExtensions(Json::array({expanded}));const auto catalogue=debugger.adapters();
    require(catalogue.at("adapters").size()==3&&!catalogue.at("adapters")[2].contains("arguments")&&!catalogue.at("adapters")[2].contains("environment"),"DAP metadata leaked process configuration");
    debugger.replaceExtensions(Json::array({expanded}));require(debugger.adapters().at("revision")==catalogue.at("revision"),"Identical registry churned the launch epoch");
    reject([&]{debugger.replaceExtensions(Json::array({expanded,expanded}));});
    auto forged=expanded;forged["plugin"]="other";reject([&]{debugger.replaceExtensions(Json::array({forged}));});
    debugger.replaceExtensions(Json::array());require(debugger.adapters().at("adapters").size()==2,"Removed plugin adapter remained selectable");
    require(debugger.adapters().at("adapters")[1].at("id")=="builtin.java" &&
            debugger.adapters().at("adapters")[1].at("requests")==Json::array({"launch","attach"}),"Java adapter request catalogue omitted launch/attach");
    forged=expanded;forged["id"]="builtin.java";forged["plugin"]="builtin";
    reject([&]{debugger.replaceExtensions(Json::array({forged}));});
    bool javaOwner=false;
    try { debugger.execute("debug.attach",{{"adapterId","builtin.java"},{"adapterOptions",Json::object()},
                                          {"javaAttach",{{"hostName","127.0.0.1"},{"port",5005},{"timeout",5000}}}}); }
    catch(const std::exception &e) { javaOwner=std::string(e.what())=="Java language service is unavailable"; }
    require(javaOwner,"Java attach incorrectly required a PID or launch artifact");
}
template<class F> static void waitFor(F test) {
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while (!test()) { if (std::chrono::steady_clock::now() > end) throw std::runtime_error("Debug acceptance timed out: "+(diagnostic ? diagnostic() : "")); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    try {
        debugInspectionContracts();
        if (toolPath("codelldb").empty() && toolPath("lldb-dap").empty()) { std::cout << "SKIP: install a DAP adapter with setup_debugger.py for integration acceptance\n"; return 77; }
        auto root = std::filesystem::canonical(utf8Path(argv[2]));
        auto resolve = [&](const std::string &path) { auto full = std::filesystem::weakly_canonical(root/utf8Path(path)); if (!containsPath(root,full)) throw std::runtime_error("Outside fixture"); return full; };
        Terminals terminals; Debugger debugger(root,resolve,terminals);
        adapterContracts(debugger,root,utf8Path(argv[1]));
        diagnostic = [&] { auto s = debugger.status(); s.erase("capabilities"); s.erase("variables"); return s.dump(2); };
        Json profile = {{"program",argv[1]},{"cwd","."},{"args",Json::array()},{"stopOnEntry",false},{"terminal","integrated"},{"adapter","automatic"},{"adapterId","builtin"},{"adapterOptions",Json::object()}};
        debugger.execute("debug.configure",profile);
        const Json endpoint={{"hostName","192.0.2.24"},{"port",5105},{"timeout",8000}};
        debugger.execute("debug.configure",{{"javaAttach",endpoint}});
        {
            Debugger readback(root,resolve,terminals);
            require(readback.profile().at("javaAttach")==endpoint,"Persisted JVM endpoint was not restored");
        }
        auto restored = Json::parse(std::ifstream(root/".cycode/launch.json")); require(restored.at("profile").at("program") == argv[1],"Debug profile was not persisted");
        int breakpoint = 0; std::ifstream source(root/"debuggee.cpp"); std::string line;
        for (int number = 1; std::getline(source,line); ++number) if (line.find("BREAK_HERE") != std::string::npos) breakpoint = number;
        debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array({{{"line",breakpoint},{"condition","value == 41"},{"hitCondition","1"}},{{"line",breakpoint+1},{"logMessage","CHECK value={value}"}}})}});
        bool rejected = false;
        try { debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array()},{"expectedBreakpoints",Json::array()}}); } catch (...) { rejected = true; }
        require(rejected,"A stale breakpoint edit overwrote current breakpoints");
        debugger.execute("debug.launch",Json::object());
        const auto firstSession = debugger.status().at("sessionGeneration").get<uint64_t>();
        require(firstSession > 0,"Started debug session has no issued generation");
        auto paused = [&] { auto state = debugger.status(); if (!state.value("error","").empty()) throw std::runtime_error(state.dump(2)); return state.value("state","") == "stopped" && !state.at("variables").empty(); };
        waitFor(paused);
        auto state = debugger.status(); require(!state.at("frames").empty(),"No real stack frames");
        bool found = false; for (const auto &v : state.at("variables")) if (v.at("name") == "value" && v.at("value") == "41") found = true;
        require(found,"Local variables missing: "+state.dump(2));
        std::cout << "Breakpoint and locals passed\n" << std::flush;
        auto evaluated = debugger.execute("debug.evaluate",{{"expression","value + 1"}});
        {
            bool stale=false;try{debugger.execute("debug.evaluate",{{"expression","value"},{"stopGeneration",state.at("stopGeneration").get<uint64_t>()+1}});}catch(const std::exception &){stale=true;}
            require(stale,"Queued debug inspection accepted an unrelated stop generation");
            const auto actual=debugger.status();
            if(actual.at("capabilities").value("supportsReadMemoryRequest",false)) {
                const auto pointer=debugger.execute("debug.evaluate",{{"expression","&value"}});
                const auto reference=pointer.value("memoryReference",std::string{});
                if(!reference.empty()) {
                    const auto memory=debugger.execute("debug.memory",{{"memoryReference",reference},{"count",4},{"stopGeneration",actual.at("stopGeneration")}});
                    require(memory.at("body").at("bytes").size()==4,"Actual DAP memory read did not return the selected local's bytes");
                }
            }
            if(actual.at("capabilities").value("supportsDisassembleRequest",false)&&!actual.at("frames").empty()) {
                const auto reference=actual.at("frames")[0].value("instructionPointerReference",std::string{});
                if(!reference.empty())require(debugger.execute("debug.disassemble",{{"memoryReference",reference},{"instructionCount",4}}).at("body").at("instructions").size()==4,"Actual adapter disassembly did not honor instruction count");
            }
        }
        require(evaluated.value("result","").find("42") != std::string::npos,"C++ expression evaluation failed: "+evaluated.dump());
        std::cout << "Expression passed\n" << std::flush;
        auto generation = state.at("generation").get<uint64_t>();
        debugger.execute("debug.stepIn",Json::object());
        waitFor([&] { auto s = debugger.status(); return s.at("generation").get<uint64_t>() > generation && s.at("state") == "stopped" && !s.at("frames").empty() && s.at("frames")[0].at("name").get<std::string>().find("increment") != std::string::npos; });
        std::cout << "Step in passed\n" << std::flush;
        debugger.execute("debug.stepOut",Json::object());
        waitFor([&] { auto s = debugger.status(); return s.at("state") == "stopped" && !s.at("frames").empty() && s.at("frames")[0].at("name").get<std::string>().find("main") != std::string::npos; });
        std::cout << "Step out passed\n" << std::flush;
        debugger.execute("debug.continue",Json::object());
        waitFor([&] { return debugger.status().at("state") == "exited"; });
        std::cout << "Exit passed\n" << std::flush;
        waitFor([&] { return debugger.status().value("output","").find("CHECK value=42") != std::string::npos; });
        waitFor([&] {
            auto terminal = debugger.status().value("terminalId",""); if (terminal.empty()) return false;
            for (const auto &line : terminals.snapshot(terminal,0,2000).lines) if (Terminals::text(line).find("RESULT=42") != std::string::npos) return true;
            return false;
        });
        debugger.execute("debug.stop",Json::object());
        require(debugger.status().at("sessionGeneration") == 0,"Disconnected session still exposes an active generation");
        debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array()}});
        debugger.execute("debug.launch",{{"args",Json::array({"--loop"})}});
        require(debugger.status().at("sessionGeneration").get<uint64_t>() > firstSession,"New debug session reused an old generation");
        debugger.execute("debug.pause",Json::object()); waitFor([&] { return debugger.status().at("state") == "stopped"; });
        debugger.execute("debug.stop",Json::object()); require(debugger.status().at("state") == "idle","Stop left debug session active");
        auto target = terminals.create(root,{argv[1],"--loop"});
        debugger.execute("debug.attach",{{"pid",target.at("pid")}});
        debugger.execute("debug.stop",Json::object());
        require(terminals.snapshot(target.at("id"),0,0).running,"Attached target is no longer reported live after detach: "+terminals.list().dump()+" debug: "+debugger.status().dump());
        terminals.close(target.at("id"));
        const auto adapter=toolPath("codelldb").empty()?toolPath("lldb-dap"):toolPath("codelldb");
        const Json extension={{"id","fixture.debug.adapter"},{"plugin","fixture.debug"},{"title","Contributed LLDB"},
            {"type","lldb"},{"arguments",{adapter}},{"requests",{"launch","attach"}},{"pidField","pid"}};
        debugger.replaceExtensions(Json::array({extension}));
        debugger.execute("debug.configure",{{"adapterId","fixture.debug.adapter"},{"adapterOptions",Json::object()}});
        debugger.execute("debug.launch",{{"args",{"--loop"}}});
        waitFor([&]{return !debugger.status().value("terminalId","").empty();});
        require(debugger.status().at("adapterId")=="fixture.debug.adapter"&&debugger.status().at("runtime")=="adapter","Contributed DAP did not use the common transport/state");
        const auto launchedTerminal=debugger.status().at("terminalId").get<std::string>();
        debugger.replaceExtensions(Json::array());
        waitFor([&]{return debugger.status().at("state")=="failed"&&!debugger.status().at("adapterRevoked").get<bool>();});
        bool alive=false;try{alive=terminals.snapshot(launchedTerminal,0,0).running;}catch(const std::exception &){}
        require(!alive,"Revoking a plugin left its launched debuggee alive");
        bool missing=false;try{debugger.execute("debug.launch",Json::object());}catch(const std::exception &){missing=true;}
        require(missing&&debugger.profile().at("adapterId")=="fixture.debug.adapter","Missing plugin silently fell back to a different adapter");
        debugger.replaceExtensions(Json::array({extension}));
        target=terminals.create(root,{argv[1],"--loop"});
        debugger.execute("debug.attach",{{"pid",target.at("pid")}});
        debugger.replaceExtensions(Json::array());
        waitFor([&]{return debugger.status().at("state")=="failed";});
        require(terminals.snapshot(target.at("id"),0,0).running,"Plugin revocation terminated an attached process");
        terminals.close(target.at("id"));
        debugger.execute("debug.configure",{{"adapterId","builtin"},{"adapterOptions",Json::object()}});
        std::cout<<"Contributed DAP: actual LLDB launch/attach, revocation ownership, explicit selection and no fallback passed\n";
        std::cout << "Real DAP: persistent profile, verified source breakpoint, locals, expression, step in/out, continue/output/exit, pause and stop passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

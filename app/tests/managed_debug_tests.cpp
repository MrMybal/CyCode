#include "cycode/Debugger.h"
#include "cycode/DebugRuntime.h"
#include "cycode/TextEdits.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
namespace fs = std::filesystem;
using Json = nlohmann::json;
static void require(bool condition,const std::string &message) { if (!condition) throw std::runtime_error(message); }
static void write(const fs::path &path,const std::string &text) { std::ofstream out(path,std::ios::binary); out << text; }
static void waitFor(Debugger &debugger,const std::function<bool(const Json &)> &test) {
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(20);
    do {
        auto state = debugger.status();
        if (!state.value("error","").empty()) throw std::runtime_error(state.dump(2));
        if (test(state)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    } while (std::chrono::steady_clock::now()<end);
    throw std::runtime_error("Managed debug acceptance timed out: "+debugger.status().dump(2));
}
static bool output(Terminals &terminals,const std::string &id,const std::string &needle) {
    for (const auto &line : terminals.snapshot(id,0,2000).lines) if (Terminals::text(line).find(needle) != std::string::npos) return true;
    return false;
}
static void terminalWait(Terminals &terminals,const std::string &id,const std::string &needle) {
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while (!output(terminals,id,needle)) { require(std::chrono::steady_clock::now()<end,"Managed terminal output: "+needle); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
}
int main() {
    if (toolPath("netcoredbg").empty() || toolPath("dotnet").empty()) return 77;
    const auto root = installationRoot()/utf8Path("data/validation/debug-managed été"); fs::create_directories(root);
    try {
        const auto source = installationRoot()/"app/tests/managed_debuggee.cs";
        fs::copy_file(source,root/"Program.cs",fs::copy_options::overwrite_existing);
        write(root/"global.json","{\"sdk\":{\"version\":\"9.0.100\",\"rollForward\":\"latestFeature\"}}");
        write(root/"NuGet.Config","<configuration><packageSources><clear /></packageSources></configuration>");
        write(root/"Managed.csproj","<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><DebugType>portable</DebugType><DebugSymbols>true</DebugSymbols><Optimize>false</Optimize></PropertyGroup></Project>");
        const auto build = runProcess({toolPath("dotnet"),"build","Managed.csproj","-c","Debug","-p:Platform=AnyCPU","--configfile","NuGet.Config","--nologo"},root,std::chrono::seconds(45),1024*1024);
        require(build.exitCode == 0,build.output);
        const auto dll = root/"bin/Debug/net8.0/Managed.dll";
        require(isManagedProgram(dll) && isRunnableArtifact(dll),"Managed PE/CLI discovery: "+pathUtf8(dll));
        const auto resolver = [&](const std::string &path) { const auto full = fs::weakly_canonical(root/utf8Path(path)); if (!containsPath(root,full)) throw std::runtime_error("Outside fixture"); return full; };
        Terminals terminals; Debugger debugger(root,resolver,terminals);
        debugger.execute("debug.configure",{{"program",pathUtf8(dll)},{"runtime","automatic"},{"dotnet","automatic"},{"adapter","automatic"},{"cwd","."},{"args",Json::array({"été # \"quoted\" $(safe)"})},{"env",{{"CYCODE_DEBUG_MARKER","accent é #"}}},{"watches",Json::array({"value + 1"})},{"buildBeforeLaunch",false}});
        int breakpoint = 0; std::ifstream text(source); std::string line;
        for (int i=1;std::getline(text,line);++i) if (line.find("BREAK_HERE") != std::string::npos) breakpoint = i;
        debugger.execute("debug.breakpoints",{{"path","Program.cs"},{"breakpoints",Json::array({{{"line",breakpoint},{"condition","value == 41"}}})}});
        debugger.execute("debug.configure",{{"adapterArgs",Json::array()}});
        debugger.execute("debug.launch",Json::object());
        waitFor(debugger,[](const Json &s) { return s.at("state") == "stopped" && !s.at("variables").empty() && !s.value("watches",Json::array()).empty(); });
        auto state = debugger.status();
        require(state.at("runtime") == "dotnet","Automatic backend must select .NET");
        require(!state.at("frames").empty() && state.at("frames")[0].at("line") == breakpoint,"Portable PDB source breakpoint: "+state.dump(2));
        require(std::any_of(state.at("variables").begin(),state.at("variables").end(),[](const Json &v) { return v.at("name") == "value" && v.at("value") == "41"; }),"Real C# locals");
        auto evaluation = debugger.execute("debug.evaluate",{{"expression","value + 1"}}); require(evaluation.at("result") == "42","C# expression evaluation: "+evaluation.dump());
        require(state.at("watches")[0].at("result") == "42","Real watches");
        bool unsupported = false;
        try { debugger.execute("debug.breakpoints",{{"path","Program.cs"},{"breakpoints",Json::array({{{"line",breakpoint},{"hitCondition","3"}}})}}); }
        catch (const std::exception &) { unsupported = true; }
        require(unsupported && debugger.status().at("breakpoints") == state.at("breakpoints"),"Unsupported breakpoint must preserve saved configuration");
        // A rejected user command is visible in status; the next session clears it.
        debugger.execute("debug.configure",{{"watches",Json::array({"value + 1"})}});
        require(std::any_of(state.at("variables").begin(),state.at("variables").end(),[](const Json &v) { return v.at("name") == "model" && v.value("variablesReference",0)>0; }),"Object reference missing");
        require(std::any_of(state.at("variables").begin(),state.at("variables").end(),[](const Json &v) { return v.at("name") == "values" && v.value("variablesReference",0)>0; }),"Array reference missing");
        for (const auto &variable : state.at("variables")) if (variable.at("name") == "model") {
            const auto children = debugger.execute("debug.variables",{{"variablesReference",variable.at("variablesReference")}});
            require(children.dump().find("Value") != std::string::npos && children.dump().find("7") != std::string::npos,"Object field expansion");
        }
        for (const auto &variable : state.at("variables")) if (variable.at("name") == "values") {
            const auto children = debugger.execute("debug.variables",{{"variablesReference",variable.at("variablesReference")},{"start",2},{"count",2}});
            require(children.at("variables").size() == 2,"Array variable paging: "+children.dump());
        }
        debugger.execute("debug.stepIn",Json::object());
        waitFor(debugger,[](const Json &s) { return s.at("state") == "stopped" && !s.at("frames").empty() && s.at("frames")[0].value("name","").find("Increment") != std::string::npos; });
        debugger.execute("debug.stepOut",Json::object());
        waitFor(debugger,[](const Json &s) { return s.at("state") == "stopped" && !s.at("frames").empty() && s.at("frames")[0].value("name","").find("Main") != std::string::npos; });
        debugger.execute("debug.continue",Json::object());
        waitFor(debugger,[](const Json &s) { return s.at("state") == "exited" && s.value("output","").find("RESULT=42") != std::string::npos; });
        const auto result = debugger.status().at("output").get<std::string>();
        require(result.find("été # \"quoted\" $(safe)") != std::string::npos && result.find("ENV=accent é #") != std::string::npos,"Exact Unicode argv/environment: "+result);
        debugger.execute("debug.stop",Json::object());
        debugger.execute("debug.breakpoints",{{"path","Program.cs"},{"breakpoints",Json::array()}});
        debugger.execute("debug.launch",{{"args",Json::array({"--loop"})}});
        waitFor(debugger,[](const Json &s) { return s.value("output","").find("READY") != std::string::npos; });
        debugger.execute("debug.pause",Json::object());
        waitFor(debugger,[](const Json &s) { return s.at("state") == "stopped"; });
        debugger.execute("debug.stop",Json::object());
        auto run = debugger.execute("debug.run",{{"args",Json::array({"--input"})}}); const auto id = run.at("terminalId").get<std::string>();
        terminalWait(terminals,id,"INPUT_READY"); terminals.write(id,"CyCode\r"); terminalWait(terminals,id,"INPUT=CyCode"); terminals.close(id);
        auto command = debugProgram(root,{{"program",pathUtf8(dll)},{"args",Json::array({"--loop"})}});
        auto target = terminals.create(root,command.argv); terminalWait(terminals,target.at("id"),"READY");
        debugger.execute("debug.attach",{{"pid",target.at("pid")},{"runtime","dotnet"}});
        debugger.execute("debug.stop",Json::object());
        require(terminals.snapshot(target.at("id"),0,0).running,"Stopping an attached .NET target must detach"); terminals.close(target.at("id"));
        std::cout << "Managed DAP: real .NET 8 portable PDB, conditional breakpoint, locals/objects/arrays, expressions/watches, step in/out, output/argv/env, pause, terminal run/input, attach/detach passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

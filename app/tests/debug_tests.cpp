#include "cycode/Debugger.h"
#include "cycode/TextEdits.h"
#include "cycode/Tools.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
using Json = nlohmann::json;
static void require(bool condition, const std::string &message) { if (!condition) throw std::runtime_error(message); }
static std::function<std::string()> diagnostic;
template<class F> static void waitFor(F test) {
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while (!test()) { if (std::chrono::steady_clock::now() > end) throw std::runtime_error("Debug acceptance timed out: "+(diagnostic ? diagnostic() : "")); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    try {
        if (toolPath("codelldb").empty() && toolPath("lldb-dap").empty()) { std::cout << "SKIP: install a DAP adapter with setup_debugger.py for integration acceptance\n"; return 77; }
        auto root = std::filesystem::canonical(utf8Path(argv[2]));
        auto resolve = [&](const std::string &path) { auto full = std::filesystem::weakly_canonical(root/utf8Path(path)); if (!containsPath(root,full)) throw std::runtime_error("Outside fixture"); return full; };
        Terminals terminals; Debugger debugger(root,resolve,terminals);
        diagnostic = [&] { auto s = debugger.status(); s.erase("capabilities"); s.erase("variables"); return s.dump(2); };
        Json profile = {{"program",argv[1]},{"cwd","."},{"args",Json::array()},{"stopOnEntry",false},{"terminal","integrated"},{"adapter","automatic"}};
        debugger.execute("debug.configure",profile);
        auto restored = Json::parse(std::ifstream(root/".cycode/launch.json")); require(restored.at("profile").at("program") == argv[1],"Debug profile was not persisted");
        int breakpoint = 0; std::ifstream source(root/"debuggee.cpp"); std::string line;
        for (int number = 1; std::getline(source,line); ++number) if (line.find("BREAK_HERE") != std::string::npos) breakpoint = number;
        debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array({{{"line",breakpoint},{"condition","value == 41"},{"hitCondition","1"}},{{"line",breakpoint+1},{"logMessage","CHECK value={value}"}}})}});
        bool rejected = false;
        try { debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array()},{"expectedBreakpoints",Json::array()}}); } catch (...) { rejected = true; }
        require(rejected,"A stale breakpoint edit overwrote current breakpoints");
        debugger.execute("debug.launch",Json::object());
        auto paused = [&] { auto state = debugger.status(); if (!state.value("error","").empty()) throw std::runtime_error(state.dump(2)); return state.value("state","") == "stopped" && !state.at("variables").empty(); };
        waitFor(paused);
        auto state = debugger.status(); require(!state.at("frames").empty(),"No real stack frames");
        bool found = false; for (const auto &v : state.at("variables")) if (v.at("name") == "value" && v.at("value") == "41") found = true;
        require(found,"Local variables missing: "+state.dump(2));
        std::cout << "Breakpoint and locals passed\n" << std::flush;
        auto evaluated = debugger.execute("debug.evaluate",{{"expression","value + 1"}});
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
        debugger.execute("debug.breakpoints",{{"path","debuggee.cpp"},{"breakpoints",Json::array()}});
        debugger.execute("debug.launch",{{"args",Json::array({"--loop"})}});
        debugger.execute("debug.pause",Json::object()); waitFor([&] { return debugger.status().at("state") == "stopped"; });
        debugger.execute("debug.stop",Json::object()); require(debugger.status().at("state") == "idle","Stop left debug session active");
        auto target = terminals.create(root,{argv[1],"--loop"});
        debugger.execute("debug.attach",{{"pid",target.at("pid")}});
        debugger.execute("debug.stop",Json::object());
        require(terminals.snapshot(target.at("id"),0,0).running,"Stopping an attached session killed its target instead of detaching");
        terminals.close(target.at("id"));
        std::cout << "Real DAP: persistent profile, verified source breakpoint, locals, expression, step in/out, continue/output/exit, pause and stop passed\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

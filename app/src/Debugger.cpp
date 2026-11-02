#include "cycode/Debugger.h"
#include "cycode/DebugRuntime.h"
#include "cycode/LanguageClient.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
using Json = nlohmann::json;
static void validateProfile(const Json &profile) {
    for (auto field : {"name","program","cwd","adapter","runtime","dotnet"}) if (!profile.at(field).is_string() || profile.at(field).get_ref<const std::string &>().size() > 4096 || profile.at(field).get_ref<const std::string &>().find('\0') != std::string::npos) throw std::runtime_error("Invalid debug profile field: "+std::string(field));
    (void)debugRuntime({},profile.at("runtime"));
    if (!profile.at("stopOnEntry").is_boolean() || !profile.at("buildBeforeLaunch").is_boolean()) throw std::runtime_error("Invalid debug profile flags");
    if (!profile.at("args").is_array() || profile.at("args").size() > 128 || !profile.at("env").is_object() || !profile.at("watches").is_array() || profile.at("watches").size() > 32 || profile.dump().size() > 65536) throw std::runtime_error("Invalid debug profile");
    for (auto field : {"args","watches","adapterArgs"}) {
        if (!profile.at(field).is_array() || profile.at(field).size()>128) throw std::runtime_error("Invalid debug arguments");
        for (const auto &text : profile.at(field)) if (!text.is_string() || text.get_ref<const std::string &>().size() > 8192 || text.get_ref<const std::string &>().find('\0') != std::string::npos) throw std::runtime_error("Invalid debug argument or watch");
    }
    for (auto i = profile.at("env").begin(); i != profile.at("env").end(); ++i) if (i.key().empty() || i.key().find_first_of("=\0",0,2) != std::string::npos || (!i->is_null() && (!i->is_string() || i->get_ref<const std::string &>().find('\0') != std::string::npos))) throw std::runtime_error("Invalid debug environment variable");
    const auto terminal = profile.value("terminal",""); if (terminal != "integrated" && terminal != "console" && terminal != "external") throw std::runtime_error("Invalid debug terminal mode");
}
static void validateBreakpoints(const Json &points) {
    if (!points.is_array() || points.size() > 1000) throw std::runtime_error("Invalid breakpoint list");
    for (const auto &point : points) {
        if (!point.is_object() || !point.contains("line") || !point.at("line").is_number_integer() || point.at("line").get<int64_t>() < 1 || point.at("line").get<int64_t>() > 10000000) throw std::runtime_error("Breakpoint lines must be one-based");
        if (point.contains("enabled") && !point.at("enabled").is_boolean()) throw std::runtime_error("Invalid breakpoint enabled flag");
        for (auto field : {"condition","hitCondition","logMessage"}) if (point.contains(field) && (!point.at(field).is_string() || point.at(field).get_ref<const std::string &>().size() > 4096)) throw std::runtime_error("Invalid breakpoint condition");
    }
}
struct Debugger::Impl {
    std::filesystem::path root;
    Resolver resolve;
    Terminals &terminals;
    LanguageClient channel;
    mutable std::mutex mutex, jobsMutex;
    std::condition_variable wake;
    std::atomic<bool> stopping{false};
    struct Job { std::string method; Json params; std::promise<Json> promise; };
    std::deque<std::shared_ptr<Job>> jobs;
    std::thread worker;
    Json configuration = {{"name","Default"},{"program",""},{"cwd","."},{"args",Json::array()}, {"env",Json::object()},
                          {"adapter","automatic"},{"adapterArgs",Json::array()},{"runtime","automatic"},{"dotnet","automatic"},{"stopOnEntry",false},{"terminal","integrated"},{"watches",Json::array()},{"buildBeforeLaunch",true}};
    Json breakpoints = Json::object(), verified = Json::object(), capabilities = Json::object();
    Json state = {{"state","idle"},{"reason",""},{"error",""},{"threads",Json::array()},{"frames",Json::array()},
                  {"scopes",Json::array()},{"variables",Json::array()},{"output",""},{"generation",0}, {"threadId",0},{"frameId",0}, {"terminalId",""}};
    std::map<int,Json> replies;
    int sequence = 1;
    bool initialized = false;
    bool attached = false;
    uint64_t stopGeneration = 0, inspectedGeneration = 0;
    std::vector<std::string> ownedTerminals;
    Impl(std::filesystem::path r, Resolver resolver, Terminals &t) : root(std::move(r)), resolve(std::move(resolver)), terminals(t) {
        try {
            auto path = resolve(".cycode/launch.json");
            if (std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) < 1024*1024) {
                std::ifstream input(path); auto saved = Json::parse(input);
                if (saved.contains("profile")) { auto candidate = configuration; for (auto i = candidate.begin(); i != candidate.end(); ++i) if (saved.at("profile").contains(i.key())) i.value() = saved.at("profile").at(i.key()); validateProfile(candidate); configuration = std::move(candidate); }
                if (saved.contains("breakpoints")) {
                    const auto &points = saved.at("breakpoints"); if (!points.is_object() || points.size() > 1000) throw std::runtime_error("Invalid saved breakpoints");
                    for (auto i = points.begin(); i != points.end(); ++i) { (void)resolve(i.key()); validateBreakpoints(i.value()); }
                    breakpoints = points;
                }
            }
        } catch (const std::exception &e) { state["error"] = std::string("Cannot load debug profile: ")+e.what(); }
        worker = std::thread([this] { loop(); });
    }
    ~Impl() { stopping = true; wake.notify_all(); worker.join(); }
    void save() {
        auto path = resolve(".cycode/launch.json"); std::filesystem::create_directories(path.parent_path());
        auto temporary = resolve(".cycode/.launch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        Json data; { std::lock_guard lock(mutex); data = {{"profile",configuration},{"breakpoints",breakpoints}}; }
        { std::ofstream out(temporary, std::ios::binary | std::ios::trunc); out << data.dump(2) << '\n'; if (!out) throw std::runtime_error("Cannot save debug profile"); }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { std::filesystem::remove(temporary); throw std::runtime_error("Cannot replace debug profile"); }
#else
        std::filesystem::rename(temporary,path);
#endif
    }
    void update(const Json &fields) { std::lock_guard lock(mutex); state.update(fields); state["generation"] = state.at("generation").get<uint64_t>()+1; }
    int send(const std::string &command, Json arguments = Json::object()) {
        const int id = sequence++;
        if (!channel.sendRaw({{"seq",id},{"type","request"},{"command",command},{"arguments",arguments}})) throw std::runtime_error("Debug adapter is unavailable or its queue is full");
        return id;
    }
    void pump() {
        for (const auto &message : channel.receiveRaw()) {
            if (!message.is_object()) continue;
            const auto type = message.value("type","");
            if (type == "response") { if (replies.size() < 128) replies[message.value("request_seq",0)] = message; }
            else if (type == "request") {
                Json response = {{"seq",sequence++},{"type","response"},{"request_seq",message.at("seq")},{"command",message.at("command")},{"success",false}};
                try {
                    if (message.at("command") != "runInTerminal") throw std::runtime_error("Unsupported reverse request");
                    const auto &params = message.at("arguments");
                    auto directory = params.value("cwd","");
                    auto cwd = directory.empty() ? root : utf8Path(directory);
                    auto created = terminals.create(cwd, params.at("args").get<std::vector<std::string>>(),100,24,params.value("env",Json::object()));
                    ownedTerminals.push_back(created.at("id"));
                    update({{"terminalId",created.at("id")}});
                    response["success"] = true; response["body"] = {{"processId",created.at("pid")},{"shellProcessId",created.at("pid")}};
                } catch (const std::exception &e) { response["message"] = e.what(); update({{"error",std::string("Program terminal: ")+e.what()}}); }
                channel.sendRaw(response);
            } else if (type == "event") {
                const auto event = message.value("event",""); const auto body = message.value("body",Json::object());
                if (event == "initialized") initialized = true;
                else if (event == "stopped") { ++stopGeneration; clearInspection("stopped",{{"stopGeneration",stopGeneration},{"reason",body.value("reason","")},{"threadId",body.value("threadId",0)}}); }
                else if (event == "continued") clearInspection("running");
                else if (event == "terminated") clearInspection("exited");
                else if (event == "exited") update({{"state","exited"},{"exitCode",body.value("exitCode",0)}});
                else if (event == "output") {
                    std::lock_guard lock(mutex); auto text = state.at("output").get<std::string>() + body.value("output","");
                    if (text.size() > 262144) { const auto boundary = text.find('\n',text.size()-262144); text.erase(0,boundary == std::string::npos ? text.size()-262144 : boundary+1); }
                    state["output"] = std::move(text);
                } else if (event == "breakpoint" && body.contains("breakpoint")) {
                    std::lock_guard lock(mutex); const auto &point = body.at("breakpoint"); state["lastBreakpoint"] = point;
                    if (point.contains("id")) for (auto &file : verified) for (auto &saved : file)
                        if (saved.contains("id") && saved.at("id") == point.at("id")) saved.update(point);
                } else if (event == "capabilities") { std::lock_guard lock(mutex); capabilities.update(body.value("capabilities",Json::object())); }
            }
        }
    }
    Json wait(int id, int seconds = 15, bool teardown = false) {
        auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        while (!replies.contains(id)) {
            pump();
            if (replies.contains(id)) break;
            if ((!teardown && stopping) || !channel.running()) throw std::runtime_error("Debug adapter stopped: "+channel.error());
            if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Debug adapter request timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        auto reply = std::move(replies.at(id)); replies.erase(id);
        if (!reply.value("success",false)) throw std::runtime_error(reply.value("message","Debug adapter rejected the request"));
        return reply.value("body",Json::object());
    }
    Json ask(const std::string &command, const Json &params = Json::object()) { return wait(send(command,params)); }
    Json current() const { std::lock_guard lock(mutex); auto result = state; result["breakpoints"] = breakpoints; result["verifiedBreakpoints"] = verified; result["capabilities"] = capabilities; return result; }
    std::string phase() const { std::lock_guard lock(mutex); return state.value("state","idle"); }
    void clearInspection(const std::string &phase, const Json &extra = Json::object()) {
        Json fields = {{"state",phase},{"threadId",0},{"frameId",0},{"threads",Json::array()},
                       {"frames",Json::array()},{"scopes",Json::array()},{"variables",Json::array()},{"watches",Json::array()}};
        fields.update(extra); update(fields);
    }
    void clearProcess() {
        if (channel.running()) { try { wait(send("disconnect",{{"terminateDebuggee",!attached},{"suspendDebuggee",false}}),3,true); } catch (...) { } }
        channel.stop(); replies.clear(); initialized = false;
        for (const auto &id : ownedTerminals) terminals.close(id);
        ownedTerminals.clear();
    }
    void validateBreakpointCapabilities(const Json &points) const {
        for (const auto &point : points) if (point.value("enabled",true))
            for (const auto &[field,capability] : std::vector<std::pair<std::string,std::string>>{{"condition","supportsConditionalBreakpoints"},{"hitCondition","supportsHitConditionalBreakpoints"},{"logMessage","supportsLogPoints"}})
                if (point.contains(field) && !point.at(field).get<std::string>().empty() && !capabilities.value(capability,false))
                    throw std::runtime_error("Selected debug engine does not support breakpoint option: "+field);
    }
    Json setBreakpoints(const std::string &path, const Json &points) {
        validateBreakpointCapabilities(points);
        auto full = resolve(path); Json request = Json::array();
        for (const auto &point : points) if (point.value("enabled",true)) {
            const auto line = point.at("line").get<int>(); if (line < 1) throw std::runtime_error("Breakpoint lines are one-based");
            Json value = {{"line",line}};
            for (const auto &[field,capability] : std::vector<std::pair<std::string,std::string>>{{"condition","supportsConditionalBreakpoints"},{"hitCondition","supportsHitConditionalBreakpoints"},{"logMessage","supportsLogPoints"}}) if (point.contains(field) && !point.at(field).get<std::string>().empty()) {
                if (!capabilities.value(capability,false)) throw std::runtime_error("Selected debug engine does not support breakpoint option: "+field);
                value[field] = point.at(field);
            }
            request.push_back(value);
        }
        auto sourcePath = pathUtf8(full);
#ifdef _WIN32
        // NetCoreDbg compares Windows PDB document names using native separators.
        if (current().value("runtime","") == "dotnet") std::replace(sourcePath.begin(),sourcePath.end(),'/','\\');
#endif
        auto result = ask("setBreakpoints",{{"source",{{"path",sourcePath}}},{"breakpoints",request},{"sourceModified",false}});
        { std::lock_guard lock(mutex); verified[path] = result.value("breakpoints",Json::array()); }
        return result;
    }
    void scopes(int frame) {
        auto response = ask("scopes",{{"frameId",frame}}); auto values = Json::array();
        for (const auto &scope : response.value("scopes",Json::array())) {
            if (scope.value("expensive",false) || scope.value("variablesReference",0) <= 0) continue;
            auto variables = ask("variables",{{"variablesReference",scope.at("variablesReference")},{"start",0},{"count",200}});
            for (auto variable : variables.value("variables",Json::array())) { variable["scope"] = scope.value("name",""); values.push_back(variable); if (values.size() >= 400) break; }
            if (values.size() >= 400) break;
        }
        update({{"frameId",frame},{"scopes",response.value("scopes",Json::array())},{"variables",values}});
        Json expressions; { std::lock_guard lock(mutex); expressions = configuration.value("watches",Json::array()); }
        Json watches = Json::array();
        for (const auto &expression : expressions) {
            Json value = {{"expression",expression}};
            try { value.update(ask("evaluate",{{"expression",expression},{"frameId",frame},{"context","watch"}})); }
            catch (const std::exception &e) { value["error"] = e.what(); }
            watches.push_back(std::move(value));
        }
        update({{"watches",watches}});
    }
    void inspect() {
        const auto generation = stopGeneration;
        auto threads = ask("threads").value("threads",Json::array()); auto state = current();
        int thread = state.value("threadId",0); if (!thread && !threads.empty()) thread = threads[0].value("id",0);
        auto frames = ask("stackTrace",{{"threadId",thread},{"startFrame",0},{"levels",100}}).value("stackFrames",Json::array());
        if (generation != stopGeneration || current().value("state","") != "stopped") return;
        update({{"threads",threads},{"threadId",thread},{"frames",frames}});
        if (!frames.empty()) scopes(frames[0].at("id"));
        inspectedGeneration = generation;
    }
    Json operation(const std::string &method, const Json &params) {
        if (method == "debug.run") {
            Json profile; { std::lock_guard lock(mutex); profile = configuration; } profile.update(params);
            validateProfile(profile); const auto program = debugProgram(root,profile);
            auto created = terminals.create(resolve(profile.value("cwd",".")),program.argv,100,24,profile.value("env",Json::object()));
            return {{"terminalId",created.at("id")},{"pid",created.at("pid")}};
        }
        if (method == "debug.configure") {
            Json profile; { std::lock_guard lock(mutex); profile = configuration; }
            for (auto field : {"name","program","cwd","args","env","adapter","adapterArgs","runtime","dotnet","stopOnEntry","terminal","watches","buildBeforeLaunch"}) if (params.contains(field)) profile[field] = params.at(field);
            validateProfile(profile);
            (void)resolve(profile.value("cwd","."));
            Json previous; { std::lock_guard lock(mutex); previous = configuration; configuration = profile; }
            try { save(); } catch (...) { std::lock_guard lock(mutex); configuration = previous; throw; }
            if (current().value("state","") == "stopped") scopes(current().value("frameId",0)); return profile;
        }
        if (method == "debug.breakpoints") {
            const auto path = pathUtf8(resolve(params.at("path")).lexically_relative(root));
            auto points = params.at("breakpoints"); validateBreakpoints(points);
            if (channel.running() && initialized) validateBreakpointCapabilities(points);
            Json previous; bool existed;
            { std::lock_guard lock(mutex); existed = breakpoints.contains(path); previous = breakpoints.value(path,Json::array());
              if (params.contains("expectedBreakpoints") && params.at("expectedBreakpoints") != previous) throw std::runtime_error("BreakpointConflict: reload breakpoints before applying changes");
              breakpoints[path] = points; verified.erase(path); }
            try { save(); } catch (...) { std::lock_guard lock(mutex); if (existed) breakpoints[path] = previous; else breakpoints.erase(path); throw; }
            if (channel.running() && initialized) setBreakpoints(path,points); return current();
        }
        if (method == "debug.launch" || method == "debug.attach") {
            if (channel.running() && current().value("state","") != "exited") throw std::runtime_error("Stop the current debug session first");
            clearProcess();
            attached = method == "debug.attach";
            Json profile; { std::lock_guard lock(mutex); profile = configuration; }
            profile.update(params);
            validateProfile(profile);
            auto program = utf8Path(profile.value("program","")); if (!program.is_absolute()) program = root/program;
            const auto runtime = debugRuntime(program,profile.value("runtime","automatic"));
            DebugProgram command;
            if (!attached) command = debugProgram(root,profile);
            auto adapter = profile.value("adapter","automatic");
            if (adapter == "automatic") { adapter = toolPath(runtime == "dotnet" ? "netcoredbg" : "codelldb"); if (adapter.empty() && runtime == "native") adapter = toolPath("lldb-dap"); }
            if (adapter.empty() || !std::filesystem::is_regular_file(utf8Path(adapter))) throw std::runtime_error("No debug adapter installed. Run setup_debugger.py or select an adapter executable.");
            { std::lock_guard lock(mutex); verified = Json::object(); capabilities = Json::object(); state.erase("exitCode"); state.erase("lastBreakpoint"); }
            clearInspection("starting",{{"attached",attached},{"runtime",runtime},{"reason",""},{"error",""},{"output",""},{"terminalId",""}});
            std::vector<std::string> adapterArgs{adapter}; if (runtime == "dotnet") adapterArgs.push_back("--interpreter=vscode");
            for (const auto &arg : profile.at("adapterArgs")) adapterArgs.push_back(arg.get<std::string>());
            if (!channel.startRaw(root,adapterArgs,runtime == "dotnet" ? profile.at("env") : Json::object())) throw std::runtime_error(channel.error());
            auto supported = ask("initialize",{{"adapterID","cycode"},{"clientID","cycode"},{"clientName","CyCode"},{"pathFormat","path"},{"linesStartAt1",true},{"columnsStartAt1",true},{"supportsVariableType",true},{"supportsVariablePaging",true},{"supportsRunInTerminalRequest",true}});
            { std::lock_guard lock(mutex); capabilities = supported; }
            if (!attached) {
                profile["program"] = command.argv.front();
                profile["args"] = std::vector<std::string>(command.argv.begin()+1,command.argv.end());
            }
            profile["cwd"] = pathUtf8(resolve(profile.value("cwd",".")));
            profile["request"] = method == "debug.attach" ? "attach" : "launch";
            profile["type"] = runtime == "dotnet" ? "coreclr" : "lldb"; profile["externalConsole"] = false;
            if (runtime == "dotnet") {
                profile["stopAtEntry"] = profile.at("stopOnEntry");
                profile["justMyCode"] = true; profile["console"] = "internalConsole";
                // NetCoreDbg uses processId; LLDB uses pid.
                if (attached) profile["processId"] = params.at("pid");
                // Apply overrides/removals to the adapter environment only. Passing
                // UTF-8 strings again through NetCoreDbg's Windows ANSI env block
                // corrupts accented values. The debuggee inherits the native block.
                profile["env"] = Json::object();
#ifdef _WIN32
                // NetCoreDbg wraps each string itself instead of applying CRT argv
                // quoting. Escape embedded quotes/backslashes before that wrapper.
                for (auto &argument : profile["args"]) {
                    const auto text = argument.get<std::string>();
                    if (text.empty()) throw std::runtime_error("NetCoreDbg cannot preserve empty launch arguments; use Run without debugging for this argument list");
                    size_t trailing = 0; while (trailing < text.size() && text[text.size()-trailing-1] == '\\') ++trailing;
                    if (trailing && text.find(' ') == std::string::npos) {
                        if (text.find_first_of("\"\t") != std::string::npos) throw std::runtime_error("NetCoreDbg cannot preserve this trailing-backslash argument");
                        continue;
                    }
                    std::string escaped; size_t slashes = 0;
                    for (size_t i=0;i<text.size()-trailing;++i) {
                        const auto c = text[i]; if (c == '\\') { ++slashes; continue; }
                        escaped.append(c == '"' ? slashes*2+1 : slashes,'\\'); slashes = 0; escaped += c;
                    }
                    // For a spaced trailing backslash, NetCoreDbg adds one itself.
                    if (trailing) escaped.append(trailing*2-1,'\\');
                    argument = escaped;
                }
#endif
            }
            auto launch = send(method == "debug.attach" ? "attach" : "launch",profile);
            auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (!initialized) {
                pump();
                if (replies.contains(launch) && !replies.at(launch).value("success",false)) (void)wait(launch);
                if (!channel.running() || stopping || std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Debug adapter initialization failed: "+channel.error());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            Json points; { std::lock_guard lock(mutex); points = breakpoints; }
            for (auto i = points.begin(); i != points.end(); ++i) setBreakpoints(i.key(),i.value());
            if (capabilities.value("supportsConfigurationDoneRequest",false)) ask("configurationDone");
            wait(launch); pump();
            if (current().value("state","") == "starting") update({{"state","running"}});
            return current();
        }
        if (method == "debug.stop") { clearProcess(); clearInspection("idle"); return current(); }
        const auto state = current(); const auto stopped = state.value("state","") == "stopped";
        if (!channel.running()) throw std::runtime_error("No active debug adapter");
        if (method == "debug.pause") {
            if (stopped) return state;
            const auto threads = ask("threads").value("threads",Json::array());
            if (threads.empty()) throw std::runtime_error("No live thread is available to pause");
            const int thread = threads[0].value("id",0);
            ask("pause",{{"threadId",thread}});
        }
        else {
            if (!stopped) throw std::runtime_error("Pause the program before inspecting or stepping");
            if (method == "debug.continue" || method == "debug.next" || method == "debug.stepIn" || method == "debug.stepOut") {
                ask(method.substr(6),{{"threadId",state.at("threadId")},{"granularity","statement"}});
                if (current().value("state","") == "stopped" && stopGeneration == inspectedGeneration) clearInspection("running");
            } else if (method == "debug.evaluate") return ask("evaluate",{{"expression",params.at("expression")},{"frameId",state.at("frameId")},{"context",params.value("context","watch")}});
            else if (method == "debug.variables") return ask("variables",{{"variablesReference",params.at("variablesReference")},{"start",params.value("start",0)},{"count",std::clamp(params.value("count",100),1,200)}});
            else if (method == "debug.source") return ask("source",{{"sourceReference",params.at("sourceReference")}});
            else if (method == "debug.frame") { scopes(params.at("id")); }
            else if (method == "debug.thread") { update({{"threadId",params.at("id")}}); inspect(); }
            else throw std::runtime_error("Unknown debug operation");
        }
        return current();
    }
    void loop() {
        while (!stopping) {
            std::shared_ptr<Job> job;
            bool operationComplete = false;
            { std::lock_guard lock(jobsMutex); if (!jobs.empty()) { job = jobs.front(); jobs.pop_front(); } }
            try {
                pump();
                if (job) { update({{"error",""}}); job->promise.set_value(operation(job->method,job->params)); operationComplete = true; }
                if (channel.running() && phase() == "stopped" && inspectedGeneration != stopGeneration) inspect();
            } catch (const std::exception &e) {
                update({{"error",e.what()}});
                if (job && !operationComplete) { try { job->promise.set_exception(std::current_exception()); } catch (...) {} }
                if (job && !operationComplete && (job->method == "debug.launch" || job->method == "debug.attach")) { clearProcess(); update({{"state","failed"}}); }
                inspectedGeneration = stopGeneration;
            }
            if (!channel.running()) { auto currentState = phase(); if (currentState == "running" || currentState == "stopped" || currentState == "starting") update({{"state","failed"},{"error","Debug adapter exited unexpectedly: "+channel.error()}}); }
            std::unique_lock lock(jobsMutex);
            if (!channel.running()) wake.wait(lock,[&] { return stopping || !jobs.empty(); });
            else wake.wait_for(lock,std::chrono::milliseconds(5),[&] { return stopping || !jobs.empty(); });
        }
        clearProcess();
        std::lock_guard lock(jobsMutex);
        for (auto &job : jobs) job->promise.set_exception(std::make_exception_ptr(std::runtime_error("Debugger stopped")));
    }
};
Debugger::Debugger(std::filesystem::path root, Resolver resolve, Terminals &terminals) : impl_(std::make_unique<Impl>(std::move(root),std::move(resolve),terminals)) {}
Debugger::~Debugger() = default;
Json Debugger::status() const { return impl_->current(); }
Json Debugger::profile() const { std::lock_guard lock(impl_->mutex); return impl_->configuration; }
Json Debugger::execute(const std::string &method, const Json &params) {
    if (method == "debug.status") return status();
    if (method == "debug.profile") return profile();
    auto job = std::make_shared<Impl::Job>(); job->method = method; job->params = params; auto future = job->promise.get_future();
    { std::lock_guard lock(impl_->jobsMutex); if (impl_->jobs.size() >= 32 || impl_->stopping) throw std::runtime_error("Debugger command queue is full or stopped"); impl_->jobs.push_back(job); }
    impl_->wake.notify_one(); return future.get();
}
} // namespace cycode

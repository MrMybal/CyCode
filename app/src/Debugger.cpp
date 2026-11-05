#include "cycode/Debugger.h"
#include "cycode/DebugAdapters.h"
#include "cycode/JavaDebug.h"
#include "cycode/DebugInspection.h"
#include "cycode/DataBreakpoints.h"
#include "cycode/DebugMutations.h"
#include "cycode/DebugExecution.h"
#include "cycode/InstructionBreakpoints.h"
#include "cycode/StepInTargets.h"
#include "cycode/RunToCursor.h"
#include "cycode/TextFile.h"
#include "cycode/NativeFileLease.h"
#include "cycode/DebugRuntime.h"
#include "cycode/UnrealLaunch.h"
#include "cycode/LanguageClient.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
using Json = nlohmann::json;
static void validateProfile(const Json &profile) {
    static const std::set<std::string> fields={"name","program","cwd","args","env","adapter","adapterArgs","adapterId","adapterOptions","runtime","dotnet","stopOnEntry","terminal","watches","buildBeforeLaunch","pid","javaAttach","unrealLaunch"};
    if(!profile.is_object())throw std::runtime_error("Debug profile must be an object");
    for(auto it=profile.begin();it!=profile.end();++it)if(!fields.contains(it.key()))throw std::runtime_error("Unknown debug profile field; use adapterOptions for adapter-specific settings: "+it.key());
    if(profile.contains("pid")&&(!profile.at("pid").is_number_integer()||profile.at("pid")<1||profile.at("pid")>2147483647))throw std::runtime_error("Attach requires a positive process ID");
    if(!profile.at("adapterId").is_string()||profile.at("adapterId").get_ref<const std::string &>().empty()||profile.at("adapterId").get_ref<const std::string &>().size()>160||profile.at("adapterId").get_ref<const std::string &>().find('\0')!=std::string::npos)throw std::runtime_error("Invalid debug adapter ID");
    validateDebugAdapterOptions(profile.at("adapterOptions"));
    if (profile.at("adapterId") == "builtin.java") validateJavaDebugOptions(profile.at("adapterOptions"));
    if (profile.contains("javaAttach")) validateJavaDebugAttach(profile.at("javaAttach"));
    (void)unrealBoundLaunchParameters(profile);
    for (auto field : {"name","program","cwd","adapter","runtime","dotnet"}) if (!profile.at(field).is_string() || profile.at(field).get_ref<const std::string &>().size() > 4096 || profile.at(field).get_ref<const std::string &>().find('\0') != std::string::npos) throw std::runtime_error("Invalid debug profile field: "+std::string(field));
    (void)debugRuntime({},profile.at("runtime"));
    if (!profile.at("stopOnEntry").is_boolean() || !profile.at("buildBeforeLaunch").is_boolean()) throw std::runtime_error("Invalid debug profile flags");
    if (!profile.at("args").is_array() || profile.at("args").size() > 128 || !profile.at("env").is_object() || !profile.at("watches").is_array() || profile.at("watches").size() > 32) throw std::runtime_error("Invalid debug profile");
    for (auto field : {"args","watches","adapterArgs"}) {
        if (!profile.at(field).is_array() || profile.at(field).size()>128) throw std::runtime_error("Invalid debug arguments");
        for (const auto &text : profile.at(field)) if (!text.is_string() || text.get_ref<const std::string &>().size() > 8192 || text.get_ref<const std::string &>().find('\0') != std::string::npos) throw std::runtime_error("Invalid debug argument or watch");
    }
    for (auto i = profile.at("env").begin(); i != profile.at("env").end(); ++i) if (i.key().empty() || i.key().find_first_of("=\0",0,2) != std::string::npos || (!i->is_null() && (!i->is_string() || i->get_ref<const std::string &>().find('\0') != std::string::npos))) throw std::runtime_error("Invalid debug environment variable");
    const auto terminal = profile.value("terminal",""); if (terminal != "integrated" && terminal != "console" && terminal != "external") throw std::runtime_error("Invalid debug terminal mode");
    if(profile.dump().size()>65536)throw std::runtime_error("Debug profile exceeds 64 KiB");
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
    JavaStart javaStart;
    JavaEpoch javaEpoch;
    JavaBuild javaBuild;
    JavaDocuments javaDocuments;
    SourceCheck sourceCheck;
    ProfileCheck profileCheck;
    void checkUnrealProfile(const Json &profile, bool launching = false) {
        if (profile.value("unrealLaunch", Json()).is_null()) return;
        if (!profileCheck) throw std::runtime_error("Unreal launch validation is unavailable in this debugger integration");
        profileCheck(profile, launching);
    }
    RunToCursor runToCursor;
    int runToArmRequest = 0, runToRestoreRequest = 0, runToContinueRequest = 0;
    std::atomic<uint64_t> reloadCancellation{0};
    uint64_t reloadErrors = 0;
    std::string reloadError;
    bool activeJava = false;
    uint64_t activeJavaEpoch = 0;
    mutable std::mutex mutex, jobsMutex;
    std::condition_variable wake;
    std::atomic<bool> stopping{false}, adapterRevoked{false};
    bool tearingDown=false;
    std::map<std::string,Json> adapters;
    uint64_t adapterEpoch=1;
    std::string activeAdapter;
    Json activeDefinition;
    struct Job { std::string method; Json params; std::promise<Json> promise;uint64_t adapterEpoch=0, reloadCancellation=0; std::shared_ptr<std::atomic<bool>> cancelled; };
    std::deque<std::shared_ptr<Job>> jobs;
    std::map<std::string,std::weak_ptr<Job>> reloadJobs;
    std::thread worker;
    Json configuration = {{"unrealLaunch",nullptr},{"name","Default"},{"program",""},{"cwd","."},{"args",Json::array()}, {"env",Json::object()},
                          {"adapter","automatic"},{"adapterArgs",Json::array()},{"adapterId","builtin"},{"adapterOptions",Json::object()},{"runtime","automatic"},{"dotnet","automatic"},{"stopOnEntry",false},{"terminal","integrated"},{"watches",Json::array()},{"buildBeforeLaunch",true}, {"javaAttach",javaDebugAttachDefaults()}};
    Json breakpoints = Json::object(), verified = Json::object(), capabilities = Json::object();
    Json functionBreakpoints = Json::array(), verifiedFunctions = Json::array(), exceptionFilters = nullptr;
    DataBreakpoints dataBreakpoints;
    InstructionBreakpoints instructionBreakpoints;
    StepInTargets stepInTargets;
    DebugTargets gotoTargets{DebugTargets::Kind::goTo};
    std::chrono::steady_clock::time_point gotoStopDeadline{};
    std::optional<std::string> profileBaseline;
    Json state = {{"state","idle"},{"reason",""},{"error",""},{"threads",Json::array()},{"frames",Json::array()},
                  {"scopes",Json::array()},{"variables",Json::array()},{"output",""},{"generation",0}, {"threadId",0},{"frameId",0}, {"terminalId",""},{"sessionGeneration",0},
                  {"dataBreakpoints",Json::array()},{"dataBreakpointRevision",0},{"dataBreakpointsSynchronized",true},
                  {"instructionBreakpoints",Json::array()},{"instructionBreakpointRevision",0},{"instructionBreakpointsSynchronized",true},
                  {"stepInTargetCatalog",""},{"gotoTargetCatalog",""},{"pendingStop",false}};
    std::map<int,Json> replies;
    int sequence = 1;
    bool initialized = false;
    bool attached = false;
    uint64_t stopGeneration = 0, inspectedGeneration = 0, sessionSequence = 0, executionEpoch = 0;
    std::vector<std::string> ownedTerminals;
    Impl(std::filesystem::path r, Resolver resolver, Terminals &t, JavaStart start, JavaEpoch epoch, JavaBuild build, JavaDocuments documents, SourceCheck check, ProfileCheck profileValidator) : root(std::move(r)), resolve(std::move(resolver)), terminals(t), javaStart(std::move(start)), javaEpoch(std::move(epoch)), javaBuild(std::move(build)), javaDocuments(std::move(documents)), sourceCheck(std::move(check)), profileCheck(std::move(profileValidator)) {
        publishRunToCursor();
        try {
            auto path = resolve(".cycode/launch.json");
            if (std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) < 1024*1024) {
                profileBaseline = readTextFileBytes(path); auto saved = Json::parse(*profileBaseline);
                if (saved.contains("profile")) { auto candidate = configuration; for (auto i = candidate.begin(); i != candidate.end(); ++i) if (saved.at("profile").contains(i.key())) i.value() = saved.at("profile").at(i.key()); validateProfile(candidate); configuration = std::move(candidate); }
                if (saved.contains("breakpoints")) {
                    const auto &points = saved.at("breakpoints"); if (!points.is_object() || points.size() > 1000) throw std::runtime_error("Invalid saved breakpoints");
                    for (auto i = points.begin(); i != points.end(); ++i) { (void)resolve(i.key()); validateBreakpoints(i.value()); }
                    breakpoints = points;
                }
                if (saved.contains("functionBreakpoints")) { validateFunctionBreakpoints(saved.at("functionBreakpoints")); functionBreakpoints = saved.at("functionBreakpoints"); }
                if (saved.contains("exceptionFilters")) { validateExceptionFilters(saved.at("exceptionFilters")); exceptionFilters = saved.at("exceptionFilters"); }
            }
        } catch (const std::exception &e) { state["error"] = std::string("Cannot load debug profile: ")+e.what(); }
        worker = std::thread([this] { loop(); });
    }
    ~Impl() { stopping = true; wake.notify_all(); worker.join(); }
    void save() {
        auto path = resolve(".cycode/launch.json"); std::filesystem::create_directories(path.parent_path());
        NativeFileLease lease(resolve(".cycode/launch.json.lock"));
        if(!lease.acquired())throw std::runtime_error("Debug configuration is being saved by another CyCode instance; retry");
        Json data; { std::lock_guard lock(mutex); data = {{"profile",configuration},{"breakpoints",breakpoints},{"functionBreakpoints",functionBreakpoints},{"exceptionFilters",exceptionFilters}}; }
        const auto bytes = data.dump(2) + "\n";
        writeTextFileAtomic(path, bytes, profileBaseline); profileBaseline = bytes;
    }
    void update(const Json &fields) { std::lock_guard lock(mutex); state.update(fields); state["generation"] = state.at("generation").get<uint64_t>()+1; }
    void publishRunToCursor() {
        auto fields = runToCursor.snapshot();
        fields["runToCursor"]["replyPending"] = runToArmRequest != 0 || runToRestoreRequest != 0 || runToContinueRequest != 0;
        update(fields);
    }
    int send(const std::string &command, Json arguments = Json::object()) {
        if (!tearingDown && activeJava && javaEpoch() != activeJavaEpoch)
            throw std::runtime_error("Java language configuration changed; stop and restart debugging");
        if(!tearingDown&&(adapterRevoked||stopping))throw std::runtime_error("Debug adapter was disabled, changed or stopped");
        const int id = sequence++;
        if (!channel.sendRaw({{"seq",id},{"type","request"},{"command",command},{"arguments",arguments}})) throw std::runtime_error("Debug adapter is unavailable or its queue is full");
        return id;
    }
    void pump() {
        if (!tearingDown && activeJava && javaEpoch() != activeJavaEpoch)
            throw std::runtime_error("Java language configuration changed; restart debugging");
        if(!tearingDown&&adapterRevoked)throw std::runtime_error("Debug adapter plugin was disabled or changed");
        for (const auto &message : channel.receiveRaw()) {
            if (!message.is_object()) continue;
            const auto type = message.value("type","");
            if (type == "response") { if (replies.size() < 128) replies[message.value("request_seq",0)] = message; }
            else if (type == "request") {
                Json response = {{"seq",sequence++},{"type","response"},{"request_seq",message.at("seq")},{"command",message.at("command")},{"success",false}};
                try {
                    if(tearingDown||stopping||adapterRevoked)throw std::runtime_error("Debug adapter is stopping");
                    if (message.at("command") != "runInTerminal") throw std::runtime_error("Unsupported reverse request");
                    if (activeJava && attached) throw std::runtime_error("Java attach does not launch a program terminal");
                    const auto &params = message.at("arguments");
                    auto directory = params.value("cwd","");
                    auto cwd = resolve(directory.empty()?".":directory);
                    auto created = terminals.create(cwd, params.at("args").get<std::vector<std::string>>(),100,24,params.value("env",Json::object()));
                    ownedTerminals.push_back(created.at("id"));
                    update({{"terminalId",created.at("id")}});
                    response["success"] = true; response["body"] = {{"processId",created.at("pid")},{"shellProcessId",created.at("pid")}};
                } catch (const std::exception &e) { response["message"] = e.what(); update({{"error",std::string("Program terminal: ")+e.what()}}); }
                channel.sendRaw(response);
            } else if (type == "event") {
                const auto event = message.value("event",""); const auto body = message.value("body",Json::object());
                if (event == "initialized") initialized = true;
                else if (event == "stopped") { runToCursor.stopped(body); publishRunToCursor(); ++executionEpoch; ++stopGeneration; clearInspection("stopped",{{"stopGeneration",stopGeneration},{"reason",body.value("reason","")},{"threadId",body.value("threadId",0)}}); }
                else if (event == "continued") { runToCursor.resumed(); publishRunToCursor(); clearInspection("running"); }
                else if (event == "invalidated") invalidateInspection();
                else if (event == "memory") invalidateInspection();
                else if (event == "hotcodereplace" && activeJava) {
                    const auto value = javaHotReloadEvent(body);
                    update({{"javaReloadEvent", value}});
                    if (value.at("changeType") == "ERROR" || value.at("changeType") == "WARNING") update({{"javaReloadDiagnostic",value}});
                    if (value.at("changeType") == "STARTING" || value.at("changeType") == "END") invalidateInspection();
                    if (value.at("changeType") == "ERROR") {
                        ++reloadErrors; reloadError = value.at("message");
                        update({{"javaReload", {{"status","failed"},{"changedClasses",Json::array()},{"message",reloadError},
                            {"operationId",current().value("javaReload",Json::object()).value("operationId","")}}}});
                    }
                }
                else if (event == "terminated") clearInspection("exited");
                else if (event == "exited") clearInspection("exited",{{"exitCode",body.value("exitCode",0)}});
                else if (event == "output") {
                    std::lock_guard lock(mutex); auto text = state.at("output").get<std::string>() + body.value("output","");
                    if (text.size() > 262144) { const auto boundary = text.find('\n',text.size()-262144); text.erase(0,boundary == std::string::npos ? text.size()-262144 : boundary+1); }
                    state["output"] = std::move(text);
                } else if (event == "breakpoint" && body.contains("breakpoint")) {
                    runToCursor.breakpointEvent(body.at("breakpoint"),body.value("reason","") == "removed"); publishRunToCursor();
                    const bool dataChanged = dataBreakpoints.breakpointEvent(body.at("breakpoint"),body.value("reason","") == "removed");
                    const bool instructionChanged = instructionBreakpoints.breakpointEvent(body.at("breakpoint"),body.value("reason","") == "removed");
                    std::lock_guard lock(mutex); const auto &point = body.at("breakpoint"); state["lastBreakpoint"] = point;
                    if (dataChanged) state.update(dataBreakpoints.snapshot());
                    if (instructionChanged) state.update(instructionBreakpoints.snapshot());
                    if (point.contains("id")) for (auto &file : verified) for (auto &saved : file)
                        if (saved.contains("id") && saved.at("id") == point.at("id")) saved.update(point);
                    if (point.contains("id")) for (auto &saved : verifiedFunctions)
                        if (saved.contains("id") && saved.at("id") == point.at("id")) saved.update(point);
                } else if (event == "capabilities") { std::lock_guard lock(mutex); auto updated=capabilities;updated.update(body.value("capabilities",Json::object()));validateDebugCapabilities(updated);capabilities=std::move(updated);stepInTargets.expire();gotoTargets.expire();state.update(stepInTargets.snapshot());state.update(gotoTargets.snapshot()); }
            }
        }
    }
    Json wait(int id, int seconds = 15, bool teardown = false, bool *settled = nullptr) {
        auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        while (!replies.contains(id)) {
            pump();
            if (replies.contains(id)) break;
            if ((!teardown && stopping) || !channel.running()) throw std::runtime_error("Debug adapter stopped: "+channel.error());
            if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Debug adapter request timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        auto reply = std::move(replies.at(id)); replies.erase(id);
        if (settled) *settled = true;
        if (!reply.value("success",false)) throw std::runtime_error(reply.value("message","Debug adapter rejected the request"));
        return reply.value("body",Json::object());
    }
    Json ask(const std::string &command, const Json &params = Json::object()) { return wait(send(command,params)); }
    Json current() const { std::lock_guard lock(mutex); auto result = state; result["breakpoints"] = breakpoints; result["verifiedBreakpoints"] = verified; result["capabilities"] = capabilities;result["adapterRevoked"]=adapterRevoked.load(); result["functionBreakpoints"] = functionBreakpoints; result["verifiedFunctionBreakpoints"] = verifiedFunctions; result["exceptionFilters"] = exceptionFilters; return result; }
    std::string phase() const { std::lock_guard lock(mutex); return state.value("state","idle"); }
    void expireTargets() {
        stepInTargets.expire(); gotoTargets.expire();
        auto fields = stepInTargets.snapshot(); fields.update(gotoTargets.snapshot()); update(fields);
    }
    void clearInspection(const std::string &phase, const Json &extra = Json::object()) {
        if (phase == "exited") { runToCursor.ended(); publishRunToCursor(); }
        if (phase != this->phase()) ++executionEpoch;
        dataBreakpoints.expire();
        stepInTargets.expire();
        gotoTargets.expire();
        if (phase == "idle" || phase == "starting" || phase == "failed" || phase == "exited") dataBreakpoints.reset();
        if (phase == "idle" || phase == "starting" || phase == "failed" || phase == "exited") instructionBreakpoints.reset();
        Json fields = {{"state",phase},{"threadId",0},{"frameId",0},{"threads",Json::array()},
                       {"frames",Json::array()},{"scopes",Json::array()},{"variables",Json::array()},{"watches",Json::array()},{"pendingStop",false}};
        fields.update(dataBreakpoints.snapshot());
        fields.update(instructionBreakpoints.snapshot());
        fields.update(stepInTargets.snapshot());
        fields.update(gotoTargets.snapshot());
        fields.update(extra); update(fields);
    }
    void invalidateInspection() {
        const auto snapshot = current();
        ++stopGeneration;
        clearInspection(snapshot.at("state"), {{"threadId",snapshot.at("threadId")},{"stopGeneration",stopGeneration},{"pendingStop",snapshot.value("pendingStop",false)}});
    }
    void clearProcess() {
        { std::lock_guard lock(mutex); state["sessionGeneration"] = 0; }
        expireTargets(); gotoStopDeadline = {};
        tearingDown=true;
        if (channel.running()) { try { wait(send("disconnect",{{"terminateDebuggee",!attached},{"suspendDebuggee",false}}),3,true); } catch (...) { } }
        channel.stop(); replies.clear(); initialized = false; activeJava = false;
        runToArmRequest = runToRestoreRequest = runToContinueRequest = 0; runToCursor.reset(); publishRunToCursor();
        dataBreakpoints.reset(); update(dataBreakpoints.snapshot());
        instructionBreakpoints.reset(); update(instructionBreakpoints.snapshot());
        for (const auto &id : ownedTerminals) terminals.close(id);
        ownedTerminals.clear();
        {std::lock_guard lock(mutex);activeAdapter.clear();activeDefinition=nullptr;adapterRevoked=false;verified=Json::object();verifiedFunctions=Json::array();}
        tearingDown=false;
    }
    void validateBreakpointCapabilities(const Json &points) const {
        for (const auto &point : points) if (point.value("enabled",true))
            for (const auto &[field,capability] : std::vector<std::pair<std::string,std::string>>{{"condition","supportsConditionalBreakpoints"},{"hitCondition","supportsHitConditionalBreakpoints"},{"logMessage","supportsLogPoints"}})
                if (point.contains(field) && !point.at(field).get<std::string>().empty() && !capabilities.value(capability,false))
                    throw std::runtime_error("Selected debug engine does not support breakpoint option: "+field);
    }
    Json setBreakpoints(const std::string &path, const Json &points, bool publish = true, int *pending = nullptr) {
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
        const auto id = send("setBreakpoints",{{"source",{{"path",sourcePath}}},{"breakpoints",request},{"sourceModified",false}});
        if (pending) { *pending = id; publishRunToCursor(); }
        bool settled = false; Json result;
        try { result = wait(id,15,false,&settled); if (pending) *pending = 0; }
        catch (...) { if (pending && settled) *pending = 0; throw; }
        if (publish) { std::lock_guard lock(mutex); verified[path] = result.value("breakpoints",Json::array()); }
        return result;
    }
    void settleRunToReplies() {
        bool changed = false;
        for (auto *id : {&runToArmRequest,&runToRestoreRequest,&runToContinueRequest}) if (*id && replies.contains(*id)) {
            try { (void)wait(*id,0); } catch (...) {} *id = 0; changed = true;
        }
        if (changed) publishRunToCursor();
    }
    void restoreRunToCursor(uint64_t revision) {
        if (!runToCursor.stopConfirmed() || phase() != "stopped" || runToArmRequest || runToRestoreRequest || runToContinueRequest)
            throw std::runtime_error("Pause the target and await outstanding replies before restoring run-to-cursor breakpoints");
        const auto path = runToCursor.path();
        runToCursor.restoring(revision); publishRunToCursor();
        try {
            const auto response = setBreakpoints(path,runToCursor.original(),false,&runToRestoreRequest);
            if (runToCursor.active()) {
                runToCursor.restored(response);
                std::lock_guard lock(mutex); verified[path] = response.at("breakpoints");
            }
            inspectedGeneration = 0;
            publishRunToCursor();
        } catch (const std::exception &error) { runToCursor.fail(error.what()); publishRunToCursor(); throw; }
    }
    void scopes(int frame) {
        if (current().value("frameId",0) != frame) expireTargets();
        const auto generation = stopGeneration;
        auto response = ask("scopes",{{"frameId",frame}}); auto values = Json::array();
        if (generation != stopGeneration || phase() != "stopped") return;
        for (const auto &scope : response.value("scopes",Json::array())) {
            if (scope.value("expensive",false) || scope.value("variablesReference",0) <= 0) continue;
            auto variables = ask("variables",{{"variablesReference",scope.at("variablesReference")},{"start",0},{"count",200}});
            if (generation != stopGeneration || phase() != "stopped") return;
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
            if (generation != stopGeneration || phase() != "stopped") return;
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
    Json operation(const std::string &method, const Json &params,uint64_t issuedAdapterEpoch, uint64_t issuedCancellation, const std::shared_ptr<std::atomic<bool>> &cancelled) {
        if (runToCursor.active() && method != "debug.stop" && method != "debug.pause" && method != "debug.restoreRunToCursor")
            throw std::runtime_error("RunToCursorActive: pause or stop before another debugger operation");
        if (current().value("pendingStop",false) && method != "debug.stop")
            throw std::runtime_error("DebugPositionPending: wait for the relocated stopped event or stop the session");
        if (stoppedDebugOperation(method)) {
            const auto snapshot = current();
            if (!params.contains("stopGeneration") || !params.at("stopGeneration").is_number_integer() || params.at("stopGeneration") < 0 ||
                snapshot.value("state", "") != "stopped" || snapshot.value("pendingStop",false) ||
                params.value("stopGeneration", uint64_t(0)) != snapshot.value("stopGeneration", uint64_t(0)))
                throw std::runtime_error("StaleDebugReference: the program resumed or stopped again; refresh inspection");
        }
        if (method == "debug.run") {
            Json profile; { std::lock_guard lock(mutex); profile = configuration; } profile.update(params);
            validateProfile(profile);
            checkUnrealProfile(profile,true);
            if (profile.at("adapterId") == "builtin.java") throw std::runtime_error("Use Project tasks Run for Java without debugging");
            const auto program = debugProgram(root,profile);
            auto created = terminals.create(resolve(profile.value("cwd",".")),program.argv,100,24,profile.value("env",Json::object()));
            return {{"terminalId",created.at("id")},{"pid",created.at("pid")}};
        }
        if (method == "debug.configure") {
            Json profile; { std::lock_guard lock(mutex);
                if (params.contains("expectedProfile")) {
                    validateProfile(params.at("expectedProfile"));
                    if (params.at("expectedProfile") != configuration)
                        throw std::runtime_error("DebugProfileConflict: the saved profile changed; refresh the preview");
                }
                profile = configuration;
            }
            for (auto field : {"name","program","cwd","args","env","adapter","adapterArgs","adapterId","adapterOptions","runtime","dotnet","stopOnEntry","terminal","watches","buildBeforeLaunch","javaAttach","unrealLaunch"}) if (params.contains(field)) profile[field] = params.at(field);
            validateProfile(profile);
            if (params.contains("unrealLaunch") || params.contains("program") || params.contains("cwd") || params.contains("args") || params.contains("runtime") || params.contains("buildBeforeLaunch")) checkUnrealProfile(profile);
            {std::lock_guard lock(mutex);if(profile.at("adapterId")!="builtin"&&profile.at("adapterId")!="builtin.java"&&!adapters.contains(profile.at("adapterId").get<std::string>()))throw std::runtime_error("Selected debug adapter plugin is unavailable");}
            (void)resolve(profile.value("cwd","."));
            Json previous; { std::lock_guard lock(mutex); previous = configuration; configuration = profile; }
            try { save(); } catch (...) { std::lock_guard lock(mutex); configuration = previous; throw; }
            if (current().value("state","") == "stopped" && !current().value("pendingStop",false)) scopes(current().value("frameId",0)); return profile;
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
        if (method == "debug.functionBreakpoints" || method == "debug.exceptionBreakpoints") {
            const bool functions = method == "debug.functionBreakpoints";
            const auto next = params.at(functions ? "breakpoints" : "filters");
            if (functions) validateFunctionBreakpoints(next); else validateExceptionFilters(next);
            Json previous, previousVerified;
            { std::lock_guard lock(mutex);
              previous = functions ? functionBreakpoints : exceptionFilters;
              previousVerified = verifiedFunctions;
              if (params.contains("expected") && params.at("expected") != previous)
                  throw std::runtime_error("BreakpointConflict: reload advanced breakpoints before applying"); }
            const bool live = channel.running() && initialized;
            Json arguments;
            if (live) arguments = functions ? functionBreakpointArguments(next,current().at("capabilities"))
                                            : exceptionBreakpointArguments(next,current().at("capabilities"));
            { std::lock_guard lock(mutex); if (functions) {functionBreakpoints = next;verifiedFunctions=Json::array();} else exceptionFilters = next; }
            try { save(); } catch (...) { std::lock_guard lock(mutex); if (functions) {functionBreakpoints = previous;verifiedFunctions=previousVerified;} else exceptionFilters = previous; throw; }
            if (live) {
                const auto response = ask(functions ? "setFunctionBreakpoints" : "setExceptionBreakpoints",arguments);
                if (functions) {
                    const auto points = response.at("breakpoints");
                    if (!points.is_array() || points.size() != arguments.at("breakpoints").size())
                        throw std::runtime_error("Invalid function breakpoint verification response");
                    std::lock_guard lock(mutex); verifiedFunctions = points;
                }
            }
            return current();
        }
        if (method == "debug.launch" || method == "debug.attach") {
            if (channel.running() && current().value("state","") != "exited") throw std::runtime_error("Stop the current debug session first");
            clearProcess();
            attached = method == "debug.attach";
            Json profile; { std::lock_guard lock(mutex); profile = configuration; }
            profile.update(params);
            validateProfile(profile);
            if (!attached) checkUnrealProfile(profile,true);
            auto program = utf8Path(profile.value("program","")); if (!program.is_absolute()) program = root/program;
            const auto selected=profile.at("adapterId").get<std::string>();
            const bool java = selected == "builtin.java";
            const bool contributed=selected!="builtin" && !java;
            if (java && attached && params.contains("pid")) throw std::runtime_error("Java attach uses a JDWP host and port, not a process ID");
            Json definition;
            if(contributed) {
                std::lock_guard lock(mutex);
                if(issuedAdapterEpoch!=adapterEpoch)throw std::runtime_error("Debug adapter registry changed; retry the launch");
                if(!adapters.contains(selected))throw std::runtime_error("Selected debug adapter plugin is unavailable");
                definition=adapters.at(selected);
            }
            const auto runtime = java ? "java" : contributed?"adapter":debugRuntime(program,profile.value("runtime","automatic"));
            if(attached&&!java&&(!params.contains("pid")||!params.at("pid").is_number_integer()||params.at("pid")<1||params.at("pid")>2147483647))throw std::runtime_error("Attach requires a positive process ID");
            DebugProgram command;
            if (!attached&&!contributed&&!java) command = debugProgram(root,profile);
            auto adapter = profile.value("adapter","automatic");
            std::vector<std::string> adapterArgs;
            Json launchParameters,adapterEnvironment=Json::object();
            if(java) {
                if (!javaStart || !javaEpoch) throw std::runtime_error("Java language service is unavailable");
                launchParameters = attached ? javaDebugAttachRequest(profile, resolve(profile.value("cwd", "."))) : javaDebugRequest(profile, resolve(profile.value("cwd", ".")));
            } else if(contributed) {
                adapterArgs=definition.at("arguments").get<std::vector<std::string>>();
                adapter=toolPath(adapterArgs.front());adapterArgs.front()=adapter;
                adapterEnvironment=definition.value("environment",Json::object());
                auto actualProfile=profile;if(!profile.at("program").get_ref<const std::string &>().empty())actualProfile["program"]=pathUtf8(program);
                launchParameters=debugAdapterRequest(definition,actualProfile,resolve(profile.value("cwd",".")),attached,params.value("pid",Json()));
            }else {
                if (adapter == "automatic") { adapter = toolPath(runtime == "dotnet" ? "netcoredbg" : "codelldb"); if (adapter.empty() && runtime == "native") adapter = toolPath("lldb-dap"); }
                adapterArgs={adapter};if(runtime=="dotnet")adapterArgs.push_back("--interpreter=vscode");
                for(const auto &arg:profile.at("adapterArgs"))adapterArgs.push_back(arg.get<std::string>());
                if(runtime=="dotnet")adapterEnvironment=profile.at("env");
            }
            if (!java && (adapter.empty() || !std::filesystem::is_regular_file(utf8Path(adapter)))) throw std::runtime_error("No debug adapter installed. Run setup_debugger.py or select an adapter executable.");
            { std::lock_guard lock(mutex); verified = Json::object(); verifiedFunctions = Json::array(); capabilities = Json::object(); state.erase("exitCode"); state.erase("lastBreakpoint"); state.erase("lastMutation"); state.erase("lastExecution"); state.erase("javaAttach"); state.erase("javaReload"); state.erase("javaReloadEvent"); state.erase("javaReloadDiagnostic"); }
            clearInspection("starting",{{"sessionGeneration",++sessionSequence},{"attached",attached},{"runtime",runtime},{"adapterId",selected},
                {"adapterPlugin",contributed?definition.value("plugin",std::string{}):""},{"adapterTitle",java?Json("Java / JDT LS"):contributed?definition.at("title"):Json("Built-in C++ / .NET")},
                {"reason",""},{"error",""},{"output",""},{"terminalId",""}});
            if (java && attached) update({{"javaAttach", profile.at("javaAttach")}});
            if (java) {
                activeJavaEpoch = javaEpoch();
                auto session = javaStart();
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                while (session.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
                    if (stopping || javaEpoch() != activeJavaEpoch || std::chrono::steady_clock::now() >= deadline)
                        throw std::runtime_error("Java debug startup cancelled, reconfigured or timed out");
                }
                const auto port = session.get();
                if (!port.is_number_integer() || port < 1 || port > 65535)
                    throw std::runtime_error("JDT LS returned an invalid debug adapter port");
                if (stopping || javaEpoch() != activeJavaEpoch) throw std::runtime_error("Java configuration changed during debug startup");
                if (!channel.connectRaw(port.get<uint16_t>())) throw std::runtime_error(channel.error());
                activeJava = true;
            } else {
                std::lock_guard lock(mutex);
                if(stopping)throw std::runtime_error("Debugger stopped");
                if(contributed) {
                    if(issuedAdapterEpoch!=adapterEpoch||!adapters.contains(selected)||adapters.at(selected)!=definition)throw std::runtime_error("Debug adapter registry changed before launch");
                    activeAdapter=selected;activeDefinition=definition;
                }
                if(!channel.startRaw(root,adapterArgs,adapterEnvironment))throw std::runtime_error(channel.error());
            }
            auto supported = ask("initialize",{{"adapterID",java?Json("java"):contributed?definition.at("type"):Json("cycode")},{"clientID","cycode"},{"clientName","CyCode"},{"pathFormat","path"},{"linesStartAt1",true},{"columnsStartAt1",true},{"supportsVariableType",true},{"supportsVariablePaging",true},{"supportsInvalidatedEvent",true},{"supportsMemoryEvent",true},{"supportsRunInTerminalRequest",true}});
            validateDebugCapabilities(supported);
            { std::lock_guard lock(mutex); capabilities = supported; }
            if (!contributed&&!attached&&!java) {
                profile["program"] = command.argv.front();
                profile["args"] = std::vector<std::string>(command.argv.begin()+1,command.argv.end());
            }
            if(!contributed&&!java) {
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
            profile.erase("adapterId");profile.erase("adapterOptions");profile.erase("javaAttach");launchParameters=profile;
            }
            auto launch = send(method == "debug.attach" ? "attach" : "launch",launchParameters);
            auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(15);
            while (!initialized) {
                pump();
                if (replies.contains(launch) && !replies.at(launch).value("success",false)) (void)wait(launch);
                if (!channel.running() || stopping || std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Debug adapter initialization failed: "+channel.error());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            Json points; { std::lock_guard lock(mutex); points = breakpoints; }
            for (auto i = points.begin(); i != points.end(); ++i) setBreakpoints(i.key(),i.value());
            Json functions,filters; { std::lock_guard lock(mutex); functions=functionBreakpoints; filters=exceptionFilters; }
            const bool hasFunctions=std::any_of(functions.begin(),functions.end(),[](const auto &point){return point.value("enabled",true);});
            if(supported.value("supportsFunctionBreakpoints",false)||hasFunctions) {
                const auto arguments=functionBreakpointArguments(functions,supported);
                const auto response=ask("setFunctionBreakpoints",arguments);
                const auto confirmed=response.at("breakpoints");
                if(!confirmed.is_array()||confirmed.size()!=arguments.at("breakpoints").size())throw std::runtime_error("Invalid function breakpoint verification response");
                {std::lock_guard lock(mutex);verifiedFunctions=confirmed;}
            }
            const auto exceptions=exceptionBreakpointArguments(filters,supported);
            if(!supported.value("exceptionBreakpointFilters",Json::array()).empty()||!supported.value("supportsConfigurationDoneRequest",false)||!exceptions.at("filters").empty())
                ask("setExceptionBreakpoints",exceptions);
            if (capabilities.value("supportsConfigurationDoneRequest",false)) ask("configurationDone");
            wait(launch); pump();
            if (current().value("state","") == "starting") update({{"state","running"}});
            return current();
        }
        if (method == "debug.stop") { clearProcess(); clearInspection("idle"); return current(); }
        const auto state = current(); const auto stopped = state.value("state","") == "stopped";
        if (!channel.running()) throw std::runtime_error("No active debug adapter");
        if (method == "debug.restoreRunToCursor") {
            for (const auto &[key,value] : params.items())
                if (key != "sessionGeneration" && key != "stopGeneration" && key != "expectedRevision") throw std::runtime_error("Unknown restoration field");
            if (!params.at("sessionGeneration").is_number_integer() || params.at("sessionGeneration") < 1 || params.at("sessionGeneration") != state.at("sessionGeneration") ||
                !params.at("expectedRevision").is_number_integer() || params.at("expectedRevision") < 0)
                throw std::runtime_error("StaleDebugReference: refresh the run-to-cursor restoration context");
            restoreRunToCursor(params.at("expectedRevision")); return current();
        }
        if (method == "debug.runToCursor") {
            if (runToArmRequest || runToRestoreRequest || runToContinueRequest)
                throw std::runtime_error("Run-to-cursor requests are still pending; await their replies or stop the session");
            auto input = params;
            const auto full = resolve(input.at("path"));
            input["path"] = pathUtf8(full.lexically_relative(root));
            const auto original = state.at("breakpoints").value(input.at("path").get<std::string>(),Json::array());
            validateBreakpoints(original);
            const auto plan = RunToCursor::plan(input,state,original);
            if (!sourceCheck) throw std::runtime_error("Native source validation service unavailable");
            sourceCheck(input);
            const auto epoch = executionEpoch;
            runToCursor.begin(plan); publishRunToCursor();
            { std::lock_guard lock(mutex); verified.erase(plan.at("path").get<std::string>()); }
            bool continuing = false, accepted = false;
            Json result = Json::object();
            const auto publishExecution = [&](const std::string &status, const std::string &message = "") {
                update({{"lastExecution",{{"method","debug.continue"},{"status",status},{"message",message},
                    {"sessionGeneration",state.at("sessionGeneration")},{"stopGeneration",state.at("stopGeneration")},
                    {"arguments",plan.at("resume")},{"result",result}}}});
            };
            try {
                runToCursor.armed(setBreakpoints(plan.at("path"),plan.at("points"),false,&runToArmRequest)); publishRunToCursor();
                const auto target = runToCursor.snapshot().at("runToCursor").at("verification");
                if (target.contains("source")) {
                    const auto &source = target.at("source");
                    if (source.contains("path")) {
                        const auto actual = resolve(source.at("path")); std::error_code error;
                        if (!std::filesystem::equivalent(full,actual,error) || error)
                            throw std::runtime_error("Destination was relocated into another source; program was not resumed");
                    } else if (source.value("sourceReference",0) > 0)
                        throw std::runtime_error("Destination source reference cannot be matched; program was not resumed");
                }
                (void)RunToCursor::plan(input,current(),original);
                if (executionEpoch != epoch || !runToCursor.stopConfirmed() || runToCursor.cleanupDue())
                    throw std::runtime_error("StaleDebugReference: target stopped or resumed while arming run to cursor");
                sourceCheck(input);
                expireTargets(); runToCursor.continuing(); publishRunToCursor(); publishExecution("applying");
                runToContinueRequest = send("continue",plan.at("resume")); continuing = true; publishRunToCursor();
                bool settled = false; Json body;
                try { body = wait(runToContinueRequest,15,false,&settled); runToContinueRequest = 0; }
                catch (...) { if (settled) runToContinueRequest = 0; throw; }
                result = debugExecutionResult("debug.continue",body); accepted = true; runToCursor.continued(result); publishRunToCursor();
                if (debugExecutionNeedsResume(phase(),epoch,executionEpoch)) clearInspection("running");
                publishExecution("accepted");
                if (runToCursor.cleanupDue() && phase() == "stopped") restoreRunToCursor(runToCursor.snapshot().at("runToCursor").at("revision"));
            } catch (const std::exception &error) {
                runToCursor.fail(error.what()); publishRunToCursor();
                if (continuing) { invalidateInspection(); if (!accepted) publishExecution("unconfirmed",error.what()); }
                else if (!runToArmRequest && runToCursor.stopConfirmed() && phase() == "stopped" && runToCursor.active()) {
                    try { restoreRunToCursor(runToCursor.snapshot().at("runToCursor").at("revision")); } catch (...) {}
                }
                throw;
            }
            return current();
        }
        if (method == "debug.instructionBreakpoints") {
            if (!params.contains("sessionGeneration") || !params.at("sessionGeneration").is_number_integer() || params.at("sessionGeneration") < 1 || params.at("sessionGeneration") != state.at("sessionGeneration"))
                throw std::runtime_error("StaleDebugSession: refresh debug status before editing instruction breakpoints");
            for (const auto &[key,value] : params.items())
                if (key != "sessionGeneration" && key != "stopGeneration" && key != "breakpoints" && key != "expectedRevision")
                    throw std::runtime_error("Unknown instruction breakpoint replacement field");
            if (!params.contains("expectedRevision") || !params.at("expectedRevision").is_number_integer() || params.at("expectedRevision") < 0)
                throw std::runtime_error("Instruction breakpoint replacement requires expectedRevision");
            const auto plan = InstructionBreakpoints::plan(params.at("breakpoints"),state.at("capabilities"));
            const auto epoch = executionEpoch;
            instructionBreakpoints.beginReplace(params.at("expectedRevision").get<uint64_t>());
            update(instructionBreakpoints.snapshot());
            try {
                const auto reply = ask("setInstructionBreakpoints",plan.at("arguments"));
                if (executionEpoch != epoch || phase() != "stopped" || current().at("sessionGeneration") != params.at("sessionGeneration"))
                    throw std::runtime_error("StaleDebugReference: instruction breakpoint execution context changed; effects may already exist");
                instructionBreakpoints.complete(plan,reply);
            } catch (...) {
                instructionBreakpoints.fail(); update(instructionBreakpoints.snapshot()); throw;
            }
            update(instructionBreakpoints.snapshot()); return current();
        }
        if (method == "debug.stepInTargets") {
            const auto arguments = StepInTargets::query(params,state);
            stepInTargets.expire(); update(stepInTargets.snapshot());
            const auto reply = ask("stepInTargets",arguments);
            const auto result = stepInTargets.complete(params,reply,current());
            update(stepInTargets.snapshot());
            return result;
        }
        if (method == "debug.gotoTargets") {
            const auto arguments = DebugTargets::query(params,state,DebugTargets::Kind::goTo);
            gotoTargets.expire(); update(gotoTargets.snapshot());
            const auto reply = ask("gotoTargets",arguments);
            const auto result = gotoTargets.complete(params,reply,current(),arguments);
            update(gotoTargets.snapshot()); return result;
        }
        if (debugExecutionOperation(method)) {
            const bool targeted = method == "debug.stepIntoTarget", relocating = method == "debug.gotoTarget";
            const auto arguments = targeted ? stepInTargets.arguments(params,state) : relocating ? gotoTargets.arguments(params,state) : debugExecutionArguments(method,params,state);
            auto publicArguments = arguments;
            if (targeted || relocating) {
                publicArguments.erase("targetId"); publicArguments["target"] = params.at("target");
                publicArguments["frameId"] = params.at("frameId");
            }
            const auto epoch = executionEpoch;
            Json result = Json::object();
            const auto publish = [&](const std::string &status, const std::string &message = "") {
                update({{"lastExecution",{{"method",method},{"status",status},{"message",message},
                    {"sessionGeneration",state.at("sessionGeneration")},{"stopGeneration",state.at("stopGeneration")},
                    {"arguments",publicArguments},{"result",result}}}});
            };
            expireTargets();
            publish("applying");
            try {
                result = debugExecutionResult(method,ask(targeted ? "stepIn" : relocating ? "goto" : method.substr(6),arguments));
                // Adapters may invalidate caches without resuming, or report a new
                // stop before the reply is consumed. Only the original pause can
                // transition here; a newer stopped event remains authoritative.
                if (debugExecutionNeedsResume(phase(),epoch,executionEpoch)) {
                    if (relocating) {
                        ++stopGeneration;
                        clearInspection("stopped",{{"stopGeneration",stopGeneration},{"pendingStop",true}});
                        inspectedGeneration = stopGeneration;
                        gotoStopDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                    } else clearInspection("running");
                }
                publish("accepted");
            } catch (const std::exception &error) {
                invalidateInspection(); publish("unconfirmed",error.what());
                throw;
            }
            return current();
        }
        if (debugMutationOperation(method)) {
            if (!params.contains("sessionGeneration") || !params.at("sessionGeneration").is_number_integer() || params.at("sessionGeneration") < 1 || params.at("sessionGeneration") != state.at("sessionGeneration"))
                throw std::runtime_error("StaleDebugSession: refresh inspection before changing target values");
            const auto arguments = debugMutationArguments(method,params,state.at("capabilities"),state.at("frameId"));
            if (method == "debug.setExpression" && std::none_of(state.at("frames").begin(),state.at("frames").end(),[&](const auto &frame) { return frame.at("id") == arguments.at("frameId"); }))
                throw std::runtime_error("StaleDebugReference: inspect a selected stack frame before assigning an expression");
            const auto epoch = executionEpoch;
            const auto samePause = [&] {
                if (epoch != executionEpoch || phase() != "stopped" || current().at("sessionGeneration") != params.at("sessionGeneration"))
                    throw std::runtime_error("StaleDebugReference: target resumed or changed while modifying values; effects may already exist");
            };
            bool submitted = false;
            Json result = Json::object();
            const bool memory = method == "debug.writeMemory";
            if (memory) result["requestedBytes"] = params.at("bytes").size();
            const auto publish = [&](const std::string &status, const std::string &message = "") {
                update({{"lastMutation",{{"method",method},{"status",status},{"message",message},{"sessionGeneration",params.at("sessionGeneration")},{"result",result}}}});
            };
            publish(memory ? "checking" : "applying");
            try {
                Json read;
                if (memory) {
                    read = {{"memoryReference",arguments.at("memoryReference")},{"offset",arguments.at("offset")},{"count",params.at("bytes").size()}};
                    const auto before = debugInspectionResult("debug.memory",ask("readMemory",read),read);
                    samePause();
                    if (stopGeneration != params.at("stopGeneration").get<uint64_t>()) throw std::runtime_error("StaleDebugReference: memory inspection changed before the write");
                    if (before.at("unreadableBytes") != 0 || before.at("bytes") != params.at("expectedBytes"))
                        throw std::runtime_error("RevisionConflict: target memory differs from the expected bytes; read it again before writing");
                }
                publish("applying"); submitted = true;
                const auto reply = ask(method.substr(6),arguments);
                samePause();
                result.update(debugMutationResult(method,reply,params));
                std::string status = "applied", message;
                if (memory) {
                    const auto after = debugInspectionResult("debug.memory",ask("readMemory",read),read);
                    samePause();
                    result["readBackVerified"] = after.at("unreadableBytes") == 0 && after.at("bytes") == params.at("bytes");
                    if (result.contains("bytesWritten") && result.at("bytesWritten") < result.at("requestedBytes")) {
                        status = "partial"; message = "The adapter reported a partial memory write; inspect the target before retrying";
                    } else if (!result.at("readBackVerified").get<bool>()) {
                        status = "unconfirmed"; message = "Memory read-back differs from the requested bytes; target effects are uncertain";
                    }
                }
                invalidateInspection(); publish(status,message);
            } catch (const std::exception &error) {
                invalidateInspection();
                const bool partial = memory && result.contains("bytesWritten") && result.at("bytesWritten") < result.at("requestedBytes");
                publish(partial ? "partial" : submitted ? "unconfirmed" : "rejected",error.what());
                throw;
            }
            return current();
        }
        if (method == "debug.dataBreakpointInfo" || method == "debug.dataBreakpoints") {
            if (!params.contains("sessionGeneration") || !params.at("sessionGeneration").is_number_integer() || params.at("sessionGeneration") < 1 ||
                params.at("sessionGeneration") != state.at("sessionGeneration"))
                throw std::runtime_error("StaleDebugSession: refresh debug status before editing data breakpoints");
            const auto stillPaused = [&] {
                const auto snapshot = current();
                if (snapshot.at("sessionGeneration") != params.at("sessionGeneration") || phase() != "stopped" ||
                    stopGeneration != params.at("stopGeneration").get<uint64_t>())
                    throw std::runtime_error("StaleDebugReference: data breakpoint pause expired");
            };
            if (method == "debug.dataBreakpointInfo") {
                for (const auto &[key,value] : params.items())
                    if (key != "name" && key != "variablesReference" && key != "sessionGeneration" && key != "stopGeneration")
                        throw std::runtime_error("Unknown data breakpoint inspection field");
                const auto arguments = DataBreakpoints::arguments(params,state.at("capabilities"),state.at("frameId"));
                const auto reply = ask("dataBreakpointInfo",arguments);
                stillPaused();
                return dataBreakpoints.capture(arguments,reply,state.at("sessionGeneration"),stopGeneration);
            }
            for (const auto &[key,value] : params.items())
                if (key != "breakpoints" && key != "expectedRevision" && key != "sessionGeneration" && key != "stopGeneration")
                    throw std::runtime_error("Unknown data breakpoint replacement field");
            const auto plan = dataBreakpoints.prepare(params.at("breakpoints"),state.at("capabilities"),state.at("sessionGeneration"),stopGeneration);
            if (!params.contains("expectedRevision") || !params.at("expectedRevision").is_number_integer() || params.at("expectedRevision") < 0)
                throw std::runtime_error("Data breakpoint replacement requires expectedRevision from debug status");
            dataBreakpoints.beginReplace(params.at("expectedRevision").get<uint64_t>());
            update(dataBreakpoints.snapshot());
            // A rejected/timed-out/late mutation can have side effects. Keep the
            // last known list marked unsynchronized until a complete replacement.
            const auto reply = ask("setDataBreakpoints",plan.at("arguments"));
            stillPaused();
            dataBreakpoints.complete(plan,reply); update(dataBreakpoints.snapshot());
            return current();
        }
        if (method == "debug.hotReload") {
            if (params.size() != 2 || !params.contains("sessionGeneration") || !params.at("sessionGeneration").is_number_integer() ||
                params.at("sessionGeneration") < 1 || params.at("sessionGeneration") != state.at("sessionGeneration"))
                throw std::runtime_error("StaleDebugSession: refresh debug status before Java reload");
            if (!activeJava || !javaBuild || !javaDocuments || (!stopped && state.at("state") != "running"))
                throw std::runtime_error("Java reload requires an active JDT LS Java debug session");
            struct CancelBuild { std::shared_ptr<std::atomic<bool>> flag; ~CancelBuild() { *flag = true; } } cancelBuild{cancelled};
            const auto check = [&] {
                if (stopping || *cancelled || reloadCancellation != issuedCancellation) throw std::runtime_error("Java reload cancelled before application");
                if (javaEpoch() != activeJavaEpoch || !channel.running() || (phase() != "running" && phase() != "stopped"))
                    throw std::runtime_error("Java debug session changed during reload");
            };
            const auto reload = [&](Json value) { value["operationId"] = params.at("operationId"); value["cancelRequested"] = cancelled->load(); update({{"javaReload",std::move(value)}}); };
            bool applying = false;
            { std::lock_guard lock(mutex); this->state.erase("javaReloadDiagnostic"); this->state.erase("javaReloadEvent"); }
            reload({{"status","building"},{"changedClasses",Json::array()},{"message",""}});
            try {
                check();
                const auto documents = javaDocuments();
                auto build = javaBuild(cancelled);
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
                bool cancellationReported = false;
                while (build.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                    pump();
                    if (reloadCancellation != issuedCancellation) *cancelled = true;
                    if (*cancelled && !cancellationReported) {
                        cancellationReported = true;
                        reload({{"status","building"},{"changedClasses",Json::array()},{"message",""}});
                    }
                    if (stopping) throw std::runtime_error("Debugger shutdown interrupted Java compilation; reload was not requested");
                    if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("JDT incremental build timed out; Java reload was not requested");
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                validateJavaBuildResult(build.get());
                pump(); check();
                if (javaDocuments() != documents) throw std::runtime_error("RevisionConflict: documents changed during JDT compilation; retry Java reload");
                const auto errors = reloadErrors;
                reload({{"status","applying"},{"changedClasses",Json::array()},{"message",""}});
                check();
                applying = true;
                const auto request = send("redefineClasses");
                invalidateInspection();
                auto result = javaHotReloadResult(wait(request, 30));
                pump();
                if (reloadErrors != errors) throw std::runtime_error("Java reload: " + reloadError);
                reload(std::move(result));
            } catch (const std::exception &e) {
                if (applying) invalidateInspection();
                const bool wasCancelled = !applying && (stopping || *cancelled || reloadCancellation != issuedCancellation);
                reload({{"status",wasCancelled ? "cancelled" : "failed"},{"changedClasses",Json::array()},{"message",e.what()}});
                throw;
            }
            return current();
        }
        if (method == "debug.pause") {
            if (stopped && (!runToCursor.active() || runToCursor.stopConfirmed())) return state;
            const auto threads = ask("threads").value("threads",Json::array());
            if (threads.empty()) throw std::runtime_error("No live thread is available to pause");
            const int thread = threads[0].value("id",0);
            ask("pause",{{"threadId",thread}});
        }
        else {
            if (!stopped) throw std::runtime_error("Pause the program before inspecting or stepping");
            if (method == "debug.memory" || method == "debug.disassemble" || method == "debug.exceptionInfo" || method == "debug.variables") {
                auto input=params;
                if(method=="debug.exceptionInfo"&&!input.contains("threadId"))input["threadId"]=state.at("threadId");
                const auto arguments=debugInspectionArguments(method,input,state.at("capabilities"));
                const auto command=method=="debug.memory"?"readMemory":method=="debug.exceptionInfo"?"exceptionInfo":method.substr(6);
                const auto response=debugInspectionResult(method,ask(command,arguments),arguments);
                if(stopGeneration!=params.at("stopGeneration").get<uint64_t>()||phase()!="stopped")throw std::runtime_error("StaleDebugReference: inspection expired while waiting for the adapter");
                if(method=="debug.variables")return {{"variables",response.at("variables")},{"stopGeneration",stopGeneration},{"start",arguments.at("start")},{"count",arguments.at("count")},{"hasMore",response.at("variables").size()>=arguments.at("count").get<size_t>()}};
                return {{"body",response},{"stopGeneration",stopGeneration}};
            } else if (method == "debug.evaluate" || method == "debug.source") {
                const auto result=method=="debug.evaluate"?ask("evaluate",{{"expression",params.at("expression")},{"frameId",state.at("frameId")},{"context",params.value("context","watch")}}):ask("source",{{"sourceReference",params.at("sourceReference")}});
                if(stopGeneration!=params.at("stopGeneration").get<uint64_t>()||phase()!="stopped")throw std::runtime_error("StaleDebugReference: inspection expired while waiting for the adapter");
                return result;
            }
            else if (method == "debug.frame") { scopes(params.at("id")); }
            else if (method == "debug.thread") { expireTargets(); update({{"threadId",params.at("id")}}); inspect(); }
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
                if (activeJava && javaEpoch() != activeJavaEpoch) {
                    clearProcess(); clearInspection("failed", {{"error", "Java language configuration changed; restart debugging"}});
                }
                if(adapterRevoked){clearProcess();clearInspection("failed",{{"error","Debug adapter plugin was disabled or changed"}});}
                pump();
                settleRunToReplies();
                if (runToCursor.cleanupDue() && phase() == "stopped" && !(job && job->method == "debug.stop") && !runToArmRequest && !runToRestoreRequest && !runToContinueRequest)
                    restoreRunToCursor(runToCursor.snapshot().at("runToCursor").at("revision"));
                if (current().value("pendingStop",false) && gotoStopDeadline != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= gotoStopDeadline) {
                    gotoStopDeadline = {};
                    const std::string message = "Debugger did not report the relocated stop; wait for its event or stop the session";
                    auto last = current().at("lastExecution"); last["status"] = "unconfirmed"; last["message"] = message;
                    update({{"lastExecution",last},{"error",message}});
                }
                if (job) { update({{"error",""}}); job->promise.set_value(operation(job->method,job->params,job->adapterEpoch,job->reloadCancellation,job->cancelled)); operationComplete = true; }
                if (channel.running() && phase() == "stopped" && !runToCursor.active() && !current().value("pendingStop",false) && inspectedGeneration != stopGeneration) inspect();
            } catch (const std::exception &e) {
                update({{"error",e.what()}});
                const auto failure=std::current_exception();
                if(adapterRevoked||(job&&!operationComplete&&(job->method=="debug.launch"||job->method=="debug.attach"))) { clearProcess();clearInspection("failed",{{"error",e.what()}}); }
                if (job && !operationComplete) { try { job->promise.set_exception(failure); } catch (...) {} }
                inspectedGeneration = job && (job->method == "debug.hotReload" || job->method == "debug.instructionBreakpoints" || job->method == "debug.stepInTargets" || job->method == "debug.gotoTargets" || job->method == "debug.runToCursor" || job->method == "debug.restoreRunToCursor" || debugMutationOperation(job->method) || debugExecutionOperation(job->method)) ? 0 : stopGeneration;
            }
            if (!channel.running()) { auto currentState = phase(); if (currentState == "running" || currentState == "stopped" || currentState == "starting") {
                const auto reason="Debug adapter exited unexpectedly: "+channel.error();clearProcess();clearInspection("failed",{{"error",reason}});
            } }
            std::unique_lock lock(jobsMutex);
            if (job && job->method == "debug.hotReload") reloadJobs.erase(job->params.at("operationId").get<std::string>());
            if (!channel.running()) wake.wait(lock,[&] { return stopping || adapterRevoked || !jobs.empty(); });
            else wake.wait_for(lock,std::chrono::milliseconds(5),[&] { return stopping || adapterRevoked || !jobs.empty(); });
        }
        clearProcess();
        std::lock_guard lock(jobsMutex);
        for (auto &job : jobs) job->promise.set_exception(std::make_exception_ptr(std::runtime_error("Debugger stopped")));
    }
};
Debugger::Debugger(std::filesystem::path root, Resolver resolve, Terminals &terminals, JavaStart start, JavaEpoch epoch, JavaBuild build, JavaDocuments documents, SourceCheck check, ProfileCheck profileValidator) : impl_(std::make_unique<Impl>(std::move(root),std::move(resolve),terminals,std::move(start),std::move(epoch),std::move(build),std::move(documents),std::move(check),std::move(profileValidator))) {}
Debugger::~Debugger() = default;
Json Debugger::status() const { return impl_->current(); }
Json Debugger::profile() const { std::lock_guard lock(impl_->mutex); return impl_->configuration; }
Json Debugger::execute(const std::string &method, const Json &params) {
    if ((method == "debug.runToCursor" || method == "debug.restoreRunToCursor") && (!params.is_object() || params.dump().size() > 1024 * 1024))
        throw std::runtime_error("Run-to-cursor request exceeds 1 MiB");
    if(method=="debug.adapters")return adapters();
    if (method == "debug.status") return status();
    if (method == "debug.profile") return profile();
    if (method == "debug.cancelReload") {
        if (!params.is_object() || params.size() > 1 || (!params.empty() && !params.contains("operationId"))) throw std::runtime_error("Java reload cancellation accepts only operationId");
        const auto id = params.value("operationId",status().value("javaReload",Json::object()).value("operationId",std::string{}));
        if (id.size() > 128 || !std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';}) || (params.contains("operationId") && id.empty()))
            throw std::runtime_error("Invalid Java reload cancellation ID");
        { std::lock_guard lock(impl_->jobsMutex); const auto found = impl_->reloadJobs.find(id);
            if (found != impl_->reloadJobs.end()) if (const auto job = found->second.lock()) *job->cancelled = true; }
        return status();
    }
    auto job = std::make_shared<Impl::Job>(); job->method = method; job->params = params; auto future = job->promise.get_future();
    if (method == "debug.hotReload") {
        static std::atomic<uint64_t> sequence{0};
        if (!job->params.contains("operationId")) job->params["operationId"] = "java-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++sequence);
        const auto id = job->params.at("operationId").get<std::string>();
        if (id.empty() || id.size() > 128 || !std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';}))
            throw std::runtime_error("Invalid Java reload operation ID");
        job->cancelled = std::make_shared<std::atomic<bool>>(false);
    }
    {std::lock_guard lock(impl_->mutex);job->adapterEpoch=impl_->adapterEpoch;
        if(stoppedDebugOperation(method)&&!debugMutationOperation(method)&&!advancedDebugExecutionOperation(method)&&method!="debug.instructionBreakpoints"&&method!="debug.stepInTargets"&&method!="debug.gotoTargets"&&method!="debug.runToCursor"&&method!="debug.restoreRunToCursor"&&!job->params.contains("stopGeneration"))job->params["stopGeneration"]=impl_->state.value("stopGeneration",uint64_t(0));
        if (debugExecutionOperation(method) && !advancedDebugExecutionOperation(method)) {
            if (!job->params.contains("sessionGeneration")) job->params["sessionGeneration"] = impl_->state.value("sessionGeneration",uint64_t(0));
            if (!job->params.contains("threadId")) job->params["threadId"] = impl_->state.value("threadId",0);
        }}
    { std::lock_guard lock(impl_->jobsMutex); if (impl_->jobs.size() >= 32 || impl_->stopping) throw std::runtime_error("Debugger command queue is full or stopped");
        if (method == "debug.stop") ++impl_->reloadCancellation;
        job->reloadCancellation = impl_->reloadCancellation.load();
        if (method == "debug.hotReload" && !impl_->reloadJobs.emplace(job->params.at("operationId").get<std::string>(),job).second) throw std::runtime_error("Java reload operation ID is already pending");
        impl_->jobs.push_back(job); }
    impl_->wake.notify_one(); return future.get();
}
Json Debugger::adapters() const {
    std::lock_guard lock(impl_->mutex);
    Json entries=Json::array({{{"id","builtin"},{"title","Built-in C++ / .NET"},{"type","automatic"},{"plugin",""},{"requests",{"launch","attach"}}}});
    entries.push_back({{"id","builtin.java"},{"title","Java / JDT LS"},{"type","java"},{"plugin",""},{"requests",Json::array({"launch","attach"})}});
    for(const auto &[id,definition]:impl_->adapters)entries.push_back(debugAdapterSummary(definition));
    return {{"revision",impl_->adapterEpoch},{"adapters",std::move(entries)}};
}
void Debugger::replaceExtensions(const Json &entries) {
    if(!entries.is_array()||entries.size()>32)throw std::runtime_error("Invalid debug adapter registry");
    std::map<std::string,Json> next;
    for(const auto &entry:entries) {
        validateDebugAdapter(entry);const auto id=entry.at("id").get<std::string>();
        const auto owner=entry.value("plugin",std::string{});
        if(id=="builtin"||id=="builtin.java"||owner.empty()||!id.starts_with(owner+".")||!next.emplace(id,entry).second)throw std::runtime_error("Duplicate, built-in or unowned debug adapter");
    }
    {std::lock_guard lock(impl_->mutex);
        if(next==impl_->adapters)return;
        impl_->adapters=std::move(next);++impl_->adapterEpoch;
        if(!impl_->activeAdapter.empty()&&(!impl_->adapters.contains(impl_->activeAdapter)||impl_->adapters.at(impl_->activeAdapter)!=impl_->activeDefinition))impl_->adapterRevoked=true;
    }
    impl_->wake.notify_all();
}
void Debugger::requestShutdown(){impl_->stopping=true;impl_->wake.notify_all();}
} // namespace cycode

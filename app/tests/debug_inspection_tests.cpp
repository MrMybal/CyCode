#include "cycode/DebugInspection.h"
#include "cycode/JavaDebug.h"
#include <functional>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(const std::function<void()> &fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid debugger inspection contract was accepted");
}
} // namespace
void dataBreakpointContracts();
void debugMutationContracts();
void debugExecutionContracts();
void instructionBreakpointContracts();
void stepInTargetContracts();
void gotoTargetContracts();
void runToCursorContracts();
void debugInspectionContracts() {
    dataBreakpointContracts();
    debugMutationContracts();
    debugExecutionContracts();
    instructionBreakpointContracts();
    stepInTargetContracts();
    gotoTargetContracts();
    runToCursorContracts();
    using namespace cycode;
    const auto incremental = javaIncrementalBuildParams();
    require(incremental.is_array() && incremental.size() == 1 && incremental[0].is_boolean() && incremental[0] == false,
            "JDT incremental build must send one positional boolean, not a scalar/named object");
    validateJavaBuildResult(1);
    for (const auto &value : {Json(0),Json(2),Json(3),Json(4),Json(-1),Json(true),Json("SUCCEED"),Json(),Json::object()})
        rejects([&] { validateJavaBuildResult(value); });
    require(javaHotReloadResult({{"changedClasses",{"demo.Main","demo.Main$Nested"}}}).at("status") == "applied",
            "Java reload lost adapter-reported changes");
    require(javaHotReloadResult({{"changedClasses",Json::array()},{"errorMessage",nullptr}}).at("status") == "unchanged",
            "Empty Java reload was reported as applied");
    for (const auto &reply : {Json::object(),Json{{"changedClasses",true}},Json{{"changedClasses",{""}}},
                             Json{{"changedClasses",{42}}},Json{{"changedClasses",{std::string("x\0y",3)}}},
                             Json{{"changedClasses",Json::array()},{"errorMessage","VM does not support redefine"}},
                             Json{{"changedClasses",{"Main"}},{"errorMessage",42}}})
        rejects([&] { javaHotReloadResult(reply); });
    auto excessive = Json::array(); for (int i=0;i<2049;++i) excessive.push_back("Main");
    rejects([&] { javaHotReloadResult({{"changedClasses",excessive}}); });
    for (const auto *type : {"BUILD_COMPLETE","STARTING","END","WARNING","ERROR"})
        require(javaHotReloadEvent({{"changeType",type},{"message","detail"}}).at("changeType") == type,
                "Java custom event was not retained");
    require(javaHotReloadEvent({{"changeType","END"},{"message",nullptr}}).at("message") == "",
            "Nullable Java event message was rejected");
    for (const auto &event : {Json::object(),Json{{"changeType","unknown"}},Json{{"changeType",1}},
                             Json{{"changeType","ERROR"},{"message",false}},Json{{"changeType","WARNING"},{"message",std::string(8193,'x')}}})
        rejects([&] { javaHotReloadEvent(event); });
    require(javaDebugAvailable({{"executeCommandProvider", {{"commands", {"vscode.java.startDebugSession"}}}}}),
            "JDT debug command was not detected");
    require(!javaDebugAvailable(Json::object()) && !javaDebugAvailable({{"executeCommandProvider", true}}),
            "Missing JDT debug add-on was advertised as available");
    validateJavaDebugOptions({{"mainClass", "demo.Main$Nested"}, {"classPaths", {"out", "lib/example.jar"}}, {"vmArgs", {"-ea", "-Dname=hello world"}}}, true);
    validateJavaDebugOptions(Json::object());
    rejects([&] { validateJavaDebugOptions(Json::object(), true); });
    for (auto options : {Json{{"mainClass", "src/Main.java"}}, Json{{"mainClass", "Main\n"}},
                         Json{{"mainClass", "Main Class"}}, Json{{"classPaths", "out"}},
                         Json{{"classPaths", {""}}}, Json{{"vmArgs", {42}}}, Json{{"port", 5005}},
                         Json{{"mainClass", std::string(4097, 'x')}}, Json{{"program", "forged"}}})
        rejects([&] { validateJavaDebugOptions(options); });
    const auto target = javaDebugTarget({{"mainClass", "demo.Main"}, {"classPath", "classes"}},
                                       std::filesystem::current_path(), "java");
    require(target.at("classPaths").size() == 1 && target.at("mainClass") == "demo.Main" &&
                std::filesystem::path(target.at("classPaths")[0].get<std::string>()).is_absolute(),
            "Java build target omitted its actual runtime classpath");
    Json javaProfile = {{"adapterOptions", {{"mainClass", "demo.Main"}, {"classPaths", {"."}}}},
                        {"args", {"hello world", ""}}, {"env", {{"CYCODE_EXAMPLE", "value"}}},
                        {"stopOnEntry", true}, {"terminal", "integrated"}};
    const auto request = javaDebugRequest(javaProfile, std::filesystem::current_path());
    require(request.at("args") == "\"hello world\" \"\"" && request.at("vmArgs")=="" && request.at("console") == "integratedTerminal" &&
                request.at("type") == "java" && request.at("stopOnEntry") == true && !request.contains("program"),
            "Java launch lost literal arguments or common profile ownership");
    require(javaDebugArguments(Json::array({"", "hello world", "quote\"value", "end\\"}), false) ==
                "\"\" \"hello world\" \"quote\\\"value\" \"end\\\\\"", "POSIX Java DAP quoting lost an empty/quoted/backslash argument");
    require(javaDebugArguments(Json::array({"", "hello world", "quote\"value", "end\\"}), true) ==
                "\"\" \"hello world\" \"quote\\\"value\" \"end\\\\\"", "Windows Java DAP quoting lost an empty/quoted/backslash argument");
    require(javaDebugArguments(Json::array({"a\\b"}), false) == "\"a\\\\b\"" &&
                javaDebugArguments(Json::array({"a\\b"}), true) == "\"a\\b\"", "Java platform-specific backslash quoting was conflated");
    javaProfile["env"]["REMOVED"] = nullptr;
    require(javaDebugRequest(javaProfile, std::filesystem::current_path()).at("env").at("REMOVED").is_null(),
            "Java integrated terminal environment removal was lost");
    javaProfile["terminal"] = "console";
    rejects([&] { javaDebugRequest(javaProfile, std::filesystem::current_path()); });
    javaProfile["terminal"] = "integrated";
    javaProfile["adapterOptions"]["classPaths"] = Json::array({"missing-gradle-resources"});
    require(javaDebugRequest(javaProfile, std::filesystem::current_path()).at("classPaths").size() == 1,
            "Java debug dropped a literal Gradle resources classpath entry");
    javaProfile["adapterOptions"].erase("classPaths");
    rejects([&] { javaDebugRequest(javaProfile, std::filesystem::current_path()); });
    const auto endpoint = javaDebugAttachDefaults();
    validateJavaDebugAttach(endpoint);
    for (const auto *host : {"127.0.0.1", "localhost", "build-agent.local", "192.0.2.24", "::1", "2001:db8::2", "fe80::1%en0"}) {
        auto candidate = endpoint; candidate["hostName"] = host; validateJavaDebugAttach(candidate);
    }
    for (const auto &host : {std::string{}, std::string("http://localhost"), std::string("localhost:5005"),
                            std::string("127.0.0.1:5005"), std::string("[::1]"), std::string("host name"),
                            std::string("localhost\n"), std::string("a\0b",3), std::string("%en0"),
                            std::string("::1%"), std::string("::1%zone%extra"), std::string(254,'a')}) {
        auto candidate = endpoint; candidate["hostName"] = host; rejects([&] { validateJavaDebugAttach(candidate); });
    }
    for (auto update : {Json{{"port",0}}, Json{{"port",65536}}, Json{{"port",true}}, Json{{"port","5005"}},
                        Json{{"timeout",99}}, Json{{"timeout",10001}}, Json{{"timeout",false}},
                        Json{{"pid",123}}, Json{{"hostName",42}}}) {
        auto candidate = endpoint; candidate.update(update); rejects([&] { validateJavaDebugAttach(candidate); });
    }
    auto incomplete = endpoint; incomplete.erase("port"); rejects([&] { validateJavaDebugAttach(incomplete); });
    Json attachProfile = {{"javaAttach",endpoint}, {"adapterOptions", {{"projectName","demo"},{"sourcePaths",{"."}},
                          {"javaExec","missing-launcher"},{"classPaths",{"missing-output"}},{"vmArgs",{"-ea"}}}},
                          {"program","unused"},{"args",{"unused"}},{"env",{{"UNUSED","value"}}}};
    const auto attached = javaDebugAttachRequest(attachProfile,std::filesystem::current_path());
    require(attached.at("request")=="attach" && attached.at("type")=="java" && attached.at("hostName")=="127.0.0.1" &&
                attached.at("port")==5005 && attached.at("timeout")==5000 && attached.at("projectName")=="demo" &&
                attached.at("sourcePaths").size()==1 && attached.size()==7,
            "Java attach lost its endpoint/source context or leaked launch-only fields");
    require(javaDebugAttachRequest(Json::object(),std::filesystem::current_path()).at("port")==5005,
            "Legacy Java attach profile did not retain the local endpoint default");
    attachProfile["pid"] = 42;
    rejects([&] { javaDebugAttachRequest(attachProfile,std::filesystem::current_path()); });
    const Json caps = {{"supportsReadMemoryRequest", true},
                       {"supportsDisassembleRequest", true},
                       {"supportsFunctionBreakpoints", true},
                       {"supportsExceptionInfoRequest", true},
                       {"exceptionBreakpointFilters",
                        Json::array({{{"filter", "all"}, {"label", "All exceptions"}, {"default", false}},
                                     {{"filter", "unhandled"}, {"default", true}}})}};
    const auto memory = debugInspectionArguments(
        "debug.memory", {{"memoryReference", "opaque-reference"}, {"count", 4}, {"offset", -16}}, caps);
    require(memory.at("memoryReference") == "opaque-reference" && memory.at("offset") == -16,
            "Debugger rewrote an opaque memory reference");
    const auto bytes =
        debugInspectionResult("debug.memory", {{"address", "0x100"}, {"data", "AAH+/w=="}}, memory);
    require(bytes.at("bytes") == Json::array({0, 1, 254, 255}) && bytes.at("unreadableBytes") == 0,
            "Memory decoding changed actual target bytes");
    const auto partial = debugInspectionResult(
        "debug.memory", {{"address", "0x100"}, {"data", "AAE="}, {"unreadableBytes", 2}}, memory);
    require(partial.at("bytes").size() == 2 && partial.at("unreadableBytes") == 2,
            "Partial memory invented unreadable data");
    for (const auto &bad : {"A", "A===", "AA=A", "AAE$", "AAB=", "AB==", "AAAAAAA="})
        rejects(
            [&] { debugInspectionResult("debug.memory", {{"address", "0x100"}, {"data", bad}}, memory); });
    rejects([&] {
        debugInspectionResult("debug.memory",
                              {{"address", "0x100"}, {"data", "AAH+/w=="}, {"unreadableBytes", 1}}, memory);
    });
    rejects(
        [&] { debugInspectionArguments("debug.memory", {{"memoryReference", "x"}, {"count", 4097}}, caps); });
    rejects([&] {
        debugInspectionArguments("debug.memory", {{"memoryReference", "x"}, {"offset", -1048577}}, caps);
    });
    rejects([&] { debugInspectionArguments("debug.memory", {{"memoryReference", "x"}}, Json::object()); });
    const auto assembly = debugInspectionArguments(
        "debug.disassemble", {{"memoryReference", "pc"}, {"instructionCount", 2}, {"instructionOffset", -1}},
        caps);
    const Json instructions = Json::array(
        {{{"address", "0x100"}, {"instruction", "nop"}}, {{"address", "0x101"}, {"instruction", "ret"}}});
    require(debugInspectionResult("debug.disassemble", {{"instructions", instructions}}, assembly)
                    .at("instructions") == instructions,
            "Disassembly replaced adapter instructions");
    rejects([&] {
        debugInspectionResult(
            "debug.disassemble",
            {{"instructions", Json::array({instructions[0], instructions[0], instructions[0]})}}, assembly);
    });
    rejects([&] {
        debugInspectionResult("debug.disassemble",
                              {{"instructions", Json::array({{{"instruction", "nop"}}})}}, assembly);
    });
    auto variables = debugInspectionArguments(
        "debug.variables",
        {{"variablesReference", 17}, {"start", 200}, {"count", 200}, {"filter", "indexed"}}, caps);
    require(variables.at("start") == 200 && variables.at("filter") == "indexed",
            "Variable paging lost the actual reference/range");
    rejects([&] { debugInspectionArguments("debug.variables", {{"variablesReference", 0}}, caps); });
    rejects([&] {
        debugInspectionArguments("debug.variables", {{"variablesReference", 17}, {"count", 201}}, caps);
    });
    rejects([&] {
        debugInspectionArguments("debug.variables", {{"variablesReference", 17}, {"filter", "registers"}},
                                 caps);
    });
    Json list = Json::array();
    for (int i = 0; i < 201; ++i)
        list.push_back({{"name", "entry"}, {"value", "1"}, {"variablesReference", 0}});
    rejects([&] { debugInspectionResult("debug.variables", {{"variables", list}}, variables); });
    const auto points = Json::array({{{"name", "UActor::Tick"}, {"enabled", true}},
                                     {{"name", "unused"}, {"enabled", false}, {"condition", "x > 1"}}});
    require(functionBreakpointArguments(points, caps).at("breakpoints") ==
                Json::array({{{"name", "UActor::Tick"}}}),
            "Disabled function breakpoint was bound");
    rejects(
        [&] { functionBreakpointArguments(Json::array({{{"name", "main"}, {"condition", "x>0"}}}), caps); });
    auto supported = caps;
    supported["supportsConditionalBreakpoints"] = true;
    supported["supportsHitConditionalBreakpoints"] = true;
    const auto conditions = Json::array({{{"name", "main"}, {"condition", "x > 0"}, {"hitCondition", "3"}}});
    require(functionBreakpointArguments(conditions, supported).at("breakpoints") == conditions,
            "Function breakpoint conditions were discarded");
    rejects([&] { validateFunctionBreakpoints(Json::array({{{"name", "main"}}, {{"name", "main"}}})); });
    rejects([&] { validateFunctionBreakpoints(Json::array({{{"name", ""}}})); });
    require(exceptionBreakpointArguments(nullptr, caps).at("filters") == Json::array({"unhandled"}),
            "Adapter exception defaults were ignored");
    require(exceptionBreakpointArguments(Json::array(), caps).at("filters").empty(),
            "Cleared exception filters were replaced by defaults");
    require(exceptionBreakpointArguments(Json::array({"all"}), caps).at("filters") == Json::array({"all"}),
            "Explicit exception selection was changed");
    rejects([&] { exceptionBreakpointArguments(Json::array({"unknown"}), caps); });
    rejects([&] { validateExceptionFilters(Json::array({"all", "all"})); });
    validateDebugCapabilities(caps);
    rejects([&] { validateDebugCapabilities({{"supportsReadMemoryRequest", "yes"}}); });
    rejects([&] {
        validateDebugCapabilities(
            {{"exceptionBreakpointFilters", Json::array({{{"filter", "x"}, {"label", 1}}})}});
    });
    rejects([&] {
        validateDebugCapabilities(
            {{"exceptionBreakpointFilters", Json::array({{{"filter", "x"}, {"default", 1}}})}});
    });
    require(debugInspectionResult("debug.exceptionInfo",
                                  {{"exceptionId", "std::runtime_error"}, {"breakMode", "unhandled"}},
                                  Json::object())
                    .at("breakMode") == "unhandled",
            "Exception information was fabricated");
    rejects([&] {
        debugInspectionResult("debug.exceptionInfo", {{"exceptionId", "x"}, {"breakMode", "fake"}},
                              Json::object());
    });
    require(stoppedDebugOperation("debug.memory") && stoppedDebugOperation("debug.next") &&
                !stoppedDebugOperation("debug.status"),
            "Stop reference guards omit live operations");
}

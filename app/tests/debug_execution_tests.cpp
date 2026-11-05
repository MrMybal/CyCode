#include "cycode/DebugExecution.h"
#include "cycode/DebugInspection.h"
#include <functional>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(const std::function<void()> &action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid debugger execution contract was accepted");
}
} // namespace
void debugExecutionContracts() {
    using namespace cycode;
    const Json caps = {{"supportsStepBack", true},
                       {"supportsRestartFrame", true},
                       {"supportsSteppingGranularity", true},
                       {"supportsSingleThreadExecutionRequests", true}};
    const Json state = {{"state", "stopped"},  {"sessionGeneration", 3},
                        {"stopGeneration", 7}, {"threadId", 11},
                        {"frameId", 9},        {"frames", Json::array({{{"id", 9}, {"name", "main"}}})},
                        {"capabilities", caps}};
    const Json context = {{"sessionGeneration", 3}, {"stopGeneration", 7}, {"threadId", 11}};
    // Ordinary requests keep their no-options wire contract even on older adapters.
    auto legacy = state;
    legacy["capabilities"] = Json::object();
    for (const auto *method : {"debug.continue", "debug.next", "debug.stepIn", "debug.stepOut"}) {
        require(debugExecutionArguments(method, Json::object(), legacy) == Json({{"threadId", 11}}),
                "A default execution request leaked unsupported granularity/options");
        require(!advancedDebugExecutionOperation(method), "A legacy step lost its queued-context defaults");
    }
    for (const auto *method : {"debug.next", "debug.stepIn", "debug.stepOut", "debug.stepBack"}) {
        for (const auto *granularity : {"statement", "line", "instruction"}) {
            auto request = context;
            request.update({{"granularity", granularity}, {"singleThread", true}});
            require(debugExecutionArguments(method, request, state) ==
                        Json({{"threadId", 11}, {"singleThread", true}, {"granularity", granularity}}),
                    "Stepping changed thread selection or the requested granularity");
        }
    }
    for (const auto *method : {"debug.stepBack", "debug.reverseContinue"}) {
        require(stoppedDebugOperation(method) && advancedDebugExecutionOperation(method),
                "Reverse execution bypassed mandatory stopped context");
        require(debugExecutionArguments(method, context, state) == Json({{"threadId", 11}}),
                "Reverse execution changed the active thread");
        for (const auto *field : {"sessionGeneration", "stopGeneration", "threadId"}) {
            auto request = context;
            request.erase(field);
            rejects([&] { debugExecutionArguments(method, request, state); });
        }
        rejects([&] { debugExecutionArguments(method, context, legacy); });
    }
    for (const auto &update :
         {Json{{"sessionGeneration", 4}}, Json{{"stopGeneration", 8}}, Json{{"threadId", 12}},
          Json{{"sessionGeneration", true}}, Json{{"stopGeneration", -1}}, Json{{"threadId", true}},
          Json{{"threadId", 2147483648LL}}, Json{{"singleThread", 1}}, Json{{"granularity", "expression"}},
          Json{{"targetId", 17}}, Json{{"frameId", 9}}, Json{{"command", "arbitrary"}}}) {
        auto request = context;
        request.update(update);
        rejects([&] { debugExecutionArguments("debug.stepBack", request, state); });
    }
    for (const auto *method : {"debug.continue", "debug.reverseContinue"}) {
        auto request = context;
        request["granularity"] = "instruction";
        rejects([&] { debugExecutionArguments(method, request, state); });
    }
    for (const auto *cap : {"supportsSteppingGranularity", "supportsSingleThreadExecutionRequests"}) {
        auto missing = state;
        missing["capabilities"].erase(cap);
        auto request = context;
        request[std::string(cap) == "supportsSteppingGranularity" ? "granularity" : "singleThread"] =
            std::string(cap) == "supportsSteppingGranularity" ? Json("statement") : Json(false);
        rejects([&] { debugExecutionArguments("debug.next", request, missing); });
    }
    const Json frame = {{"sessionGeneration", 3}, {"stopGeneration", 7}, {"frameId", 9}};
    require(debugFrameRestartable(state) &&
                debugExecutionArguments("debug.restartFrame", frame, state) == Json({{"frameId", 9}}),
            "An advertised restartable selected frame was refused");
    require(stoppedDebugOperation("debug.restartFrame"), "Frame restart bypassed stopped context");
    for (auto changed : {Json{{"frameId", 10}}, Json{{"frames", Json::array()}}, Json{{"state", "running"}},
                         Json{{"frames", Json::array({{{"id", 9}, {"canRestart", false}}})}},
                         Json{{"frames", Json::array({{{"id", 9}, {"canRestart", "yes"}}})}},
                         Json{{"capabilities", Json::object()}}}) {
        auto invalid = state;
        invalid.update(changed);
        require(!debugFrameRestartable(invalid),
                "A stale/unsupported/unrestartable frame enabled its action");
        rejects([&] { debugExecutionArguments("debug.restartFrame", frame, invalid); });
    }
    for (auto update :
         {Json{{"frameId", true}}, Json{{"frameId", -1}}, Json{{"frameId", 10}}, Json{{"singleThread", true}},
          Json{{"threadId", 11}}, Json{{"granularity", "line"}}}) {
        auto invalid = frame;
        invalid.update(update);
        rejects([&] { debugExecutionArguments("debug.restartFrame", invalid, state); });
    }
    for (const auto *key : {"sessionGeneration", "stopGeneration", "frameId"}) {
        auto invalid = frame;
        invalid.erase(key);
        rejects([&] { debugExecutionArguments("debug.restartFrame", invalid, state); });
    }
    for (const auto *method : {"debug.continue", "debug.reverseContinue"}) {
        require(debugExecutionResult(method, Json::object()).empty(),
                "An absent continued-thread report was fabricated");
        require(debugExecutionResult(method, {{"allThreadsContinued", false}}).at("allThreadsContinued") ==
                    false,
                "A partial-thread continuation was published as all threads resumed");
        rejects([&] { debugExecutionResult(method, {{"allThreadsContinued", 1}}); });
    }
    require(debugExecutionResult("debug.restartFrame", nullptr).empty(),
            "An empty restart acknowledgement required invented target data");
    rejects([&] { debugExecutionResult("debug.next", Json::array()); });
    rejects([&] { debugExecutionResult("debug.next", {{"oversized", std::string(65537, 'x')}}); });
    // An invalidated event can alter inspection generations without executing the
    // program. The accepted request must still leave that original pause. A real
    // new stopped event changes the epoch and must survive an immediate reply.
    require(debugExecutionNeedsResume("stopped", 41, 41),
            "Same-pause invalidation blocked an acknowledged resume");
    require(!debugExecutionNeedsResume("stopped", 41, 42),
            "A rapid new pause was overwritten by a late step acknowledgement");
    require(!debugExecutionNeedsResume("exited", 41, 42) && !debugExecutionNeedsResume("running", 41, 42),
            "An execution reply overwrote a termination or continued event");
}

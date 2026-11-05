#include "cycode/DebugExecution.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
int reference(const Json &value, int minimum) {
    if (!value.is_number_integer() || value < minimum || value > 2147483647)
        throw std::runtime_error("Invalid debugger execution reference");
    return value.get<int>();
}
void capability(const Json &caps, const char *key) {
    if (!caps.value(key, false))
        throw std::runtime_error("Debug adapter does not support " + std::string(key));
}
} // namespace
bool advancedDebugExecutionOperation(const std::string &method) {
    return method == "debug.stepBack" || method == "debug.reverseContinue" || method == "debug.restartFrame" ||
           method == "debug.stepIntoTarget" || method == "debug.gotoTarget";
}
bool debugExecutionOperation(const std::string &method) {
    return advancedDebugExecutionOperation(method) || method == "debug.continue" || method == "debug.next" ||
           method == "debug.stepIn" || method == "debug.stepOut";
}
Json debugExecutionArguments(const std::string &method, const Json &p, const Json &state) {
    if (!p.is_object() || !debugExecutionOperation(method) || method == "debug.stepIntoTarget" || method == "debug.gotoTarget")
        throw std::runtime_error("Invalid debugger execution operation");
    if (state.value("state", "") != "stopped" || state.value("pendingStop", false))
        throw std::runtime_error("Pause the program before stepping");
    const bool restart = method == "debug.restartFrame";
    const bool step = method == "debug.next" || method == "debug.stepIn" || method == "debug.stepOut" ||
                      method == "debug.stepBack";
    std::set<std::string> fields = {"sessionGeneration", "stopGeneration", restart ? "frameId" : "threadId"};
    if (!restart)
        fields.insert("singleThread");
    if (step)
        fields.insert("granularity");
    for (const auto &[key, value] : p.items())
        if (!fields.contains(key))
            throw std::runtime_error("Unknown debugger execution field: " + key);
    for (const auto *key : {"sessionGeneration", "stopGeneration"}) {
        if (!p.contains(key)) {
            if (advancedDebugExecutionOperation(method))
                throw std::runtime_error(
                    "Advanced execution requires observed sessionGeneration and stopGeneration");
        } else if (!p.at(key).is_number_integer() ||
                   p.at(key) < (std::string(key) == "sessionGeneration" ? 1 : 0) ||
                   p.at(key) != state.at(key))
            throw std::runtime_error("StaleDebugReference: execution context changed before dispatch");
    }
    const auto &caps = state.at("capabilities");
    if (restart) {
        capability(caps, "supportsRestartFrame");
        const int frameId = reference(p.at("frameId"), 0);
        if (frameId != state.at("frameId"))
            throw std::runtime_error("StaleDebugReference: selected stack frame changed");
        const auto &frames = state.at("frames");
        const auto frame = std::find_if(frames.begin(), frames.end(),
                                        [&](const auto &item) { return item.at("id") == frameId; });
        if (frame == frames.end() || !frame->value("canRestart", true))
            throw std::runtime_error("Selected stack frame cannot be restarted");
        return {{"frameId", frameId}};
    }
    if (method == "debug.stepBack" || method == "debug.reverseContinue")
        capability(caps, "supportsStepBack");
    const int threadId = reference(state.at("threadId"), 1);
    if (!p.contains("threadId") && advancedDebugExecutionOperation(method))
        throw std::runtime_error("Reverse execution requires the observed selected threadId");
    if (p.contains("threadId") && reference(p.at("threadId"), 1) != threadId)
        throw std::runtime_error("StaleDebugReference: selected thread changed");
    Json result = {{"threadId", threadId}};
    if (p.contains("singleThread")) {
        capability(caps, "supportsSingleThreadExecutionRequests");
        if (!p.at("singleThread").is_boolean())
            throw std::runtime_error("singleThread must be boolean");
        result["singleThread"] = p.at("singleThread");
    }
    if (p.contains("granularity")) {
        capability(caps, "supportsSteppingGranularity");
        if (p.at("granularity") != "statement" && p.at("granularity") != "line" &&
            p.at("granularity") != "instruction")
            throw std::runtime_error("Invalid stepping granularity");
        result["granularity"] = p.at("granularity");
    }
    return result;
}
Json debugExecutionResult(const std::string &method, const Json &body) {
    if (!debugExecutionOperation(method))
        throw std::runtime_error("Unknown debugger execution response");
    if (body.is_null() && method != "debug.continue")
        return Json::object();
    if (!body.is_object() || body.dump().size() > 65536)
        throw std::runtime_error("Invalid debugger execution response");
    Json result = Json::object();
    if ((method == "debug.continue" || method == "debug.reverseContinue") &&
        body.contains("allThreadsContinued")) {
        if (!body.at("allThreadsContinued").is_boolean())
            throw std::runtime_error("Invalid allThreadsContinued response");
        result["allThreadsContinued"] = body.at("allThreadsContinued");
    }
    return result;
}
bool debugFrameRestartable(const Json &state) {
    try {
        debugExecutionArguments("debug.restartFrame",
                                {{"sessionGeneration", state.at("sessionGeneration")},
                                 {"stopGeneration", state.at("stopGeneration")},
                                 {"frameId", state.at("frameId")}},
                                state);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
bool debugExecutionNeedsResume(const std::string &phase, uint64_t dispatchEpoch, uint64_t currentEpoch) {
    return phase == "stopped" && dispatchEpoch == currentEpoch;
}
} // namespace cycode

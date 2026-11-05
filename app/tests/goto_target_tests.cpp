#include "cycode/DebugExecution.h"
#include "cycode/DebugInspection.h"
#include "cycode/DebugTargets.h"
#include "cycode/StepInTargets.h"
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
    require(rejected, "Invalid next-statement contract was accepted");
}
} // namespace
void gotoTargetContracts() {
    using namespace cycode;
    constexpr auto kind = DebugTargets::Kind::goTo;
    Json state = {
        {"state", "stopped"},
        {"sessionGeneration", 3},
        {"stopGeneration", 7},
        {"threadId", 11},
        {"frameId", 9},
        {"frames", Json::array({{{"id", 9},
                                 {"source",
                                  {{"path", "/adapter/actual.cpp"},
                                   {"name", "actual.cpp"},
                                   {"sourceReference", 0},
                                   {"adapterData", {{"opaque", "context"}}}}}}})},
        {"capabilities", {{"supportsGotoTargetsRequest", true}, {"supportsStepInTargetsRequest", true}}}};
    auto query = DebugTargets::context(state, kind);
    query["line"] = 40;
    query["column"] = 5;
    const auto arguments = DebugTargets::query(query, state, kind);
    require(arguments.at("source").at("path") == "/adapter/actual.cpp" &&
                arguments.at("source").contains("adapterData") &&
                !arguments.at("source").contains("sourceReference") && arguments.at("line") == 40 &&
                arguments.at("column") == 5 && !arguments.contains("frameId"),
            "Query lost the actual frame source/opaque data or leaked execution context");
    const Json reply = {{"targets", Json::array({{{"id", -2147483648LL},
                                                  {"label", "destination"},
                                                  {"line", 42},
                                                  {"column", 3},
                                                  {"endLine", 42},
                                                  {"endColumn", 8},
                                                  {"instructionPointerReference", "opaque:instruction"}}})}};
    DebugTargets targets(kind);
    auto catalogue = targets.complete(query, reply, state, arguments);
    state.update(targets.snapshot());
    require(DebugTargets::current(catalogue, state, kind) && catalogue.at("requestedLine") == 40 &&
                catalogue.at("requestedColumn") == 5 && catalogue.at("targets")[0].at("line") == 42 &&
                catalogue.at("targets")[0].at("instructionPointerReference") == "opaque:instruction" &&
                !catalogue.at("source").contains("adapterData") &&
                catalogue.at("targets")[0].at("id").is_string(),
            "Issued goto catalogue exposed adapter IDs/data or replaced actual destination coordinates");
    auto execute = DebugTargets::context(state, kind);
    execute["target"] = catalogue.at("targets")[0].at("id");
    require(targets.arguments(execute, state) == Json({{"threadId", 11}, {"targetId", -2147483648LL}}),
            "Goto injected stepping options or changed the signed adapter identity");
    StepInTargets steps;
    auto stepContext = StepInTargets::context(state);
    const auto stepCatalogue =
        steps.complete(stepContext, {{"targets", Json::array({{{"id", 1}, {"label", "callee"}}})}}, state);
    state.update(steps.snapshot());
    require(DebugTargets::current(catalogue, state, kind) && StepInTargets::current(stepCatalogue, state),
            "Independent query families overwrote each other's catalogue marker");
    auto wrong = execute;
    wrong["target"] = stepCatalogue.at("targets")[0].at("id");
    rejects([&] { targets.arguments(wrong, state); });
    for (const auto &update :
         {Json{{"targetId", 1}}, Json{{"path", "caller.cpp"}}, Json{{"source", {{"path", "caller.cpp"}}}},
          Json{{"singleThread", true}}, Json{{"singleThread", false}}, Json{{"granularity", "line"}},
          Json{{"line", 40}}, Json{{"command", "continue"}}}) {
        auto request = execute;
        request.update(update);
        rejects([&] { targets.arguments(request, state); });
    }
    for (const auto *key : {"sessionGeneration", "stopGeneration", "threadId", "frameId"}) {
        auto request = execute;
        request.erase(key);
        rejects([&] { targets.arguments(request, state); });
        auto missing = query;
        missing.erase(key);
        rejects([&] { DebugTargets::query(missing, state, kind); });
        auto changed = state;
        changed[key] = changed.at(key).get<int>() + 1;
        require(!DebugTargets::current(catalogue, changed, kind),
                "A different pause/thread/frame retained goto targets");
        rejects([&] { targets.arguments(execute, changed); });
    }
    auto changed = state;
    changed["frames"][0]["source"]["adapterData"] = {{"opaque", "changed"}};
    rejects([&] { targets.arguments(execute, changed); });
    DebugTargets responseOwner(kind);
    rejects([&] { responseOwner.complete(query, reply, changed, arguments); });
    require(responseOwner.snapshot().at("gotoTargetCatalog") == "",
            "A changed source published a stale query response");
    changed["frames"][0]["source"]["path"] = "/adapter/other.cpp";
    require(!DebugTargets::current(catalogue, changed, kind), "Changed source paths remained current");
    auto pending = state;
    pending["pendingStop"] = true;
    require(!DebugTargets::available(pending, kind) && !StepInTargets::available(pending),
            "A pending relocated stop allowed another target query");
    rejects([&] { targets.arguments(execute, pending); });
    for (const auto *method : {"debug.continue", "debug.next", "debug.stepIn", "debug.stepOut"})
        rejects([&] { debugExecutionArguments(method, Json::object(), pending); });
    require(stoppedDebugOperation("debug.gotoTargets") && stoppedDebugOperation("debug.gotoTarget") &&
                advancedDebugExecutionOperation("debug.gotoTarget") &&
                debugExecutionResult("debug.gotoTarget", nullptr).empty(),
            "Relocation bypassed explicit context or fabricated an acknowledgement body");
    rejects([&] { debugExecutionArguments("debug.gotoTarget", execute, state); });
    for (const auto &update :
         {Json{{"line", 0}}, Json{{"line", true}}, Json{{"line", 10000001}}, Json{{"column", 0}},
          Json{{"column", true}}, Json{{"column", 10000001}}, Json{{"sourceReference", 1}},
          Json{{"targetId", 1}}, Json{{"path", "caller.cpp"}}}) {
        auto request = query;
        request.update(update);
        rejects([&] { DebugTargets::query(request, state, kind); });
    }
    for (const auto &source : {Json::object(), Json{{"name", "only a name"}}, Json{{"sourceReference", true}},
                               Json{{"path", std::string("x\0y", 3)}}, Json{{"path", std::string(8193, 'x')}},
                               Json{{"path", "source.cpp"}, {"adapterData", std::string(65536, 'x')}}}) {
        auto missing = state;
        missing["frames"][0]["source"] = source;
        require(!DebugTargets::available(missing, kind),
                "A malformed/unbounded frame source enabled relocation");
    }
    auto reference = state;
    reference["frames"][0]["source"] = {{"sourceReference", 17}, {"name", "virtual source"}};
    auto referenceArgs = DebugTargets::query(query, reference, kind);
    require(referenceArgs.at("source").at("sourceReference") == 17 &&
                !referenceArgs.at("source").contains("path"),
            "An adapter-owned virtual source required a fabricated filesystem path");
    auto noColumn = query;
    noColumn.erase("column");
    require(!DebugTargets::query(noColumn, state, kind).contains("column"),
            "An absent column was fabricated");
    for (const auto &update : {Json{{"line", nullptr}}, Json{{"line", true}}, Json{{"line", -1}},
                               Json{{"instructionPointerReference", 1}},
                               Json{{"instructionPointerReference", std::string("x\0y", 3)}},
                               Json{{"instructionPointerReference", std::string(8193, 'x')}}}) {
        auto bad = reply;
        bad["targets"][0].update(update);
        rejects([&] { responseOwner.complete(query, bad, state, arguments); });
    }
    auto absent = reply;
    absent["targets"][0].erase("line");
    rejects([&] { responseOwner.complete(query, absent, state, arguments); });
    rejects([&] { responseOwner.complete(query, reply, state); });
    targets.expire();
    rejects([&] { targets.arguments(execute, state); });
    const auto refreshed = targets.complete(query, reply, state, arguments);
    state.update(targets.snapshot());
    require(refreshed.at("catalogId") != catalogue.at("catalogId"),
            "Goto refresh reused a consumed identity");
    rejects([&] { targets.arguments(execute, state); });
    const auto empty = targets.complete(query, {{"targets", Json::array()}}, reference, referenceArgs);
    reference.update(targets.snapshot());
    require(empty.at("targets").empty() && DebugTargets::current(empty, reference, kind),
            "A real empty virtual-source result was replaced by a fabricated target");
}

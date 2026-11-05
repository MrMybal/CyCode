#include "cycode/DebugExecution.h"
#include "cycode/DebugInspection.h"
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
    require(rejected, "Invalid step-in target contract was accepted");
}
} // namespace
void stepInTargetContracts() {
    using namespace cycode;
    Json state = {{"state", "stopped"},
                  {"sessionGeneration", 3},
                  {"stopGeneration", 7},
                  {"threadId", 11},
                  {"frameId", 9},
                  {"frames", Json::array({{{"id", 9}}})},
                  {"capabilities",
                   {{"supportsStepInTargetsRequest", true},
                    {"supportsSteppingGranularity", true},
                    {"supportsSingleThreadExecutionRequests", true}}}};
    const auto context = StepInTargets::context(state);
    require(StepInTargets::available(state) && StepInTargets::query(context, state) == Json({{"frameId", 9}}),
            "Target query lost its selected frame or leaked host context into DAP");
    require(stoppedDebugOperation("debug.stepInTargets") && stoppedDebugOperation("debug.stepIntoTarget") &&
                advancedDebugExecutionOperation("debug.stepIntoTarget"),
            "Targeted execution bypassed mandatory explicit paused context");
    const Json reply = {{"targets", Json::array({{{"id", -2147483648LL},
                                                  {"label", "outer(inner())"},
                                                  {"line", 40},
                                                  {"column", 5},
                                                  {"endLine", 40},
                                                  {"endColumn", 19}},
                                                 {{"id", 2147483647}, {"label", "inner"}}})}};
    StepInTargets targets;
    auto catalogue = targets.complete(context, reply, state);
    state.update(targets.snapshot());
    require(StepInTargets::current(catalogue, state) && catalogue.at("targets").size() == 2 &&
                catalogue.at("targets")[0].at("id").is_string() &&
                catalogue.at("targets")[0].at("label") == "outer(inner())" &&
                catalogue.at("targets")[0].at("endColumn") == 19 &&
                !catalogue.at("targets")[1].contains("line"),
            "Issued targets lost actual labels/ranges or fabricated absent coordinates");
    auto request = context;
    auto plain = context;
    plain["target"] = catalogue.at("targets")[1].at("id");
    require(targets.arguments(plain, state) == Json({{"threadId", 11}, {"targetId", 2147483647}}),
            "An ordinary targeted step injected unrequested granularity/thread options");
    request.update({{"target", catalogue.at("targets")[0].at("id")},
                    {"singleThread", false},
                    {"granularity", "instruction"}});
    require(targets.arguments(request, state) == Json({{"threadId", 11},
                                                       {"targetId", -2147483648LL},
                                                       {"singleThread", false},
                                                       {"granularity", "instruction"}}),
            "Targeted step changed signed adapter IDs or lost explicitly requested options");
    for (const auto *field : {"sessionGeneration", "stopGeneration", "threadId", "frameId"}) {
        auto missing = context;
        missing.erase(field);
        rejects([&] { StepInTargets::query(missing, state); });
        missing = request;
        missing.erase(field);
        rejects([&] { targets.arguments(missing, state); });
        auto stale = state;
        stale[field] = stale.at(field).get<int>() + 1;
        require(!StepInTargets::current(catalogue, stale), "A changed context retained a current catalogue");
        rejects([&] { targets.arguments(request, stale); });
    }
    for (const auto &update :
         {Json{{"sessionGeneration", true}}, Json{{"threadId", 0}}, Json{{"frameId", 2147483648LL}},
          Json{{"stopGeneration", -1}}, Json{{"command", "arbitrary"}}, Json{{"targetId", 123}}}) {
        auto invalid = context;
        invalid.update(update);
        rejects([&] { StepInTargets::query(invalid, state); });
        invalid = request;
        invalid.update(update);
        rejects([&] { targets.arguments(invalid, state); });
    }
    for (const auto &update :
         {Json{{"state", "running"}}, Json{{"frames", Json::array()}}, Json{{"capabilities", Json::object()}},
          Json{{"capabilities", {{"supportsStepInTargetsRequest", "yes"}}}}}) {
        auto changed = state;
        changed.update(update);
        require(!StepInTargets::available(changed) && !StepInTargets::current(catalogue, changed),
                "An unavailable adapter/frame enabled target selection");
        rejects([&] { targets.arguments(request, changed); });
    }
    for (const auto &update : {Json{{"target", "invented"}}, Json{{"target", 1}}, Json{{"singleThread", 1}},
                               Json{{"granularity", "expression"}}}) {
        auto invalid = request;
        invalid.update(update);
        rejects([&] { targets.arguments(invalid, state); });
    }
    for (const auto *cap : {"supportsSingleThreadExecutionRequests", "supportsSteppingGranularity"}) {
        auto missing = state;
        missing["capabilities"].erase(cap);
        rejects([&] { targets.arguments(request, missing); });
    }
    rejects([&] { debugExecutionArguments("debug.stepIntoTarget", request, state); });
    require(debugExecutionResult("debug.stepIntoTarget", nullptr).empty(),
            "An empty targeted-step acknowledgement fabricated stop data");
    targets.expire();
    require(targets.snapshot().at("stepInTargetCatalog") == "", "Expired catalogue remained published");
    rejects([&] { targets.arguments(request, state); });
    auto refreshed = targets.complete(context, reply, state);
    state.update(targets.snapshot());
    require(refreshed.at("catalogId") != catalogue.at("catalogId") &&
                !StepInTargets::current(catalogue, state),
            "Refresh reused old target identities in the same pause/frame");
    rejects([&] { targets.arguments(request, state); });
    for (const auto &point :
         {Json{{"id", true}, {"label", "bad"}}, Json{{"id", -2147483649LL}, {"label", "bad"}},
          Json{{"id", 2147483648LL}, {"label", "bad"}}, Json{{"id", 1}, {"label", ""}},
          Json{{"id", 1}, {"label", std::string("x\0y", 3)}},
          Json{{"id", 1}, {"label", std::string(4097, 'x')}}, Json{{"id", 1}, {"label", "bad"}, {"line", -1}},
          Json{{"id", 1}, {"label", "bad"}, {"column", true}},
          Json{{"id", 1}, {"label", "bad"}, {"endLine", 9007199254740992LL}},
          Json{{"id", 1}, {"label", "bad"}, {"line", 5}, {"endLine", 4}},
          Json{{"id", 1}, {"label", "bad"}, {"line", 5}, {"endLine", 5}, {"column", 4}, {"endColumn", 3}}}) {
        rejects([&] { targets.complete(context, {{"targets", Json::array({point})}}, state); });
        require(targets.snapshot().at("stepInTargetCatalog") == "",
                "Invalid target response retained a partial catalogue");
    }
    rejects([&] {
        targets.complete(context,
                         {{"targets", Json::array({reply.at("targets")[0], reply.at("targets")[0]})}}, state);
    });
    rejects([&] { targets.complete(context, Json::object(), state); });
    rejects([&] { targets.complete(context, {{"targets", nullptr}}, state); });
    auto oversized = reply;
    oversized["extra"] = std::string(512 * 1024, 'x');
    rejects([&] { targets.complete(context, oversized, state); });
    Json many = Json::array();
    for (int id = 0; id < 128; ++id)
        many.push_back({{"id", id}, {"label", "same label"}, {"column", 0}});
    require(targets.complete(context, {{"targets", many}}, state).at("targets").size() == 128,
            "Distinct IDs with the same actual label were incorrectly collapsed");
    many.push_back({{"id", 128}, {"label", "extra"}});
    rejects([&] { targets.complete(context, {{"targets", many}}, state); });
    const auto empty = targets.complete(context, {{"targets", Json::array()}}, state);
    state.update(targets.snapshot());
    require(empty.at("targets").empty() && StepInTargets::current(empty, state),
            "An actual empty catalogue was fabricated into a target or rejected");
}

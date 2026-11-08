#include "cycode/RunToCursor.h"
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
    require(rejected, "Invalid run-to-cursor contract was accepted");
}
} // namespace
void runToCursorContracts() {
    using cycode::RunToCursor;
    const Json state = {{"state", "stopped"},
                        {"sessionGeneration", 3},
                        {"stopGeneration", 7},
                        {"threadId", 11},
                        {"capabilities", Json::object()}};
    const Json original = Json::array({{{"line", 2}, {"condition", "n > 2"}},
                                       {{"line", 9}, {"enabled", false}, {"logMessage", "disabled"}}});
    const Json request = {{"sessionGeneration", 3},
                          {"stopGeneration", 7},
                          {"threadId", 11},
                          {"path", "source.cpp"},
                          {"line", 9},
                          {"expectedDocumentId", 5},
                          {"expectedRevision", 6},
                          {"expectedBreakpoints", original}};
    const auto plan = RunToCursor::plan(request, state, original);
    require(plan.at("original") == original && plan.at("points").size() == 3 &&
                plan.at("points")[1] == original[1] && plan.at("targetIndex") == 1 &&
                plan.at("wireCount") == 2 && plan.at("temporary") == true &&
                plan.at("resume") == Json({{"threadId", 11}}),
            "Temporary breakpoint altered the saved/disabled list or enabled-to-wire mapping");
    for (const auto *key : {"sessionGeneration", "stopGeneration", "threadId", "expectedDocumentId",
                            "expectedRevision", "path", "line", "expectedBreakpoints"}) {
        auto missing = request;
        missing.erase(key);
        rejects([&] { RunToCursor::plan(missing, state, original); });
    }
    for (const auto &update :
         {Json{{"sessionGeneration", true}}, Json{{"sessionGeneration", 4}}, Json{{"stopGeneration", 8}},
          Json{{"threadId", 12}}, Json{{"expectedRevision", 0}}, Json{{"expectedDocumentId", true}},
          Json{{"line", 10000001}}, Json{{"line", 0}}, Json{{"line", true}},
          Json{{"path", std::string("x\0y", 3)}}, Json{{"granularity", "line"}}, Json{{"command", "goto"}},
          Json{{"targetId", 9}}, Json{{"expectedBreakpoints", Json::array()}},
          Json{{"singleThread", true}}}) {
        auto invalid = request;
        invalid.update(update);
        rejects([&] { RunToCursor::plan(invalid, state, original); });
    }
    auto unsignedRequest = request;
    unsignedRequest["expectedDocumentId"] = UINT64_MAX;
    unsignedRequest["expectedRevision"] = uint64_t(6);
    require(RunToCursor::plan(unsignedRequest, state, original).at("original") == original,
            "Unsigned document references were compared through a signed JSON bound");
    auto negative = request;
    negative["expectedDocumentId"] = -1;
    rejects([&] { RunToCursor::plan(negative, state, original); });
    auto single = request;
    single["singleThread"] = false;
    auto caps = state;
    caps["capabilities"] = {{"supportsSingleThreadExecutionRequests", true}};
    require(RunToCursor::plan(single, caps, original).at("resume").at("singleThread") == false,
            "Explicit single-thread false was lost");
    for (const auto &update : {Json{{"state", "running"}}, Json{{"pendingStop", true}}}) {
        auto invalid = state;
        invalid.update(update);
        rejects([&] { RunToCursor::plan(request, invalid, original); });
    }
    auto reused = original;
    reused.push_back({{"line", 9}});
    auto reuseRequest = request;
    reuseRequest["expectedBreakpoints"] = reused;
    const auto reusePlan = RunToCursor::plan(reuseRequest, state, reused);
    require(!reusePlan.at("temporary").get<bool>() && reusePlan.at("points") == reused,
            "An unconditional destination was duplicated");
    for (const auto *field : {"condition", "hitCondition", "logMessage"}) {
        auto collision = reused;
        collision[2][field] = "not empty";
        auto invalid = request;
        invalid["expectedBreakpoints"] = collision;
        rejects([&] { RunToCursor::plan(invalid, state, collision); });
    }
    Json full = Json::array();
    for (int index = 1; index <= 1000; ++index)
        full.push_back({{"line", index}});
    auto crowded = request;
    crowded["expectedBreakpoints"] = full;
    crowded["line"] = 1001;
    rejects([&] { RunToCursor::plan(crowded, state, full); });
    crowded["line"] = 1000;
    require(!RunToCursor::plan(crowded, state, full).at("temporary").get<bool>(),
            "A full list could not reuse its existing unconditional destination");
    const Json response = {
        {"breakpoints", Json::array({{{"verified", true}, {"id", 1}, {"line", 2}},
                                     {{"verified", true}, {"id", -2147483648LL}, {"line", 9}}})}};
    RunToCursor run;
    run.begin(plan);
    rejects([&] { run.begin(plan); });
    run.breakpointEvent({{"verified", false}, {"id", -2147483648LL}}, false);
    run.breakpointEvent({{"verified", true}, {"id", -2147483648LL}, {"line", 9}}, false);
    run.armed(response);
    require(run.original() == original && run.stopConfirmed() && run.needsRestore(),
            "Arming lost the original list/stop confirmation");
    run.continuing();
    require(!run.stopConfirmed(), "Continue submission retained an old paused guarantee");
    rejects([&] { run.restoring(run.snapshot().at("runToCursor").at("revision")); });
    run.stopped({{"reason", "breakpoint"}, {"hitBreakpointIds", Json::array({-2147483648LL})}});
    require(run.cleanupDue() && run.snapshot().at("runToCursor").at("targetHit") == true,
            "Actual signed target hit did not schedule cleanup");
    run.continued({{"allThreadsContinued", false}});
    require(run.snapshot().at("runToCursor").at("continueResult").at("allThreadsContinued") == false,
            "Late acknowledgement fabricated resumed thread state");
    rejects([&] { run.restoring(0); });
    run.restoring(run.snapshot().at("runToCursor").at("revision"));
    rejects([&] { run.restored({{"breakpoints", Json::array()}}); });
    run.fail("restore rejected");
    require(run.active() && run.needsRestore() && !run.cleanupDue(),
            "Uncertain restoration silently released the source lease");
    run.restoring(run.snapshot().at("runToCursor").at("revision"));
    run.restored({{"breakpoints",
                   Json::array({{{"verified", false}, {"message", "Original point remains pending"}}})}});
    require(!run.active() && !run.needsRestore() && run.snapshot().at("runToCursor").at("targetHit") == true,
            "Confirmed original replacement fabricated its verification or lost the hit report");
    for (const auto &update : {Json{{"verified", false}}, Json{{"line", 10}}, Json{{"id", true}}}) {
        RunToCursor rejected;
        rejected.begin(plan);
        auto bad = response;
        bad["breakpoints"][1].update(update);
        rejects([&] { rejected.armed(bad); });
        require(rejected.needsRestore(), "Rejected verification lost cleanup ownership");
    }
    auto absent = response;
    absent["breakpoints"][1].erase("line");
    RunToCursor missing;
    missing.begin(plan);
    rejects([&] { missing.armed(absent); });
    auto ambiguous = response;
    ambiguous["breakpoints"][1]["id"] = 1;
    RunToCursor duplicate;
    duplicate.begin(plan);
    rejects([&] { duplicate.armed(ambiguous); });
    auto oversized = response;
    oversized["extra"] = std::string(512 * 1024, 'x');
    rejects([&] { RunToCursor::verification(oversized, 2); });
    for (const auto &body : {Json{{"reason", "exception"}}, Json{{"reason", "pause"}},
                             Json{{"reason", "breakpoint"}, {"hitBreakpointIds", Json::array({1})}}}) {
        RunToCursor interrupted;
        interrupted.begin(plan);
        interrupted.armed(response);
        interrupted.continuing();
        interrupted.stopped(body);
        const auto hit = interrupted.snapshot().at("runToCursor").at("targetHit");
        require(interrupted.cleanupDue() &&
                    (body.contains("hitBreakpointIds") ? hit == false : hit.is_null()),
                "An earlier stop fabricated a destination hit or failed to clean up");
    }
    RunToCursor early;
    early.begin(plan);
    early.stopped({{"reason", "pause"}});
    early.armed(response);
    rejects([&] { early.continuing(); });
    RunToCursor changed;
    changed.begin(plan);
    changed.armed(response);
    changed.continuing();
    changed.breakpointEvent({{"id", -2147483648LL}, {"verified", false}}, true);
    require(changed.snapshot().at("runToCursor").at("status") == "unconfirmed" && changed.needsRestore(),
            "A removed target breakpoint retained a confirmed running plan");
    changed.stopped({{"reason", "pause"}});
    require(changed.cleanupDue(), "A later pause failed to re-enable cleanup");
    changed.resumed();
    require(!changed.stopConfirmed() && !changed.cleanupDue(),
            "A real continued event left cleanup enabled while running");
    changed.ended();
    require(!changed.active() && !changed.needsRestore(), "Target exit retained a dead process lease");
    RunToCursor flood;
    flood.begin(plan);
    for (int index = 0; index < 257; ++index)
        flood.breakpointEvent({{"verified", true}, {"id", index}}, false);
    rejects([&] { flood.armed(response); });
    const auto revision = run.snapshot().at("runToCursor").at("revision");
    run.reset();
    require(run.snapshot().at("runToCursor").at("revision") > revision &&
                run.snapshot().at("runToCursor").at("status") == "idle",
            "Reset reused an old restoration revision");
}

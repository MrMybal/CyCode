#include "cycode/DebugInspection.h"
#include "cycode/InstructionBreakpoints.h"
#include <functional>
#include <stdexcept>
#include <string>

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
    require(rejected, "Invalid instruction breakpoint contract was accepted");
}
} // namespace
void instructionBreakpointContracts() {
    using namespace cycode;
    const Json caps = {{"supportsInstructionBreakpoints", true},
                       {"supportsConditionalBreakpoints", true},
                       {"supportsHitConditionalBreakpoints", true}};
    const Json points = Json::array({{{"instructionReference", "opaque:frame"},
                                      {"offset", -16},
                                      {"condition", "counter > 1"},
                                      {"hitCondition", "3"}},
                                     {{"instructionReference", "0x100"}, {"enabled", false}},
                                     {{"instructionReference", "0x104"}}});
    const auto plan = InstructionBreakpoints::plan(points, caps);
    require(plan.at("arguments").at("breakpoints") ==
                    Json::array({{{"instructionReference", "opaque:frame"},
                                  {"offset", -16},
                                  {"condition", "counter > 1"},
                                  {"hitCondition", "3"}},
                                 {{"instructionReference", "0x104"}, {"offset", 0}}}) &&
                plan.at("indices") == Json::array({0, 2}),
            "Disabled instruction points reached the adapter or enabled entry mapping changed");
    require(InstructionBreakpoints::draft(plan.at("entries"))[1].at("enabled") == false &&
                !InstructionBreakpoints::draft(plan.at("entries"))[1].contains("verification"),
            "Draft cloning leaked stale verification or dropped disabled entries");
    for (const auto &update :
         {Json{{"instructionReference", ""}}, Json{{"instructionReference", std::string("x\0y", 3)}},
          Json{{"instructionReference", std::string(8193, 'x')}}, Json{{"offset", 1048577}},
          Json{{"offset", -1048577}}, Json{{"offset", true}}, Json{{"enabled", 1}},
          Json{{"condition", std::string(4097, 'x')}}, Json{{"hitCondition", std::string("x\0y", 3)}},
          Json{{"dataId", "foreign"}}, Json{{"mode", "guessed"}}}) {
        auto invalid = points;
        invalid[0].update(update);
        rejects([&] { InstructionBreakpoints::plan(invalid, caps); });
    }
    rejects([&] { InstructionBreakpoints::plan(points, Json::object()); });
    auto missing = caps;
    missing.erase("supportsConditionalBreakpoints");
    rejects([&] { InstructionBreakpoints::plan(points, missing); });
    auto disabled = points;
    disabled[0]["enabled"] = false;
    require(InstructionBreakpoints::plan(disabled, missing).at("arguments").at("breakpoints").size() == 1,
            "An inactive condition required an adapter capability or became a live breakpoint");
    auto duplicate = points;
    duplicate.push_back(points[0]);
    duplicate.back()["enabled"] = false;
    rejects([&] { InstructionBreakpoints::plan(duplicate, caps); });
    require(stoppedDebugOperation("debug.instructionBreakpoints"),
            "Instruction replacement bypassed the paused-stop guard");
    InstructionBreakpoints registry;
    registry.beginReplace(0);
    registry.breakpointEvent(
        {{"id", 11}, {"verified", false}, {"reason", "pending"}, {"message", "Loading symbols"}});
    registry.complete(
        plan,
        {{"breakpoints",
          Json::array({{{"id", 11}, {"verified", true}, {"instructionReference", "0x200"}, {"offset", 4}},
                       {{"id", 12}, {"verified", false}, {"message", "Read-only target"}}})}});
    auto state = registry.snapshot();
    const auto &active = state.at("instructionBreakpoints");
    require(state.at("instructionBreakpointsSynchronized") == true && active.size() == 3 &&
                active[1].at("verification").is_null(),
            "Disabled instruction verification was fabricated or list ordering was lost");
    require(active[0].at("verification").at("verified") == false &&
                active[0].at("verification").at("reason") == "pending" &&
                active[0].at("verification").at("instructionReference") == "0x200" &&
                active[0].at("verification").at("offset") == 4,
            "Pre-acknowledgement events or actual relocated addresses were lost");
    registry.breakpointEvent({{"id", 11}, {"verified", true}});
    require(registry.snapshot().at("instructionBreakpoints")[0].at("verification").at("message") == "" &&
                !registry.snapshot().at("instructionBreakpoints")[0].at("verification").contains("reason"),
            "A verified update retained an obsolete pending failure reason/message");
    registry.breakpointEvent({{"id", 11}}, true);
    require(registry.snapshot().at("instructionBreakpoints")[0].at("verification").at("verified") == false,
            "Adapter removal remained verified");
    const auto before = registry.snapshot();
    rejects([&] { registry.beginReplace(0); });
    require(registry.snapshot() == before, "A revision conflict changed installed instruction points");
    require(!registry.breakpointEvent({{"id", 999}, {"verified", "malformed unrelated source event"}}) &&
                registry.snapshot() == before,
            "An unrelated breakpoint event corrupted the instruction list");
    registry.beginReplace(before.at("instructionBreakpointRevision"));
    rejects([&] { registry.complete(plan, {{"breakpoints", Json::array()}}); });
    registry.fail();
    require(registry.snapshot().at("instructionBreakpointsSynchronized") == false &&
                registry.snapshot().at("instructionBreakpoints") == before.at("instructionBreakpoints"),
            "A failed replacement claimed success or discarded the last known configuration");
    const auto clear = InstructionBreakpoints::plan(Json::array(), caps);
    registry.beginReplace(registry.snapshot().at("instructionBreakpointRevision"));
    registry.complete(clear, {{"breakpoints", Json::array()}});
    require(registry.snapshot().at("instructionBreakpoints").empty() &&
                registry.snapshot().at("instructionBreakpointsSynchronized") == true,
            "Explicit empty replacement did not reconcile uncertain instruction state");
    const auto single =
        InstructionBreakpoints::plan(Json::array({{{"instructionReference", "0x100"}}}), caps);
    for (const auto &point : {Json{{"verified", 1}}, Json{{"verified", true}, {"id", true}},
                              Json{{"verified", false}, {"message", std::string(8193, 'x')}},
                              Json{{"verified", true}, {"instructionReference", ""}},
                              Json{{"verified", true}, {"offset", 9007199254740992LL}},
                              Json{{"verified", false}, {"reason", "invented"}}}) {
        registry.beginReplace(registry.snapshot().at("instructionBreakpointRevision"));
        rejects([&] { registry.complete(single, {{"breakpoints", Json::array({point})}}); });
        registry.fail();
        require(registry.snapshot().at("instructionBreakpointsSynchronized") == false,
                "Malformed verification remained synchronized");
    }
    registry.beginReplace(registry.snapshot().at("instructionBreakpointRevision"));
    rejects([&] {
        registry.complete(plan, {{"breakpoints", Json::array({{{"verified", true}, {"id", 1}},
                                                              {{"verified", true}, {"id", 1}}})}});
    });
    registry.fail();
    registry.beginReplace(registry.snapshot().at("instructionBreakpointRevision"));
    for (int id = 0; id < 257; ++id)
        registry.breakpointEvent({{"id", 1000 + id}, {"verified", true}});
    rejects([&] {
        registry.complete(single, {{"breakpoints", Json::array({{{"verified", true}, {"id", 1}}})}});
    });
    registry.fail();
    registry.beginReplace(registry.snapshot().at("instructionBreakpointRevision"));
    registry.complete(single, {{"breakpoints", Json::array({{{"verified", true}, {"id", -7}}})}});
    require(registry.breakpointEvent({{"id", -7}, {"verified", false}}) &&
                registry.snapshot().at("instructionBreakpoints")[0].at("verification").at("verified") ==
                    false,
            "A signed 32-bit DAP breakpoint ID was rejected or lost its events");
    const auto revision = registry.snapshot().at("instructionBreakpointRevision").get<uint64_t>();
    registry.reset();
    require(registry.snapshot().at("instructionBreakpoints").empty() &&
                registry.snapshot().at("instructionBreakpointRevision") > revision &&
                registry.snapshot().at("instructionBreakpointsSynchronized") == true,
            "A new session retained instruction addresses or reused its previous configuration revision");
    rejects([&] { registry.complete(single, {{"breakpoints", Json::array({{{"verified", true}}})}}); });
    Json maximum = Json::array();
    for (int index = 0; index < 128; ++index)
        maximum.push_back({{"instructionReference", std::to_string(index)}});
    require(InstructionBreakpoints::plan(maximum, caps).at("entries").size() == 128,
            "The documented instruction breakpoint count bound was not supported");
    maximum.push_back({{"instructionReference", "128"}});
    rejects([&] { InstructionBreakpoints::plan(maximum, caps); });
    maximum = Json::array();
    for (int index = 0; index < 128; ++index)
        maximum.push_back({{"instructionReference", std::string(8188, 'x') + std::to_string(index)}});
    rejects([&] { InstructionBreakpoints::plan(maximum, caps); });
    const auto filler = debugInspectionResult(
        "debug.disassemble",
        {{"instructions",
          Json::array(
              {{{"address", "0x100"}, {"instruction", "unreadable"}, {"presentationHint", "invalid"}}})}},
        {{"instructionCount", 1}});
    require(filler.at("instructions")[0].at("presentationHint") == "invalid",
            "Unreachable disassembly lost its invalid marker");
    rejects([&] {
        debugInspectionResult(
            "debug.disassemble",
            {{"instructions",
              Json::array({{{"address", "0x100"}, {"instruction", "nop"}, {"presentationHint", true}}})}},
            {{"instructionCount", 1}});
    });
}

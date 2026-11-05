#include "cycode/InstructionBreakpoints.h"
#include "cycode/DebugInspection.h"
#include <set>
#include <stdexcept>
#include <string>

namespace cycode {
namespace {
using Json = nlohmann::json;
std::string text(const Json &value, size_t maximum, bool nonempty = false) {
    if (!value.is_string() || (nonempty && value.get_ref<const std::string &>().empty()) ||
        value.get_ref<const std::string &>().size() > maximum ||
        value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid instruction breakpoint text");
    return value.get<std::string>();
}
Json verification(const Json &point) {
    auto result = debugBreakpointVerification(point, true);
    if (point.contains("instructionReference"))
        result["instructionReference"] = text(point.at("instructionReference"), 8192, true);
    if (point.contains("offset")) {
        const auto &offset = point.at("offset");
        if (!offset.is_number_integer() || offset < -9007199254740991LL || offset > 9007199254740991LL)
            throw std::runtime_error("Invalid verified instruction breakpoint offset");
        result["offset"] = offset;
    }
    if (point.contains("reason")) {
        if (point.at("reason") != "pending" && point.at("reason") != "failed")
            throw std::runtime_error("Invalid breakpoint verification reason");
        result["reason"] = point.at("reason");
    }
    return result;
}
bool applyEvent(Json &entries, const Json &point, bool removed) {
    bool changed = false;
    for (auto &entry : entries) {
        auto &saved = entry.at("verification");
        if (!saved.is_object() || !saved.contains("id") || saved.at("id") != point.at("id"))
            continue;
        auto next = saved;
        next.update(point);
        if (point.contains("verified") && !point.contains("message"))
            next["message"] = "";
        if (point.value("verified", false) && !point.contains("reason"))
            next.erase("reason");
        if (removed) {
            next["verified"] = false;
            next["message"] = "Removed by debug adapter";
            next.erase("reason");
        }
        saved = verification(next);
        changed = true;
    }
    return changed;
}
} // namespace
InstructionBreakpoints::Json InstructionBreakpoints::plan(const Json &points, const Json &caps) {
    if (!caps.value("supportsInstructionBreakpoints", false))
        throw std::runtime_error("Debug adapter does not support instruction breakpoints");
    if (!points.is_array() || points.size() > 128 || points.dump().size() > 1024 * 1024)
        throw std::runtime_error("Instruction breakpoint limit is 128 entries / 1 MiB");
    Json entries = Json::array(), wire = Json::array(), indices = Json::array();
    std::set<std::pair<std::string, int>> unique;
    for (const auto &point : points) {
        if (!point.is_object())
            throw std::runtime_error("Invalid instruction breakpoint entry");
        for (const auto &[key, value] : point.items())
            if (key != "instructionReference" && key != "offset" && key != "enabled" && key != "condition" &&
                key != "hitCondition")
                throw std::runtime_error("Unknown instruction breakpoint field: " + key);
        const auto reference = text(point.at("instructionReference"), 8192, true);
        const auto offset = point.value("offset", Json(0));
        if (!offset.is_number_integer() || offset < -1048576 || offset > 1048576)
            throw std::runtime_error("Instruction breakpoint byte offset exceeds 1 MiB");
        if (!unique.emplace(reference, offset.get<int>()).second)
            throw std::runtime_error("Duplicate instruction breakpoint reference/offset");
        const auto enabled = point.value("enabled", Json(true));
        if (!enabled.is_boolean())
            throw std::runtime_error("Invalid instruction breakpoint enabled flag");
        Json entry = {{"instructionReference", reference},
                      {"offset", offset},
                      {"enabled", enabled},
                      {"condition", text(point.value("condition", Json("")), 4096)},
                      {"hitCondition", text(point.value("hitCondition", Json("")), 4096)}};
        if (enabled.get<bool>()) {
            Json item = {{"instructionReference", reference}, {"offset", offset}};
            for (const auto &[key, capability] :
                 {std::pair{"condition", "supportsConditionalBreakpoints"},
                  std::pair{"hitCondition", "supportsHitConditionalBreakpoints"}}) {
                if (entry.at(key) == "")
                    continue;
                if (!caps.value(capability, false))
                    throw std::runtime_error("Instruction breakpoint option is unsupported: " +
                                             std::string(key));
                item[key] = entry.at(key);
            }
            indices.push_back(entries.size());
            wire.push_back(std::move(item));
        }
        entry["verification"] = nullptr;
        entries.push_back(std::move(entry));
    }
    return {{"arguments", {{"breakpoints", wire}}}, {"entries", entries}, {"indices", indices}};
}
InstructionBreakpoints::Json InstructionBreakpoints::draft(const Json &active) {
    Json result = active;
    for (auto &entry : result)
        entry.erase("verification");
    return result;
}
void InstructionBreakpoints::beginReplace(uint64_t expectedRevision) {
    if (pending_ || expectedRevision != revision_)
        throw std::runtime_error(
            "RevisionConflict: instruction breakpoint list changed; refresh debug status");
    ++revision_;
    synchronized_ = false;
    pending_ = true;
    deferredLost_ = false;
    deferred_ = Json::array();
    deferredBytes_ = 0;
}
void InstructionBreakpoints::complete(const Json &plan, const Json &reply) {
    if (!pending_ || deferredLost_)
        throw std::runtime_error(
            "Instruction breakpoint event state is uncertain; replace or clear the full list again");
    if (!reply.is_object() || reply.dump().size() > 512 * 1024 || !reply.contains("breakpoints") ||
        !reply.at("breakpoints").is_array() || reply.at("breakpoints").size() != plan.at("indices").size())
        throw std::runtime_error("Invalid instruction breakpoint verification count");
    auto next = plan.at("entries");
    std::set<int> ids;
    for (size_t index = 0; index < reply.at("breakpoints").size(); ++index) {
        auto actual = verification(reply.at("breakpoints")[index]);
        if (actual.contains("id") && !ids.insert(actual.at("id").get<int>()).second)
            throw std::runtime_error("Duplicate instruction breakpoint verification ID");
        next.at(plan.at("indices")[index].get<size_t>())["verification"] = std::move(actual);
    }
    for (const auto &event : deferred_)
        applyEvent(next, event.at("point"), event.at("removed"));
    active_ = std::move(next);
    synchronized_ = true;
    pending_ = false;
    deferred_ = Json::array();
    deferredBytes_ = 0;
}
void InstructionBreakpoints::fail() {
    pending_ = false;
    deferred_ = Json::array();
    deferredBytes_ = 0;
}
bool InstructionBreakpoints::breakpointEvent(const Json &point, bool removed) {
    if (!point.is_object() || !point.contains("id"))
        return false;
    bool known = false;
    for (const auto &entry : active_)
        if (entry.at("verification").is_object() && entry.at("verification").contains("id") &&
            entry.at("verification").at("id") == point.at("id"))
            known = true;
    if (!pending_ && !known)
        return false;
    try {
        if (point.dump().size() > 65536)
            throw std::runtime_error("Excessive instruction breakpoint event");
        auto candidate = point;
        if (!candidate.contains("verified"))
            candidate["verified"] = false;
        auto checked = verification(candidate);
        if (!point.contains("verified"))
            checked.erase("verified");
        if (!point.contains("message"))
            checked.erase("message");
        if (pending_) {
            const auto size = checked.dump().size();
            if (deferred_.size() >= 256 || deferredBytes_ + size > 1024 * 1024)
                deferredLost_ = true;
            else {
                deferred_.push_back({{"point", checked}, {"removed", removed}});
                deferredBytes_ += size;
            }
        }
        return applyEvent(active_, checked, removed) || pending_;
    } catch (const std::exception &) {
        if (pending_)
            deferredLost_ = true;
        else
            synchronized_ = false;
        return true;
    }
}
void InstructionBreakpoints::reset() {
    fail();
    active_ = Json::array();
    synchronized_ = true;
    deferredLost_ = false;
    ++revision_;
}
InstructionBreakpoints::Json InstructionBreakpoints::snapshot() const {
    return {{"instructionBreakpoints", active_},
            {"instructionBreakpointRevision", revision_},
            {"instructionBreakpointsSynchronized", synchronized_}};
}
} // namespace cycode

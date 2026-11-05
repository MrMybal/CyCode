#include "cycode/RunToCursor.h"
#include "cycode/DebugExecution.h"
#include "cycode/DebugInspection.h"
#include <set>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
void positive(const Json &value, uint64_t maximum = UINT64_MAX) {
    if (!value.is_number_integer() || value < 1 || value > maximum)
        throw std::runtime_error("Invalid run-to-cursor reference");
}
void text(const Json &value, size_t maximum, bool empty = false) {
    if (!value.is_string() || (!empty && value.get_ref<const std::string &>().empty()) ||
        value.get_ref<const std::string &>().size() > maximum ||
        value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid run-to-cursor text");
}
} // namespace
RunToCursor::Json RunToCursor::plan(const Json &p, const Json &state, const Json &original) {
    const std::set<std::string> fields = {"path",
                                          "line",
                                          "sessionGeneration",
                                          "stopGeneration",
                                          "threadId",
                                          "expectedDocumentId",
                                          "expectedRevision",
                                          "expectedBreakpoints",
                                          "singleThread"};
    if (!p.is_object() || p.dump().size() > 1024 * 1024)
        throw std::runtime_error("Run-to-cursor request exceeds 1 MiB");
    for (const auto &[key, value] : p.items())
        if (!fields.contains(key))
            throw std::runtime_error("Unknown run-to-cursor field: " + key);
    for (const auto *key : {"sessionGeneration", "threadId", "expectedDocumentId", "expectedRevision"})
        positive(p.at(key));
    if (!p.at("stopGeneration").is_number_integer() || p.at("stopGeneration") < 0)
        throw std::runtime_error("Run to cursor requires an observed stopGeneration");
    text(p.at("path"), 8192);
    positive(p.at("line"), 10000000);
    if (!original.is_array() || original.size() > 1000 || original.dump().size() > 1024 * 1024 ||
        p.at("expectedBreakpoints") != original)
        throw std::runtime_error("BreakpointConflict: refresh the complete source breakpoint list");
    Json context = {{"sessionGeneration", p.at("sessionGeneration")},
                    {"stopGeneration", p.at("stopGeneration")},
                    {"threadId", p.at("threadId")}};
    if (p.contains("singleThread"))
        context["singleThread"] = p.at("singleThread");
    const auto resume = debugExecutionArguments("debug.continue", context, state);
    Json points = original;
    size_t wire = 0, target = 0;
    bool reused = false;
    for (const auto &point : original) {
        positive(point.at("line"), 10000000);
        if (point.contains("enabled") && !point.at("enabled").is_boolean())
            throw std::runtime_error("Invalid source breakpoint enabled flag");
        for (const auto *key : {"condition", "hitCondition", "logMessage"})
            if (point.contains(key))
                text(point.at(key), 4096, true);
        if (!point.value("enabled", true))
            continue;
        if (point.at("line") == p.at("line")) {
            if (reused)
                throw std::runtime_error("Duplicate destination source breakpoints");
            for (const auto *key : {"condition", "hitCondition", "logMessage"})
                if (!point.value(key, "").empty())
                    throw std::runtime_error("Destination already has a conditional breakpoint or logpoint");
            reused = true;
            target = wire;
        }
        ++wire;
    }
    if (!reused) {
        if (points.size() >= 1000)
            throw std::runtime_error("No room for a temporary source breakpoint");
        target = wire++;
        points.push_back({{"line", p.at("line")}});
    }
    return {{"path", p.at("path")},  {"line", p.at("line")}, {"sessionGeneration", p.at("sessionGeneration")},
            {"original", original},  {"points", points},     {"wireCount", wire},
            {"targetIndex", target}, {"temporary", !reused}, {"resume", resume}};
}
RunToCursor::Json RunToCursor::verification(const Json &reply, size_t count) {
    if (!reply.is_object() || reply.dump().size() > 512 * 1024 || !reply.contains("breakpoints") ||
        !reply.at("breakpoints").is_array() || reply.at("breakpoints").size() != count)
        throw std::runtime_error("Invalid source breakpoint replacement response");
    Json result = Json::array();
    for (const auto &point : reply.at("breakpoints")) {
        auto item = debugBreakpointVerification(point, true);
        for (const auto *key : {"line", "column"})
            if (point.contains(key)) {
                if (!point.at(key).is_number_integer() || point.at(key) < 0 ||
                    point.at(key) > 9007199254740991LL)
                    throw std::runtime_error("Invalid actual source breakpoint location");
                item[key] = point.at(key);
            }
        if (point.contains("source")) {
            const auto &source = point.at("source");
            if (!source.is_object())
                throw std::runtime_error("Invalid verified source descriptor");
            Json value = Json::object();
            if (source.contains("path")) {
                text(source.at("path"), 8192);
                value["path"] = source.at("path");
            }
            if (source.contains("sourceReference")) {
                if (!source.at("sourceReference").is_number_integer() || source.at("sourceReference") < 0 ||
                    source.at("sourceReference") > 2147483647)
                    throw std::runtime_error("Invalid verified source reference");
                value["sourceReference"] = source.at("sourceReference");
            }
            item["source"] = std::move(value);
        }
        result.push_back(std::move(item));
    }
    return result;
}
void RunToCursor::begin(const Json &plan) {
    if (active_)
        throw std::runtime_error("A run-to-cursor breakpoint lease is already active");
    plan_ = plan;
    target_ = targetHit_ = nullptr;
    continueResult_ = Json::object();
    active_ = true;
    // Even an unchanged replacement needs an acknowledged response before retry.
    restore_ = true;
    cleanup_ = false;
    status_ = "arming";
    message_.clear();
    stopReason_.clear();
    ++revision_;
    stopped_ = true;
    deferred_ = Json::array();
    deferredBytes_ = 0;
}
void RunToCursor::armed(const Json &reply) {
    if (!active_ || status_ != "arming")
        throw std::runtime_error("Run-to-cursor arming state changed");
    const auto points = verification(reply, plan_.at("wireCount"));
    target_ = points.at(plan_.at("targetIndex").get<size_t>());
    const auto deferred = std::move(deferred_);
    deferred_ = Json::array();
    deferredBytes_ = 0;
    for (const auto &event : deferred) {
        const auto &point = event.at("point");
        if (target_.contains("id") && point.contains("id") && target_.at("id") == point.at("id")) {
            target_.update(verification({{"breakpoints", Json::array({point})}}, 1).at(0));
            if (event.at("removed").get<bool>())
                target_["verified"] = false;
        }
    }
    if (status_ == "unconfirmed")
        throw std::runtime_error(message_);
    if (!target_.at("verified").get<bool>() || !target_.contains("line") ||
        target_.at("line") != plan_.at("line"))
        throw std::runtime_error(
            "Destination breakpoint is unverified or relocated; program was not resumed");
    if (target_.contains("id"))
        for (size_t index = 0; index < points.size(); ++index)
            if (index != plan_.at("targetIndex").get<size_t>() && points[index].contains("id") &&
                points[index].at("id") == target_.at("id"))
                throw std::runtime_error(
                    "Destination shares another breakpoint identity; program was not resumed");
    status_ = "arming";
    ++revision_;
}
void RunToCursor::continuing() {
    if (!active_ || status_ != "arming" || !stopped_ || target_.is_null() || !stopReason_.empty())
        throw std::runtime_error("Run-to-cursor context changed before continuing");
    status_ = "running";
    ++revision_;
    stopped_ = false;
}
void RunToCursor::stopped(const Json &body) {
    if (!active_)
        return;
    stopped_ = true;
    cleanup_ = true;
    targetHit_ = nullptr;
    stopReason_.clear();
    ++revision_;
    try {
        if (!body.is_object() || body.dump().size() > 65536)
            throw std::runtime_error("Invalid stop event");
        text(body.at("reason"), 512);
        stopReason_ = body.at("reason");
        if (status_ == "running" && body.contains("hitBreakpointIds") && target_.is_object() &&
            target_.contains("id")) {
            const auto &ids = body.at("hitBreakpointIds");
            if (!ids.is_array() || ids.size() > 1000)
                throw std::runtime_error("Invalid hit breakpoint IDs");
            bool hit = false;
            for (const auto &id : ids) {
                if (!id.is_number_integer() || id < -2147483648LL || id > 2147483647)
                    throw std::runtime_error("Invalid hit breakpoint ID");
                if (id == target_.at("id"))
                    hit = true;
            }
            targetHit_ = hit;
        }
    } catch (const std::exception &error) {
        message_ = error.what();
        targetHit_ = nullptr;
    }
}
void RunToCursor::continued(const Json &result) {
    continueResult_ = debugExecutionResult("debug.continue", result);
    ++revision_;
}
void RunToCursor::resumed() {
    if (!active_)
        return;
    stopped_ = false;
    cleanup_ = false;
    ++revision_;
}
void RunToCursor::breakpointEvent(const Json &point, bool removed) {
    if (!active_)
        return;
    try {
        if (target_.is_null() && status_ == "arming") {
            const auto bytes = point.dump().size();
            if (bytes > 65536 || deferred_.size() >= 256 || deferredBytes_ + bytes > 1024 * 1024)
                throw std::runtime_error("Too many breakpoint events while arming run to cursor");
            deferred_.push_back({{"point", point}, {"removed", removed}});
            deferredBytes_ += bytes;
            return;
        }
        if (!point.contains("id") || !target_.is_object() || !target_.contains("id") ||
            point.at("id") != target_.at("id"))
            return;
        auto next = verification({{"breakpoints", Json::array({point})}}, 1).at(0);
        const auto previousSource = target_.value("source", Json::object());
        target_.update(next);
        if (removed)
            target_["verified"] = false;
        if (!target_.at("verified").get<bool>() || target_.value("line", 0) != plan_.at("line") ||
            target_.value("source", Json::object()) != previousSource)
            throw std::runtime_error("Run-to-cursor breakpoint changed or was removed");
        ++revision_;
    } catch (const std::exception &error) {
        fail(error.what());
    }
}
void RunToCursor::restoring(uint64_t expectedRevision) {
    if (!active_ || !restore_ || !stopped_ || revision_ != expectedRevision)
        throw std::runtime_error("Run-to-cursor restoration context changed");
    status_ = "restoring";
    cleanup_ = false;
    ++revision_;
}
void RunToCursor::restored(const Json &reply) {
    if (!active_ || status_ != "restoring")
        throw std::runtime_error("Run-to-cursor restoration is not pending");
    size_t count = 0;
    for (const auto &point : plan_.at("original"))
        if (point.value("enabled", true))
            ++count;
    (void)verification(reply, count);
    active_ = restore_ = cleanup_ = false;
    status_ = "stopped";
    message_.clear();
    deferred_ = Json::array();
    deferredBytes_ = 0;
    ++revision_;
}
void RunToCursor::fail(const std::string &message) {
    status_ = "unconfirmed";
    message_ = message.size() <= 8192 ? message : "Run-to-cursor error message exceeds 8192 bytes";
    cleanup_ = false;
    ++revision_;
}
void RunToCursor::ended() {
    if (!active_)
        return;
    active_ = restore_ = cleanup_ = false;
    status_ = "stopped";
    message_ = "Debug target exited";
    ++revision_;
}
void RunToCursor::reset() {
    plan_ = Json::object();
    target_ = targetHit_ = nullptr;
    active_ = restore_ = cleanup_ = stopped_ = false;
    continueResult_ = Json::object();
    deferred_ = Json::array();
    deferredBytes_ = 0;
    status_ = "idle";
    message_.clear();
    stopReason_.clear();
    ++revision_;
}
bool RunToCursor::active() const {
    return active_;
}
bool RunToCursor::needsRestore() const {
    return restore_;
}
bool RunToCursor::cleanupDue() const {
    return active_ && restore_ && cleanup_;
}
bool RunToCursor::stopConfirmed() const {
    return stopped_;
}
RunToCursor::Json RunToCursor::original() const {
    return plan_.at("original");
}
std::string RunToCursor::path() const {
    return plan_.value("path", "");
}
RunToCursor::Json RunToCursor::snapshot() const {
    return {{"runToCursor",
             {{"status", status_},
              {"revision", revision_},
              {"active", active_},
              {"restorePending", restore_},
              {"sessionGeneration", plan_.value("sessionGeneration", uint64_t(0))},
              {"path", path()},
              {"line", plan_.value("line", 0)},
              {"temporary", plan_.value("temporary", false)},
              {"stopConfirmed", stopped_},
              {"verification", target_},
              {"targetHit", targetHit_},
              {"stopReason", stopReason_},
              {"message", message_},
              {"continueResult", continueResult_}}}};
}
} // namespace cycode

#include "cycode/DebugTargets.h"
#include "cycode/DebugExecution.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
const std::set<std::string> contextFields = {"sessionGeneration", "stopGeneration", "threadId", "frameId"};
using Kind = DebugTargets::Kind;
const char *catalogField(Kind kind) {
    return kind == Kind::goTo ? "gotoTargetCatalog" : "stepInTargetCatalog";
}
void fields(const Json &value, bool execution, Kind kind) {
    if (!value.is_object() || value.dump().size() > 4096)
        throw std::runtime_error("Invalid debug target request");
    for (const auto &[key, unused] : value.items())
        if (!contextFields.contains(key) && !(execution && key == "target") &&
            !(execution && kind == Kind::stepIn && (key == "singleThread" || key == "granularity")) &&
            !(!execution && kind == Kind::goTo && (key == "line" || key == "column")))
            throw std::runtime_error("Unknown debug target field: " + key);
}
void sameContext(const Json &parameters, const Json &state, Kind kind) {
    const auto expected = DebugTargets::context(state, kind);
    for (const auto &key : contextFields)
        if (!parameters.contains(key) || !parameters.at(key).is_number_integer() ||
            parameters.at(key) != expected.at(key))
            throw std::runtime_error("StaleDebugReference: debug target context changed; refresh targets");
}
void text(const Json &value, size_t limit = 4096) {
    if (!value.is_string() || value.get_ref<const std::string &>().empty() ||
        value.get_ref<const std::string &>().size() > limit ||
        value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid debug target text");
}
} // namespace
DebugTargets::Json DebugTargets::context(const Json &state, Kind kind) {
    if (state.value("state", "") != "stopped" || state.value("pendingStop", false) ||
        !state.at("capabilities")
             .value(kind == Kind::goTo ? "supportsGotoTargetsRequest" : "supportsStepInTargetsRequest",
                    false))
        throw std::runtime_error("Targets require an inspected pause and advertised adapter capability");
    Json result = Json::object();
    for (const auto &key : contextFields) {
        const auto &value = state.at(key);
        const bool positive = key == "sessionGeneration" || key == "threadId";
        if (!value.is_number_integer() || value < (positive ? 1 : 0) ||
            ((key == "threadId" || key == "frameId") && value > 2147483647))
            throw std::runtime_error("Invalid step-in target context");
        result[key] = value;
    }
    const auto &frames = state.at("frames");
    if (!frames.is_array() || std::none_of(frames.begin(), frames.end(), [&](const auto &frame) {
            return frame.contains("id") && frame.at("id").is_number_integer() &&
                   frame.at("id") == result.at("frameId");
        }))
        throw std::runtime_error("Inspect the selected stack frame before requesting debug targets");
    if (kind == Kind::goTo)
        (void)source(state);
    return result;
}
DebugTargets::Json DebugTargets::source(const Json &state, bool adapterData) {
    const auto &frames = state.at("frames");
    const auto frame = std::find_if(frames.begin(), frames.end(),
                                    [&](const auto &value) { return value.at("id") == state.at("frameId"); });
    if (frame == frames.end() || !frame->contains("source") || !frame->at("source").is_object())
        throw std::runtime_error("Selected frame has no source for setting the next statement");
    const auto &raw = frame->at("source");
    if (raw.dump().size() > 65536)
        throw std::runtime_error("Frame source exceeds 64 KiB");
    Json result = Json::object();
    for (const auto *key : {"path", "name"})
        if (raw.contains(key)) {
            if (!raw.at(key).is_string())
                throw std::runtime_error("Invalid frame source text");
            if (!raw.at(key).get_ref<const std::string &>().empty()) {
                text(raw.at(key), std::string(key) == "path" ? 8192 : 4096);
                result[key] = raw.at(key);
            }
        }
    if (raw.contains("sourceReference")) {
        if (!raw.at("sourceReference").is_number_integer() || raw.at("sourceReference") < 0 ||
            raw.at("sourceReference") > 2147483647)
            throw std::runtime_error("Invalid frame source reference");
        if (raw.at("sourceReference") > 0)
            result["sourceReference"] = raw.at("sourceReference");
    }
    if (!result.contains("path") && !result.contains("sourceReference"))
        throw std::runtime_error("Frame source needs a path or sourceReference");
    if (adapterData && raw.contains("adapterData"))
        result["adapterData"] = raw.at("adapterData");
    return result;
}
DebugTargets::Json DebugTargets::query(const Json &parameters, const Json &state, Kind kind) {
    fields(parameters, false, kind);
    sameContext(parameters, state, kind);
    if (kind == Kind::goTo) {
        Json result = {{"source", source(state, true)}};
        for (const auto *key : {"line", "column"}) {
            if (std::string(key) == "column" && !parameters.contains(key))
                continue;
            if (!parameters.at(key).is_number_integer() || parameters.at(key) < 1 ||
                parameters.at(key) > 10000000)
                throw std::runtime_error("Next-statement coordinates must be one-based and at most 10000000");
            result[key] = parameters.at(key);
        }
        return result;
    }
    return {{"frameId", parameters.at("frameId")}};
}
bool DebugTargets::available(const Json &state, Kind kind) {
    try {
        (void)context(state, kind);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
bool DebugTargets::current(const Json &catalogue, const Json &state, Kind kind) {
    try {
        sameContext(catalogue, state, kind);
        if (kind == Kind::goTo && catalogue.at("source") != source(state))
            return false;
        return catalogue.at("catalogId").is_string() &&
               !catalogue.at("catalogId").get<std::string>().empty() &&
               catalogue.at("catalogId") == state.at(catalogField(kind));
    } catch (const std::exception &) {
        return false;
    }
}
DebugTargets::Json DebugTargets::complete(const Json &parameters, const Json &reply, const Json &state,
                                          const Json &queriedArguments) {
    expire();
    const auto arguments = query(parameters, state, kind_);
    if (kind_ == Kind::goTo && arguments != queriedArguments)
        throw std::runtime_error("StaleDebugReference: frame source changed during target query");
    if (!reply.is_object() || reply.dump().size() > 512 * 1024 || !reply.contains("targets") ||
        !reply.at("targets").is_array() || reply.at("targets").size() > 128)
        throw std::runtime_error("Invalid or oversized debug target response");
    if (serial_ == std::numeric_limits<uint64_t>::max())
        throw std::runtime_error("Debug target catalogue counter exhausted");
    const auto id = std::string(kind_ == Kind::goTo ? "goto-" : "step-") + std::to_string(++serial_);
    auto catalogue = context(state, kind_);
    if (kind_ == Kind::goTo) {
        catalogue["source"] = source(state);
        catalogue["requestedLine"] = parameters.at("line");
        if (parameters.contains("column"))
            catalogue["requestedColumn"] = parameters.at("column");
    }
    catalogue["catalogId"] = id;
    catalogue["targets"] = Json::array();
    std::map<std::string, int> issued;
    std::set<int> rawIds;
    for (const auto &target : reply.at("targets")) {
        if (!target.is_object() || !target.contains("id") || !target.at("id").is_number_integer() ||
            target.at("id") < -2147483648LL || target.at("id") > 2147483647)
            throw std::runtime_error("Invalid debug target ID");
        const auto raw = target.at("id").get<int>();
        if (!rawIds.insert(raw).second)
            throw std::runtime_error("Duplicate debug target ID");
        text(target.at("label"));
        const auto handle = id + "-" + std::to_string(issued.size());
        Json item = {{"id", handle}, {"label", target.at("label")}};
        if (kind_ == Kind::goTo && !target.contains("line"))
            throw std::runtime_error("Goto target requires the actual destination line");
        for (const auto *key : {"line", "column", "endLine", "endColumn"})
            if (target.contains(key)) {
                if (!target.at(key).is_number_integer() || target.at(key) < 0 ||
                    target.at(key) > 9007199254740991LL)
                    throw std::runtime_error("Invalid debug target source range");
                item[key] = target.at(key);
            }
        if (kind_ == Kind::goTo && target.contains("instructionPointerReference")) {
            text(target.at("instructionPointerReference"), 8192);
            item["instructionPointerReference"] = target.at("instructionPointerReference");
        }
        if (item.contains("line") && item.contains("endLine") && item.at("line") > 0 &&
            item.at("endLine") > 0 && item.at("endLine") < item.at("line"))
            throw std::runtime_error("Reversed debug target source range");
        if (item.contains("column") && item.contains("endColumn") && item.at("column") > 0 &&
            item.at("endColumn") > 0 && item.contains("line") && item.contains("endLine") &&
            item.at("line") == item.at("endLine") && item.at("endColumn") < item.at("column"))
            throw std::runtime_error("Reversed debug target column range");
        issued.emplace(handle, raw);
        catalogue["targets"].push_back(std::move(item));
    }
    targets_ = std::move(issued);
    catalogue_ = std::move(catalogue);
    if (kind_ == Kind::goTo)
        sourceContext_ = source(state, true);
    return catalogue_;
}
DebugTargets::Json DebugTargets::arguments(const Json &parameters, const Json &state) const {
    fields(parameters, true, kind_);
    sameContext(parameters, state, kind_);
    if (!current(catalogue_, state, kind_) || !parameters.at("target").is_string() ||
        (kind_ == Kind::goTo && sourceContext_ != source(state, true)))
        throw std::runtime_error("StaleDebugReference: refresh the debug target catalogue");
    const auto found = targets_.find(parameters.at("target").get<std::string>());
    if (found == targets_.end())
        throw std::runtime_error("Unknown or expired debug target handle");
    if (kind_ == Kind::goTo)
        return {{"threadId", parameters.at("threadId")}, {"targetId", found->second}};
    auto step = parameters;
    step.erase("frameId");
    step.erase("target");
    auto result = debugExecutionArguments("debug.stepIn", step, state);
    result["targetId"] = found->second;
    return result;
}
void DebugTargets::expire() {
    catalogue_ = Json::object();
    sourceContext_ = Json::object();
    targets_.clear();
}
DebugTargets::Json DebugTargets::snapshot() const {
    return {{catalogField(kind_), catalogue_.value("catalogId", "")}};
}
} // namespace cycode

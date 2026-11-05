#include "cycode/DataBreakpoints.h"
#include "cycode/DebugInspection.h"
#include <algorithm>
#include <utility>
#include <set>
#include <stdexcept>

namespace cycode {
namespace {
using Json = nlohmann::json;
std::string bounded(const Json &value, size_t limit, bool nonempty = false) {
    if (!value.is_string() || (nonempty && value.get_ref<const std::string &>().empty()) ||
        value.get_ref<const std::string &>().size() > limit || value.get_ref<const std::string &>().find('\0') != std::string::npos)
        throw std::runtime_error("Invalid data breakpoint text");
    return value.get<std::string>();
}
int reference(const Json &value, bool positive) {
    if (!value.is_number_integer() || value < (positive ? 1 : 0) || value > 2147483647)
        throw std::runtime_error("Invalid data breakpoint reference");
    return value.get<int>();
}
void supported(const Json &caps) {
    if (!caps.value("supportsDataBreakpoints", false)) throw std::runtime_error("This debug adapter does not support data breakpoints");
}
Json verification(const Json &point) {
    return debugBreakpointVerification(point);
}
} // namespace
DataBreakpoints::Json DataBreakpoints::arguments(const Json &p, const Json &caps, int frame) {
    supported(caps);
    Json result = {{"name",bounded(p.at("name"),4096,true)}};
    if (p.contains("variablesReference")) result["variablesReference"] = reference(p.at("variablesReference"),true);
    else result["frameId"] = reference(frame,false);
    return result;
}
DataBreakpoints::Json DataBreakpoints::capture(const Json &arguments, const Json &reply, uint64_t session, uint64_t stop) {
    if (!reply.is_object() || reply.dump().size() > 65536 || !reply.contains("dataId") || !session)
        throw std::runtime_error("Invalid data breakpoint information response");
    const auto description = bounded(reply.at("description"),8192);
    const auto accesses = reply.value("accessTypes",Json::array());
    if (!accesses.is_array() || accesses.size() > 3) throw std::runtime_error("Invalid data breakpoint access catalogue");
    std::set<std::string> seen;
    for (const auto &access : accesses) {
        const auto value = bounded(access,16,true);
        if ((value != "read" && value != "write" && value != "readWrite") || !seen.insert(value).second)
            throw std::runtime_error("Invalid or duplicate data breakpoint access type");
    }
    const auto persist = reply.value("canPersist",Json(false));
    if (!persist.is_boolean()) throw std::runtime_error("Invalid data breakpoint persistence flag");
    Json info = {{"available",!reply.at("dataId").is_null()},{"id",""},{"description",description},
                 {"name",bounded(arguments.at("name"),4096,true)},{"accessTypes",accesses},{"canPersist",persist},
                 {"sessionGeneration",session},{"stopGeneration",stop}};
    if (!info.at("available").get<bool>()) return info;
    const auto dataId = bounded(reply.at("dataId"),8192,true);
    const auto id = "data-" + std::to_string(session) + "-" + std::to_string(++serial_);
    info["id"] = id;
    const auto bytes = info.dump().size() + dataId.size();
    if (candidates_.size() >= 128 || bytes_ + bytes > 1024*1024)
        throw std::runtime_error("Data breakpoint inspection limit reached; resume/pause to refresh handles");
    candidates_.emplace(id,Candidate{info,dataId,session,stop}); bytes_ += bytes;
    return info;
}
DataBreakpoints::Json DataBreakpoints::prepare(const Json &points, const Json &caps, uint64_t session, uint64_t stop) const {
    supported(caps);
    if (!points.is_array() || points.size() > 32) throw std::runtime_error("Data breakpoint limit is 32");
    Json wire = Json::array(), entries = Json::array();
    std::set<std::string> usedData;
    for (const auto &point : points) {
        if (!point.is_object()) throw std::runtime_error("Invalid data breakpoint entry");
        for (const auto &[key,value] : point.items())
            if (key != "id" && key != "accessType" && key != "condition" && key != "hitCondition") throw std::runtime_error("Unknown data breakpoint field");
        const auto id = bounded(point.at("id"),128,true);
        const auto found = candidates_.find(id);
        if (found == candidates_.end() || found->second.session != session || found->second.stop != stop)
            throw std::runtime_error("StaleDebugReference: inspect every desired data breakpoint at the current pause before replacing the list");
        const auto &candidate = found->second;
        if (!usedData.insert(candidate.dataId).second) throw std::runtime_error("Duplicate data breakpoint target");
        Json item = {{"dataId",candidate.dataId}};
        auto entry = candidate.publicInfo;
        entry["accessType"] = ""; entry["condition"] = ""; entry["hitCondition"] = "";
        if (point.contains("accessType")) {
            const auto access = bounded(point.at("accessType"),16,true);
            const auto &allowed = candidate.publicInfo.at("accessTypes");
            if (std::find(allowed.begin(),allowed.end(),access) == allowed.end()) throw std::runtime_error("Data breakpoint access type was not advertised");
            item["accessType"] = entry["accessType"] = access;
        }
        for (const auto &[key,capability] : {std::pair{"condition","supportsConditionalBreakpoints"},std::pair{"hitCondition","supportsHitConditionalBreakpoints"}}) {
            const auto value = bounded(point.value(key,Json("")),4096);
            if (!value.empty()) {
                if (!caps.value(capability,false)) throw std::runtime_error("Data breakpoint option is unsupported: " + std::string(key));
                item[key] = entry[key] = value;
            }
        }
        wire.push_back(std::move(item)); entries.push_back(std::move(entry));
    }
    return {{"arguments",{{"breakpoints",wire}}},{"entries",entries}};
}
void DataBreakpoints::beginReplace(uint64_t expectedRevision) {
    if (expectedRevision != revision_) throw std::runtime_error("RevisionConflict: data breakpoint configuration changed; refresh debug status");
    ++revision_; synchronized_ = false;
}
void DataBreakpoints::complete(const Json &plan, const Json &reply) {
    if (!reply.is_object() || reply.dump().size() > 512*1024 || !reply.contains("breakpoints") ||
        !reply.at("breakpoints").is_array() || reply.at("breakpoints").size() != plan.at("entries").size())
        throw std::runtime_error("Invalid data breakpoint verification count");
    auto next = plan.at("entries");
    std::set<int> ids;
    for (size_t index = 0; index < next.size(); ++index) {
        auto confirmed = verification(reply.at("breakpoints")[index]);
        if (confirmed.contains("id") && !ids.insert(confirmed.at("id").get<int>()).second) throw std::runtime_error("Duplicate data breakpoint verification ID");
        next[index]["verification"] = std::move(confirmed);
    }
    active_ = std::move(next); synchronized_ = true;
}
bool DataBreakpoints::breakpointEvent(const Json &point, bool removed) {
    if (!point.is_object() || !point.contains("id")) return false;
    bool changed = false;
    for (auto &entry : active_)
        if (entry.at("verification").contains("id") && entry.at("verification").at("id") == point.at("id")) {
            auto merged = entry.at("verification"); merged.update(point);
            if (removed) { merged["verified"] = false; merged["message"] = "Removed by debug adapter"; }
            entry["verification"] = verification(merged); changed = true;
        }
    return changed;
}
void DataBreakpoints::expire() { candidates_.clear(); bytes_ = 0; }
void DataBreakpoints::reset() { expire(); active_ = Json::array(); synchronized_ = true; ++revision_; }
DataBreakpoints::Json DataBreakpoints::snapshot() const {
    return {{"dataBreakpoints",active_},{"dataBreakpointRevision",revision_},{"dataBreakpointsSynchronized",synchronized_}};
}
} // namespace cycode

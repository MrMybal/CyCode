#include "cycode/Engine.h"
#include "cycode/Hierarchy.h"
#include "cycode/TextEdits.h"
#include <algorithm>
namespace cycode {
namespace {
std::string labelText(std::string value) {
    for (auto &c : value)
        if (static_cast<unsigned char>(c) < 32 || c == 127)
            c = ' ';
    return value;
}
} // namespace
std::string Engine::hierarchyInvalid(const HierarchySession &session) const {
    if (std::chrono::steady_clock::now() - session.created > std::chrono::minutes(5))
        return "Hierarchy session expired; start again at the symbol";
    const auto document = documents_.find(session.path);
    if (document == documents_.end() || document->second.revision != session.revision ||
        document->second.identity != session.identity || hierarchyGeneration_ != session.generation ||
        monitor_.snapshot().revision != session.diskGeneration ||
        language_.epoch(session.path) != session.epoch)
        return "Hierarchy context changed; refresh at the symbol";
    return {};
}
Json Engine::hierarchyNode(HierarchySession &session, const Json &raw) {
    validateHierarchyItem(raw);
    if (!raw.at("uri").get_ref<const std::string &>().starts_with("file:"))
        return nullptr;
    // Navigation issues only workspace or registered read-only SDK references.
    const auto location = sourceLocationLocked(raw.at("uri"), hierarchyRange(raw.at("selectionRange")),
                                               session.includeExternal);
    if (location.is_null())
        return nullptr;
    const auto encoded = raw.dump();
    for (const auto &[id, item] : session.items)
        if (item == raw)
            return session.descriptions.at(id);
    if (session.items.size() >= 512)
        return nullptr;
    const auto id = "node-" + std::to_string(session.items.size() + 1);
    auto description = location;
    description["id"] = id;
    description["name"] = labelText(raw.at("name"));
    description["detail"] = labelText(raw.value("detail", std::string{}));
    description["kind"] = raw.at("kind");
    const auto bytes = encoded.size() + description.dump().size();
    if (session.bytes + bytes > 2 * 1024 * 1024)
        return nullptr;
    session.items.emplace(id, raw); // Preserve opaque server data, never accept it from a caller.
    session.descriptions.emplace(id, description);
    session.bytes += bytes;
    return description;
}
Json Engine::hierarchyOperation(const std::string &method, const Json &params) {
    std::unique_lock lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (auto it = hierarchies_.begin(); it != hierarchies_.end();)
        if (now - it->second.created > std::chrono::minutes(5))
            it = hierarchies_.erase(it);
        else
            ++it;
    if (method == "language.hierarchyStatus") {
        const auto found = hierarchies_.find(params.at("sessionId").get<std::string>());
        const auto reason = found == hierarchies_.end()
                                ? std::string("Hierarchy session expired; start again at the symbol")
                                : hierarchyInvalid(found->second);
        return {{"valid", reason.empty()}, {"reason", reason}};
    }
    const bool prepare = method == "language.hierarchyPrepare";
    if (!prepare && method != "language.hierarchyExpand")
        throw std::runtime_error("Unsupported hierarchy operation");
    HierarchySession candidate;
    std::string sessionId, nodeId, direction;
    Json request;
    if (prepare) {
        candidate.path = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
        candidate.kind = params.at("kind");
        hierarchyMethod(candidate.kind); // Validate before opening/synchronizing a document.
        if (!language_.accepts(candidate.path))
            throw std::runtime_error("No language backend for this file type");
        auto &document = openLocked(candidate.path);
        if (params.at("expectedRevision") != document.revision)
            throw std::runtime_error("RevisionConflict: hierarchy source changed");
        (void)utf16Offset(document.text, params.at("position"));
        candidate.revision = document.revision;
        candidate.identity = document.identity;
        candidate.generation = hierarchyGeneration_;
        candidate.diskGeneration = monitor_.snapshot().revision;
        candidate.epoch = language_.epoch(candidate.path);
        candidate.backend = language_.backend(candidate.path);
        candidate.includeExternal = params.value("includeExternal", false);
        request = {{"textDocument", {{"uri", LanguageClient::uri(resolve(candidate.path))}}},
                   {"position", params.at("position")}};
        sessionId = "hierarchy-" + std::to_string(now.time_since_epoch().count()) + "-" +
                    std::to_string(nextHierarchy_++);
    } else {
        sessionId = params.at("sessionId");
        const auto found = hierarchies_.find(sessionId);
        if (found == hierarchies_.end())
            throw std::runtime_error("Hierarchy session expired; start again at the symbol");
        const auto reason = hierarchyInvalid(found->second);
        if (!reason.empty())
            throw std::runtime_error("RevisionConflict: " + reason);
        candidate = found->second;
        nodeId = params.at("item");
        direction = params.at("direction");
        if (direction.empty())
            throw std::runtime_error("Choose a hierarchy expansion direction");
        hierarchyMethod(candidate.kind, direction);
        if (!candidate.items.contains(nodeId))
            throw std::runtime_error("Unknown issued hierarchy item");
        request = {{"item", candidate.items.at(nodeId)}};
    }
    request["cycodeLanguage"] = candidate.backend; // Follow-up items may refer to SDK headers.
    auto future = language_.request(hierarchyMethod(candidate.kind, direction), std::move(request));
    lock.unlock();
    auto response = future.get();
    lock.lock();
    const auto reason = hierarchyInvalid(candidate);
    if (!reason.empty())
        throw std::runtime_error("RevisionConflict: " + reason);
    if (!prepare && !hierarchies_.contains(sessionId))
        throw std::runtime_error("Hierarchy session was evicted while querying");
    if (response.is_null())
        response = Json::array();
    if (!response.is_array() || response.dump().size() > 2 * 1024 * 1024)
        throw std::runtime_error("Invalid or oversized hierarchy response");
    Json nodes = Json::array();
    size_t omitted = 0, count = 0, publicBytes = 0;
    bool truncated = false;
    for (const auto &entry : response) {
        if (count++ >= 256) {
            truncated = true;
            omitted += response.size() - 256;
            break;
        }
        const auto &raw =
            prepare || candidate.kind == "type" ? entry : entry.at(direction == "incoming" ? "from" : "to");
        auto node = hierarchyNode(candidate, raw);
        if (node.is_null()) {
            ++omitted;
            continue;
        }
        Json sites = Json::array();
        if (!prepare && candidate.kind == "call") {
            const auto &ranges = entry.at("fromRanges");
            if (!ranges.is_array())
                throw std::runtime_error("Invalid hierarchy call-site ranges");
            const auto &owner = direction == "incoming" ? raw : candidate.items.at(nodeId);
            for (const auto &range : ranges) {
                if (sites.size() >= 256) {
                    truncated = true;
                    break;
                }
                const auto checked = hierarchyRange(range);
                auto site = sourceLocationLocked(owner.at("uri"), checked, candidate.includeExternal);
                if (!site.is_null())
                    sites.push_back(std::move(site));
            }
        }
        Json edge = {{"item", std::move(node)}, {"sites", std::move(sites)}};
        publicBytes += edge.dump().size();
        if (publicBytes > 2 * 1024 * 1024) {
            truncated = true;
            ++omitted;
            break;
        }
        nodes.push_back(std::move(edge));
    }
    // Stage cache additions before publishing. Concurrent expansions merge issued nodes
    // only by rejecting a changed cache; no returned ID can be silently rebound.
    if (!prepare) {
        const auto &current = hierarchies_.at(sessionId);
        for (const auto &[id, raw] : current.items)
            if (!candidate.items.contains(id) || candidate.items.at(id) != raw)
                throw std::runtime_error("Hierarchy expanded concurrently; retry this branch");
    }
    size_t bytes = candidate.bytes;
    for (const auto &[id, session] : hierarchies_)
        if (id != sessionId)
            bytes += session.bytes;
    while ((prepare && hierarchies_.size() >= 8) || bytes > 8 * 1024 * 1024) {
        auto oldest = hierarchies_.end();
        for (auto it = hierarchies_.begin(); it != hierarchies_.end(); ++it)
            if (it->first != sessionId &&
                (oldest == hierarchies_.end() || it->second.created < oldest->second.created))
                oldest = it;
        if (oldest == hierarchies_.end())
            break;
        bytes -= oldest->second.bytes;
        hierarchies_.erase(oldest);
    }
    auto result =
        Json{{"sessionId", sessionId}, {"path", candidate.path},    {"revision", candidate.revision},
             {"kind", candidate.kind}, {"nodes", std::move(nodes)}, {"truncated", truncated},
             {"omitted", omitted}};
    hierarchies_[sessionId] = std::move(candidate);
    return result;
}
} // namespace cycode

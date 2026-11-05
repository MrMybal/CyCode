#include "cycode/Engine.h"
#include "cycode/TextFile.h"
namespace cycode {
Json Engine::resourceDocuments() {
    Json result = Json::array();
    std::lock_guard lock(mutex_);
    for (const auto &[path, doc] : documents_)
        result.push_back({{"path", path},
                          {"id", doc.identity},
                          {"revision", doc.revision},
                          {"dirty", doc.dirty},
                          {"diskConflict", doc.diskConflict},
                          {"missing", !doc.diskExisted}});
    return result;
}
Json Engine::resourceCommit(const Json &plan, const std::function<void()> &commit) {
    const std::string operation = plan.at("operation"),
                      from = operation == "restore" || operation == "createFile" ||
                                     operation == "createDirectory"
                                 ? ""
                                 : plan.at("path").get<std::string>(),
                      to = plan.at("destination");
    std::lock_guard lock(mutex_);
    for(const auto &closed:plan.value("closedRoots",Json::array()))
        for(const auto &[path,doc]:documents_)if(resourceContains(closed,path))
            throw std::runtime_error("Close affected working-file and project-trash documents before this action");
    Json affected = Json::array();
    std::vector<std::pair<std::string, std::string>> moved;
    for (const auto &[path, doc] : documents_)
        if (resourceContains(from, path) || resourceContains(to, path)) {
            affected.push_back({{"path", path},
                                {"id", doc.identity},
                                {"revision", doc.revision},
                                {"dirty", doc.dirty},
                                {"diskConflict", doc.diskConflict},
                                {"missing", !doc.diskExisted}});
            const bool reattach = operation == "restore" && !doc.diskExisted && doc.diskConflict == "missing";
            if (doc.dirty || (!doc.diskConflict.empty() && !reattach))
                throw std::runtime_error("Save or reconcile affected documents before changing their paths");
            if (!resourceContains(from, path) && !reattach)
                throw std::runtime_error("An open buffer already occupies the destination");
            if (!reattach && readTextFileBytes(resolve(path)) != doc.disk)
                throw std::runtime_error("DiskConflict: open resource changed");
            if (reattach) {
                auto payload = root_ / ".cycode/trash" / plan.at("trash").get<std::string>() / "payload";
                const auto relative = utf8Path(path).lexically_relative(utf8Path(to));
                if (relative != ".")
                    payload /= relative;
                if (readTextFileBytes(payload) != doc.disk)
                    throw std::runtime_error(
                        "Close the missing buffer before restoring a different disk baseline");
            }
            moved.push_back({path, to.empty() ? "" : reattach ? path : resourceRebase(path, from, to)});
        }
    if (affected != plan.at("openDocuments"))
        throw std::runtime_error("RevisionConflict: open resources changed since preview");
    commit(); // All potentially failing document/path checks precede the filesystem mutation.
    Json documents = Json::array(), warnings = Json::array();
    for (auto &[oldPath, newPath] : moved) {
        if (newPath.empty()) {
            auto &doc = documents_.at(oldPath);
            doc.diskExisted = false;
            doc.diskConflict = "missing";
            doc.revision = nextRevision_++;
            documentEvent("document.missing",oldPath,doc);
            try {
                language_.sync(oldPath, doc.text, doc.revision, doc.identity);
            } catch (const std::exception &e) {
                warnings.push_back(e.what());
            }
            documents.push_back(
                {{"id", doc.identity}, {"from", oldPath}, {"to", oldPath}, {"missing", true}});
            continue;
        }
        auto node = documents_.extract(oldPath);
        try {
            language_.close(oldPath);
        } catch (const std::exception &e) {
            warnings.push_back(e.what());
        }
        node.key().swap(newPath);
        auto inserted = documents_.insert(std::move(node));
        auto &doc = inserted.position->second;
        doc.revision = nextRevision_++;
        if (operation == "restore") {
            doc.diskExisted = true;
            doc.diskConflict.clear();
        }
        documentEvent(operation=="restore"?"document.restored":"document.moved",inserted.position->first,doc);
        try {
            language_.sync(inserted.position->first, doc.text, doc.revision, doc.identity);
        } catch (const std::exception &e) {
            warnings.push_back(e.what());
        }
        documents.push_back({{"id", doc.identity}, {"from", oldPath}, {"to", inserted.position->first}});
    }
    monitor_.invalidate();
    Json effect = {{"from", from},
                   {"to", operation=="removeFile"?"":to},
                   {"removed", operation == "delete"||operation=="removeFile"},
                   {"documents", documents},
                   {"warnings", warnings}};
    {
        std::lock_guard events(resourceEventsMutex_);
        effect["revision"] = ++resourceRevision_;
        resourceEvents_.push_back(effect);
        resourceEventBytes_ += effect.dump().size();
        while (resourceEvents_.size() > 128 || resourceEventBytes_ > 4 * 1024 * 1024) {
            resourceEventBytes_ -= resourceEvents_.front().dump().size();
            resourceEvents_.erase(resourceEvents_.begin());
        }
    }
    events_.publish("resources.changed",{{"operation",operation},{"from",from},{"to",to},{"revision",effect.at("revision")},{"documentCount",documents.size()}});
    return effect;
}
Json Engine::resourceOperation(const std::string &method, const Json &params) {
    if (method == "resources.events") {
        std::lock_guard events(resourceEventsMutex_);
        const auto after = params.value("after", uint64_t(0));
        Json result = Json::array();
        if (after > resourceRevision_)
            throw std::runtime_error("Invalid resource event cursor");
        for (const auto &event : resourceEvents_)
            if (event.at("revision").get<uint64_t>() > after)
                result.push_back(event);
        const bool gap =
            !resourceEvents_.empty() && after < resourceEvents_.front().at("revision").get<uint64_t>() - 1;
        return {{"revision", resourceRevision_}, {"events", result}, {"gap", gap}};
    }
    std::lock_guard serial(resourcesMutex_);
    if (method == "resources.trash")
        return resources_->trash(params.value("limit", size_t(256)));
    if (method == "resources.children")
        return resources_->children(params.value("path", std::string(".")));
    if (method == "resources.preview")
        return resources_->preview(params, resourceDocuments());
    auto commit = [&](const Json &plan, const std::function<void()> &action) {
        return resourceCommit(plan, action);
    };
    Json result;
    if (method == "resources.apply")
        result = resources_->apply(params.at("id"), commit);
    else if (method == "resources.undo")
        result = resources_->undo(params.at("id"), resourceDocuments(), commit);
    else
        throw std::runtime_error("Unknown resource operation");
    checkpointRecovery();
    return result;
}
} // namespace cycode

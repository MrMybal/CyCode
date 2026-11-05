#include "cycode/Engine.h"
#include "cycode/TextFile.h"
namespace cycode {
Json Engine::pollWorkspace() {
    std::lock_guard pollLock(workspacePollMutex_);
    const auto watch = monitor_.snapshot();
    Json reloaded = Json::array(), conflicts = Json::array();
    const auto now = std::chrono::steady_clock::now();
    if (watch.revision != polledWorkspaceRevision_ || now - lastWorkspacePoll_ >= std::chrono::seconds(2)) {
        if(watch.revision!=polledWorkspaceRevision_)events_.publish("workspace.changed",{{"revision",watch.revision},{"indexRevision",watch.indexRevision}});
        polledWorkspaceRevision_ = watch.revision;
        lastWorkspacePoll_ = now;
        struct Snapshot {
            std::string path, disk;
            uint64_t revision;
            bool dirty;
        };
        std::vector<Snapshot> documents;
        {
            std::lock_guard lock(mutex_);
            for (const auto &[path, doc] : documents_)
                documents.push_back({path, doc.disk, doc.revision, doc.dirty});
        }
        for (const auto &doc : documents) {
            std::string conflict;
            try {
                const auto file = resolve(doc.path);
                if (!std::filesystem::is_regular_file(file))
                    conflict = "missing";
                else {
                    const auto raw = readTextFileBytes(file);
                    if (raw != doc.disk) {
                        if (doc.dirty)
                            conflict = "changed";
                        else {
                            dispatch("documents.reload",
                                     {{"path", doc.path}, {"expectedRevision", doc.revision}});
                            reloaded.push_back(doc.path);
                        }
                    }
                }
            } catch (const std::exception &e) {
                conflict = std::string(e.what()).starts_with("RevisionConflict") ? "changed" : "unreadable";
            }
            std::lock_guard lock(mutex_);
            if (auto found = documents_.find(doc.path); found != documents_.end()) {
                const auto previous=found->second.diskConflict;
                found->second.diskConflict = conflict;
                if(previous!=conflict)documentEvent("document.conflict",doc.path,found->second);
            }
        }
    }
    {
        std::lock_guard lock(mutex_);
        for (const auto &[path, doc] : documents_)
            if (!doc.diskConflict.empty())
                conflicts.push_back({{"path", path}, {"reason", doc.diskConflict}});
    }
    return {{"revision", watch.revision},
            {"indexRevision", watch.indexRevision},
            {"pluginsRevision", plugins_.revision()},
            {"recovery", recoveryStatus()},
            {"reloaded", reloaded},
            {"conflicts", conflicts},
            {"watcher", {{"backend", watch.backend}, {"error", watch.error}}}};
}
} // namespace cycode

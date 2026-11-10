#include "cycode/Engine.h"
#include "cycode/NativeTrace.h"
#include "cycode/TextFile.h"
namespace cycode {
void Engine::checkpointRecovery() {
    NativeTraceScope trace(NativeTraceEvent::RecoveryScan);
    std::lock_guard serial(recoveryMutex_);
    Json errors = Json::array();
    std::vector<RecoveryStore::Snapshot> snapshots;
    std::set<std::string> dirty;
    try {
        size_t count = 0, bytes = 0;
        {
            std::lock_guard lock(mutex_);
            for (const auto *owner : {&documents_, &untitled_})
                for (const auto &[path, doc] : *owner)
                    if (doc.dirty) {
                        dirty.insert(path);
                        bytes += doc.text.size() + doc.disk.size();
                        if (++count > RecoveryStore::maxDocuments || bytes > RecoveryStore::maxBytes) {
                            if (errors.empty())
                                errors.push_back(
                                    {{"error", "Recovery quota exceeded (128 documents / 128 MiB)"}});
                            continue;
                        }
                        if (recovery_.savedRevision(path) != doc.revision)
                            snapshots.push_back({path, doc.text, doc.disk, doc.revision, doc.bom, doc.crlf,
                                                 doc.crOnly, doc.diskExisted});
                    }
        }
        recovery_.retain(dirty);
        for (const auto &snapshot : snapshots)
            try {
                recovery_.checkpoint(snapshot);
            } catch (const std::exception &e) {
                errors.push_back({{"path", snapshot.path}, {"error", e.what()}});
            }
    } catch (const std::exception &e) {
        errors.push_back({{"error", e.what()}});
    }
    recoveryErrors_ = std::move(errors);
}
void Engine::retireRecovery(const std::string &path) {
    NativeTraceScope trace(NativeTraceEvent::RecoveryRetire);
    std::lock_guard serial(recoveryMutex_);
    {
        std::lock_guard lock(mutex_);
        const auto &owner = isUntitledDocument(path) ? untitled_ : documents_;
        if (auto it = owner.find(path); it != owner.end() && it->second.dirty)
            return;
    }
    try {
        recovery_.erase(path);
    } catch (const std::exception &e) {
        recoveryErrors_.push_back({{"path", path}, {"error", e.what()}});
    }
}
Json Engine::recoveryStatus() {
    std::lock_guard serial(recoveryMutex_);
    auto result = recovery_.status();
    size_t pending = 0;
    {
        std::lock_guard lock(mutex_);
        for (const auto *owner : {&documents_, &untitled_})
            for (const auto &[path, doc] : *owner)
                if (doc.dirty && recovery_.savedRevision(path) != doc.revision)
                    ++pending;
    }
    result["pending"] = pending;
    result["errors"] = recoveryErrors_;
    result["intervalMs"] = 1000;
    return result;
}
Json Engine::recoveryOperation(const std::string &method, const Json &params) {
    if (method == "recovery.status")
        return recoveryStatus();
    if (method == "recovery.flush") {
        checkpointRecovery();
        return recoveryStatus();
    }
    if (method == "recovery.list") {
        auto result = recovery_.list(params.value("limit", size_t(256)));
        result["status"] = recoveryStatus();
        return result;
    }
    const std::string session = params.at("session"), id = params.at("id"),
                      checkpoint = params.at("checkpoint");
    if (method == "recovery.discard")
        return recovery_.discard(session, id, checkpoint);
    struct Target {
        std::string state;
        bool exists = false;
        std::optional<std::string> bytes;
    };
    auto target = [&](const std::string &path) {
        Target value;
        if (isUntitledDocument(path)) {
            value.state = "untitled";
            return value;
        }
        const auto full = resolve(path);
        try {
            value.exists = std::filesystem::exists(full);
            if (!value.exists)
                value.state = "missing";
            else {
                value.bytes = readTextFileBytes(full);
                value.state = RecoveryStore::fingerprint(*value.bytes);
            }
        } catch (const std::exception &) {
            value.state = "unreadable";
            value.exists = true;
        }
        return value;
    };
    if (method == "recovery.preview")
        return recovery_.inspect(session, id, checkpoint, [&](const RecoveryStore::Entry &entry) {
            const auto disk = target(entry.snapshot.path);
            auto result = entry.info;
            const auto limit = params.value("limitBytes", size_t(65536));
            if (limit < 1 || limit > 262144)
                throw std::runtime_error("Invalid recovery preview limit");
            const auto &text = entry.snapshot.text;
            auto end = std::min(limit, text.size());
            while (end > 0 && end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                --end;
            result["text"] = text.substr(0, end);
            result["truncated"] = end < text.size();
            result["diskState"] = disk.state;
            result["diskChanged"] =
                entry.snapshot.existed ? (!disk.bytes || *disk.bytes != entry.snapshot.disk) : disk.exists;
            result["missing"] = !disk.exists;
            std::lock_guard lock(mutex_);
            const auto key = documentKey(entry.snapshot.path);
            const auto &owner = isUntitledDocument(key) ? untitled_ : documents_;
            const auto it = owner.find(key);
            result["targetRevision"] = it == owner.end() ? uint64_t(0) : it->second.revision;
            result["targetDirty"] = it != owner.end() && it->second.dirty;
            return result;
        });
    if (method == "recovery.restore")
        return recovery_.inspect(
            session, id, checkpoint,
            [&](const RecoveryStore::Entry &entry) {
                Json result;
                std::string key;
                {
                    std::lock_guard lock(mutex_);
                    key = documentKey(entry.snapshot.path);
                    const bool draft = isUntitledDocument(key);
                    auto &owner = draft ? untitled_ : documents_;
                    const auto found = owner.find(key);
                    const auto revision = found == owner.end() ? uint64_t(0) : found->second.revision;
                    if (params.at("expectedRevision").get<uint64_t>() != revision)
                        throw std::runtime_error("RevisionConflict: recovery target changed");
                    if (found != owner.end() && found->second.dirty)
                        throw std::runtime_error(
                            "Save or close the modified target before restoring a checkpoint");
                    const auto disk = target(key);
                    if (disk.state != params.at("expectedDiskState").get<std::string>())
                        throw std::runtime_error("DiskConflict: recovery target changed since preview");
                    std::string previous = found == owner.end() ? std::string{} : found->second.text;
                    if (found == owner.end() && disk.bytes) {
                        try {
                            previous = *disk.bytes;
                            if (previous.starts_with("\xef\xbb\xbf"))
                                previous.erase(0, 3);
                            validateEditorText(previous);
                            previous = normalizeEditorText(previous);
                        } catch (const std::exception &) {
                            previous.clear();
                        }
                    }
                    Document doc;
                    doc.identity = found == owner.end() ? nextDocumentIdentity_++ : found->second.identity;
                    doc.text = entry.snapshot.text;
                    doc.disk = entry.snapshot.disk;
                    doc.bom = entry.snapshot.bom;
                    doc.crlf = entry.snapshot.crlf;
                    doc.crOnly = entry.snapshot.crOnly;
                    doc.diskExisted = entry.snapshot.existed && disk.exists;
                    doc.saved = doc.disk;
                    if (doc.bom && doc.saved.starts_with("\xef\xbb\xbf"))
                        doc.saved.erase(0, 3);
                    doc.saved = normalizeEditorText(doc.saved);
                    doc.dirty = true;
                    doc.revision = nextRevision_++;
                    const bool match = entry.snapshot.existed
                                           ? (disk.bytes && *disk.bytes == entry.snapshot.disk)
                                           : !disk.exists;
                    doc.diskConflict = match ? "" : disk.exists ? "changed" : "missing";
                    if (draft)
                        checkUntitledQuota(key, doc.text.size());
                    owner[key] = std::move(doc);
                    if (!draft)
                        language_.sync(key, owner.at(key).text, owner.at(key).revision, owner.at(key).identity);
                    documentEvent("document.restored", key, owner.at(key));
                    result = {{"document", documentJson(key, owner.at(key))},
                              {"sourceRetained", true},
                              {"warning", ""}};
                    if (params.value("includePreviousText", false))
                        result["previousText"] = previous;
                }
                // Transfer ownership only after the recovered buffer has its own durable checkpoint.
                checkpointRecovery();
                result["checkpointProtected"] =
                    recovery_.savedRevision(key) == result.at("document").at("revision").get<uint64_t>();
                if (!result.at("checkpointProtected").get<bool>())
                    result["warning"] = "Recovered in memory; the original checkpoint was retained because a "
                                        "new checkpoint could not be saved.";
                return result;
            },
            true);
    throw std::runtime_error("Unknown recovery operation");
}
} // namespace cycode

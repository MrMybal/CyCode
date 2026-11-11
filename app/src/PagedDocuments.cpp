#include "cycode/ByteOffset.h"
#include "cycode/Engine.h"
#include "cycode/Error.h"
#include "cycode/StreamingData.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <chrono>
#include <fstream>
namespace cycode {
std::shared_ptr<SpoolStore> Engine::workingStoreLocked() {
    if (!workingStore_)
        workingStore_ = std::make_shared<SpoolStore>(root_);
    return workingStore_;
}
Json Engine::pagedDocumentDescription(const std::string &path, const PagedDocument &doc) const {
    return {{"path", path},
            {"id", doc.identity},
            {"revision", doc.revision},
            {"dirty", doc.knownDirty},
            {"bytes", doc.text->bytes()},
            {"storage", "paged"},
            {"positionEncoding", "utf8-bytes"}};
}
Json Engine::pagedDocumentOperation(const std::string &method, const Json &params) {
    std::unique_lock lock(mutex_);
    const auto requested = params.at("path").get<std::string>();
    if (requested.find('\0') != std::string::npos)
        throw NativeError(ErrorCode::InvalidParams, "NUL in paged path");
    const auto full = resolve(requested);
    const auto path = pathUtf8(full.lexically_relative(root_));
    if (documents_.contains(path))
        throw NativeError(ErrorCode::UnsupportedOperation,
                          "Close the ordinary live buffer before choosing paged storage");
    if (pendingPagedDocuments_.contains(path))
        throw NativeError(ErrorCode::BackendUnavailable, "Paged source is being opened");
    auto found = pagedDocuments_.find(path);
    if (found == pagedDocuments_.end()) {
        if (method != "documents.readPage")
            throw NativeError(ErrorCode::InvalidParams, "Open paged text with documents.readPage first");
        if (params.contains("expectedRevision") || params.contains("expectedDocumentId"))
            throw NativeError(ErrorCode::RevisionConflict, "Paged document is not open in this process");
        pendingPagedDocuments_.insert(path);
        lock.unlock();
        std::shared_ptr<LargeText> text;
        try {
            text = std::make_shared<LargeText>(full, std::make_shared<SpoolStore>(root_));
        } catch (...) {
            lock.lock();
            pendingPagedDocuments_.erase(path);
            throw;
        }
        lock.lock();
        pendingPagedDocuments_.erase(path);
        found = pagedDocuments_.emplace(path, PagedDocument{text, nextDocumentIdentity_++, nextRevision_++})
                    .first;
        events_.publish("document.opened", pagedDocumentDescription(path, found->second));
    }
    auto &doc = found->second;
    if (doc.busy)
        throw NativeError(ErrorCode::BackendUnavailable, "Paged document has an active I/O operation");
    if ((params.contains("expectedRevision") && params.at("expectedRevision") != doc.revision) ||
        (params.contains("expectedDocumentId") && params.at("expectedDocumentId") != doc.identity))
        throw NativeError(ErrorCode::RevisionConflict, "Paged document revision or identity changed");
    if (method == "documents.readPage") {
        const auto offset = byteOffset(params, "offset");
        if (offset && !params.contains("expectedRevision"))
            throw NativeError(ErrorCode::InvalidParams, "Continuation pages require expectedRevision");
        auto result = pagedDocumentDescription(path, doc);
        const auto text = doc.text->read(offset, size_t(byteOffset(params, "length", 65536)));
        result["text"] = text;
        result["offset"] = offset;
        result["nextOffset"] = offset + text.size();
        result["hasMore"] = offset + text.size() < doc.text->bytes();
        return result;
    }
    if (!params.contains("expectedRevision"))
        throw NativeError(ErrorCode::InvalidParams, "Current expectedRevision is required");
    if (method == "documents.readLines") {
        const auto text = doc.text;
        doc.busy = true;
        lock.unlock();
        LargeText::Lines page;
        try {
            page = text->readLines(byteOffset(params, "line"), byteOffset(params, "character"),
                                   size_t(byteOffset(params, "length", 65536)));
        } catch (...) {
            lock.lock();
            doc.busy = false;
            throw;
        }
        lock.lock();
        doc.busy = false;
        auto result = pagedDocumentDescription(path, doc);
        result.update({{"text", page.text}, {"offset", page.offset}, {"nextOffset", page.nextOffset},
                       {"line", byteOffset(params, "line")}, {"character", byteOffset(params, "character")},
                       {"nextLine", page.nextLine}, {"nextCharacter", page.nextCharacter},
                       {"totalLines", page.totalLines}, {"linePositionEncoding", "utf16"},
                       {"hasMore", page.nextOffset < text->bytes()}});
        return result;
    }
    if (method == "documents.savePagedAs") {
        const auto destination = params.at("destination").get<std::string>();
        if (destination.find('\0') != std::string::npos)
            throw NativeError(ErrorCode::InvalidParams, "NUL in Save as destination");
        const auto targetFile = resolve(destination);
        const auto target = pathUtf8(targetFile.lexically_relative(root_));
        if (target == path || documents_.contains(target) || pagedDocuments_.contains(target) ||
            pendingPagedDocuments_.contains(target) || std::filesystem::exists(targetFile))
            throw NativeError(ErrorCode::DiskConflict, "Save as requires a new unoccupied workspace file");
        // Stage the new map node before publication, preserving identity but invalidating old plans.
        const auto inserted = pagedDocuments_.emplace(target, PagedDocument{doc.text, doc.identity, nextRevision_++, true});
        doc.busy = true;
        const auto text = doc.text;
        lock.unlock();
        try {
            text->saveAs(targetFile);
        } catch (...) {
            lock.lock();
            doc.busy = false;
            pagedDocuments_.erase(inserted.first);
            throw;
        }
        lock.lock();
        auto &saved = inserted.first->second;
        saved.busy = false;
        pagedDocuments_.erase(found);
        monitor_.invalidate();
        auto result = pagedDocumentDescription(target, saved);
        result["previousPath"] = path;
        events_.publish("document.moved", {{"path", target}, {"from", path}, {"to", target},
                                          {"id", saved.identity}, {"revision", saved.revision},
                                          {"dirty", false}, {"missing", false}, {"diskConflict", ""},
                                          {"transaction", ""}});
        events_.publish("document.saved", result);
        return result;
    }
    if (method == "documents.closePaged") {
        if (doc.text->dirty() && !params.value("discard", false))
            throw NativeError(ErrorCode::RevisionConflict,
                              "Explicit discard is required for unsaved paged edits");
        events_.publish("document.closed", pagedDocumentDescription(path, doc));
        pagedDocuments_.erase(found);
        ++nextRevision_;
        return {{"closed", true}};
    }
    if (method == "documents.savePaged") {
        doc.busy = true;
        const auto text = doc.text;
        lock.unlock();
        try {
            text->save();
        } catch (...) {
            lock.lock();
            doc.busy = false;
            doc.knownDirty = text->dirty();
            throw;
        }
        lock.lock();
        doc.busy = false;
        doc.knownDirty = text->dirty();
        events_.publish("document.saved", pagedDocumentDescription(path, doc));
        return pagedDocumentDescription(path, doc);
    }
    const auto offset = byteOffset(params, "offset"), length = byteOffset(params, "length");
    const auto inserted = params.at("text").get<std::string>();
    doc.busy = true;
    const auto text = doc.text;
    lock.unlock();
    LargeTextSnapshot after;
    try {
        if (!text->matchesDisk())
            throw NativeError(ErrorCode::DiskConflict, "Paged source changed outside CyCode");
        after = text->edited(offset, length, inserted);
    } catch (...) {
        lock.lock();
        doc.busy = false;
        throw;
    }
    lock.lock();
    doc.busy = false;
    ChangePlan plan;
    plan.title = "Edit paged text";
    FileChange file;
    file.path = path;
    file.revision = doc.revision;
    file.paged = doc.text;
    file.pagedBefore = doc.text->snapshot();
    file.pagedAfter = std::move(after);
    file.edits = Json::array({{{"byteOffset", offset}, {"byteLength", length}, {"text", inserted}}});
    plan.files.push_back(std::move(file));
    return storeChange(std::move(plan));
}
namespace {
bool checkpointName(const std::string &id) {
    return id.size() == 37 && id.starts_with("core-") &&
           std::all_of(id.begin()+5, id.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
std::filesystem::path checkpointDirectory(const std::filesystem::path &root, const std::string &id) {
    if (!checkpointName(id))
        throw NativeError(ErrorCode::InvalidParams, "Invalid paged checkpoint reference");
    const auto dir = root/".cycode"/"working-data"/id;
    if (std::filesystem::weakly_canonical(dir) != dir || !std::filesystem::is_directory(dir))
        throw NativeError(ErrorCode::FileSystemError, "Missing or redirected paged checkpoint");
    return dir;
}
Json checkpointMetadata(const std::filesystem::path &root, const std::string &id, bool verify) {
    const auto dir = checkpointDirectory(root, id);
    const auto manifest = dir/"paged-checkpoint.json";
    if (std::filesystem::is_symlink(manifest) || std::filesystem::weakly_canonical(manifest) != manifest ||
        !std::filesystem::is_regular_file(manifest) || std::filesystem::file_size(manifest) > 65536)
        throw NativeError(ErrorCode::FileSystemError, "Missing or oversized paged checkpoint manifest");
    const auto result = Json::parse(readTextFileBytes(manifest), [](int depth, Json::parse_event_t, Json &) {
        if (depth > 8)
            throw NativeError(ErrorCode::InvalidParams, "Paged checkpoint metadata is too deeply nested");
        return true;
    });
    if (result.value("format", "") != "cycode-paged-recovery/1" || result.at("checkpoint") != id ||
        result.at("content") != "blob-1.bin" || !result.at("path").is_string() ||
        !result.at("bytes").is_number_unsigned() || !result.at("digest").is_string() ||
        !result.at("baselineDigest").is_string())
        throw NativeError(ErrorCode::InvalidParams, "Malformed paged checkpoint manifest");
    const auto validDigest = [](const Json &value) {
        if (!value.is_string()) return false;
        const auto &hash = value.get_ref<const std::string &>();
        return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    };
    const auto path = result.at("path").get<std::string>();
    if (path.empty() || path.size() > 8192 || path.find('\0') != std::string::npos ||
        !validDigest(result.at("digest")) || !validDigest(result.at("baselineDigest")) ||
        !result.at("historicalRevision").is_number_unsigned() || result.at("historicalRevision") == 0 ||
        !result.at("updatedAt").is_number_unsigned())
        throw NativeError(ErrorCode::InvalidParams, "Invalid paged checkpoint metadata values");
    const auto content = dir/"blob-1.bin";
    if (std::filesystem::weakly_canonical(content) != content || !std::filesystem::is_regular_file(content) ||
        std::filesystem::file_size(content) != result.at("bytes").get<uint64_t>())
        throw NativeError(ErrorCode::RevisionConflict, "Paged recovery payload is missing or changed");
    if (verify && digestFileStreaming(content) != result.at("digest").get<std::string>())
        throw NativeError(ErrorCode::RevisionConflict, "Paged recovery digest mismatch");
    return result;
}
Json publicCheckpoint(const Json &meta) {
    return {{"checkpoint", meta.at("checkpoint")}, {"path", meta.at("path")},
            {"bytes", meta.at("bytes")}, {"updatedAt", meta.at("updatedAt")},
            {"historicalRevision", meta.at("historicalRevision")}};
}
} // namespace
Json Engine::pagedRecoveryOperation(const std::string &method, const Json &params) {
    if (method == "documents.pagedCheckpoints") {
        Json rows = Json::array();
        size_t scanned = 0, invalid = 0;
        bool truncated = false;
        const auto parent = root_/".cycode"/"working-data";
        if (std::filesystem::exists(parent)) {
            if (std::filesystem::weakly_canonical(parent) != parent)
                throw NativeError(ErrorCode::FileSystemError, "Redirected paged recovery store");
            for (const auto &entry : std::filesystem::directory_iterator(parent)) {
                if (scanned == 4096 || rows.size() >= 256) {
                    truncated = true;
                    break;
                }
                ++scanned;
                const auto id = pathUtf8(entry.path().filename());
                if (!checkpointName(id) || !std::filesystem::exists(entry.path()/"paged-checkpoint.json"))
                    continue;
                try {
                    auto row = publicCheckpoint(checkpointMetadata(root_, id, false));
                    resolve(row.at("path"));
                    rows.push_back(std::move(row));
                } catch (const std::exception &) { ++invalid; }
            }
        }
        return {{"checkpoints", rows}, {"scanned", scanned}, {"invalid", invalid}, {"truncated", truncated},
                {"payloadDigestsVerified", false}};
    }
    if (method == "documents.previewPagedCheckpoint") {
        const auto id = params.at("checkpoint").get<std::string>();
        const auto meta = checkpointMetadata(root_, id, true);
        const auto full = resolve(meta.at("path"));
        const auto content = checkpointDirectory(root_, id)/"blob-1.bin";
        const auto bytes = meta.at("bytes").get<uint64_t>(), offset = byteOffset(params, "offset");
        const auto length = byteOffset(params, "length", 65536);
        if (length < 4 || length > 262144 || offset > bytes)
            throw NativeError(ErrorCode::InvalidParams, "Use a valid 4..262144 byte recovery page");
        std::ifstream input(content, std::ios::binary);
        input.seekg(std::streamoff(offset));
        std::string text(size_t(std::min<uint64_t>(length, bytes-offset)), '\0');
        input.read(text.data(), std::streamsize(text.size()));
        if ((!input && !text.empty()) || (!text.empty() && (static_cast<unsigned char>(text[0])&0xc0)==0x80))
            throw NativeError(ErrorCode::InvalidParams, "Use a UTF-8 boundary in the recovery payload");
        if (offset+text.size() < bytes) {
            const auto next = input.peek();
            if (next != EOF && (static_cast<unsigned char>(next)&0xc0)==0x80) {
                while (!text.empty() && (static_cast<unsigned char>(text.back())&0xc0)==0x80)
                    text.pop_back();
                if (!text.empty()) text.pop_back();
            }
        }
        auto result = publicCheckpoint(meta);
        result.update({{"offset", offset}, {"text", text}, {"nextOffset", offset+text.size()},
                       {"hasMore", offset+text.size()<bytes}, {"payloadDigestsVerified", true}});
        bool matches = false;
        try { matches = digestFileStreaming(full) == meta.at("baselineDigest").get<std::string>(); } catch (...) {}
        result["diskMatchesBaseline"] = matches;
        return result;
    }
    std::unique_lock lock(mutex_);
    const auto path = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
    auto found = pagedDocuments_.find(path);
    if (found == pagedDocuments_.end() || params.at("expectedRevision") != found->second.revision)
        throw NativeError(ErrorCode::RevisionConflict, "Open paged document with its current revision first");
    auto &doc = found->second;
    if (doc.busy)
        throw NativeError(ErrorCode::BackendUnavailable, "Paged document has active I/O");
    const auto text = doc.text;
    const auto revision = doc.revision;
    doc.busy = true;
    lock.unlock();
    Json result;
    LargeTextSnapshot restored;
    try {
        if (!text->matchesDisk())
            throw NativeError(ErrorCode::DiskConflict, "Reconcile the paged source disk conflict first");
        if (method == "documents.checkpointPaged") {
            auto store = std::make_shared<SpoolStore>(root_);
            auto blob = store->putStream(text->bytes(), [&](const AtomicByteSink &sink) { text->stream(sink); });
            const auto id = pathUtf8(store->directory().filename());
            Json meta = {{"format", "cycode-paged-recovery/1"}, {"checkpoint", id}, {"path", path},
                         {"bytes", blob->bytes}, {"content", "blob-1.bin"}, {"digest", blob->digest},
                         {"baselineDigest", text->baselineDigest()}, {"historicalRevision", revision},
                         {"updatedAt", std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count()}};
            AtomicFileOptions options;
            options.access = AtomicAccess::PrivateUser;
            writeFileAtomic(store->directory()/"paged-checkpoint.json", meta.dump(), options);
            store->preserve();
            result = publicCheckpoint(meta);
        } else {
            const auto id = params.at("checkpoint").get<std::string>();
            const auto meta = checkpointMetadata(root_, id, true);
            if (meta.at("path") != path || meta.at("baselineDigest") != text->baselineDigest())
                throw NativeError(ErrorCode::DiskConflict, "Checkpoint path or original disk baseline differs");
            restored = text->recovered(checkpointDirectory(root_, id)/"blob-1.bin", meta.at("digest"));
            if (restored->bytes != meta.at("bytes"))
                throw NativeError(ErrorCode::RevisionConflict, "Checkpoint changed during restoration");
        }
    } catch (...) {
        lock.lock();
        doc.busy = false;
        throw;
    }
    lock.lock();
    doc.busy = false;
    if (method == "documents.checkpointPaged")
        return result;
    ChangePlan plan;
    plan.title = "Restore paged recovery checkpoint";
    FileChange file;
    file.path = path;
    file.revision = revision;
    file.paged = text;
    file.pagedBefore = text->snapshot();
    file.pagedAfter = std::move(restored);
    file.edits = Json::array();
    plan.files.push_back(std::move(file));
    return storeChange(std::move(plan));
}
} // namespace cycode

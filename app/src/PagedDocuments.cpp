#include "cycode/ByteOffset.h"
#include "cycode/Engine.h"
#include "cycode/Error.h"
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
} // namespace cycode

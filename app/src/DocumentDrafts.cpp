#include "cycode/Engine.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <cwchar>
namespace cycode {
std::string Engine::documentKey(const std::string &path) const {
    if (path.starts_with("untitled:")) {
        if (!isUntitledDocument(path))
            throw std::runtime_error("Invalid untitled document reference");
        return path;
    }
    return pathUtf8(resolve(path).lexically_relative(root_));
}
void Engine::checkUntitledQuota(const std::string &key, size_t bytes) const {
    if (!untitled_.contains(key) && untitled_.size() >= 64)
        throw std::runtime_error("Untitled document quota exceeded (64 documents)");
    for (const auto &[path, doc] : untitled_)
        if (path != key)
            bytes += doc.text.size();
    if (bytes > 64 * 1024 * 1024)
        throw std::runtime_error("Untitled document quota exceeded (64 MiB)");
}
Json Engine::createUntitled(const Json &params) {
    auto text = params.value("text", std::string{});
    validateEditorText(text);
    text = normalizeEditorText(text);
    std::lock_guard lock(mutex_);
    const auto name = params.value("name", "Untitled-" + std::to_string(nextDocumentIdentity_) + ".txt");
    const auto key = newUntitledReference(name);
    checkUntitledQuota(key, text.size());
    Document doc;
    doc.identity = nextDocumentIdentity_++;
    doc.revision = nextRevision_++;
    doc.text = std::move(text);
    doc.dirty = true; // An empty draft must still be checkpointed and explicitly saved/discarded.
    doc.diskExisted = false;
    const auto &opened = untitled_.emplace(key, std::move(doc)).first->second;
    documentEvent("document.opened", key, opened);
    return documentJson(key, opened);
}
Json Engine::saveDocumentAs(const Json &params) {
    std::unique_lock lock(mutex_);
    const auto key = documentKey(params.at("path"));
    const auto full = resolve(params.at("destination"));
    const auto target = pathUtf8(full.lexically_relative(root_));
    if (target == key || documents_.contains(target) || std::filesystem::exists(full))
        throw std::runtime_error(
            "Choose a new workspace file; Save as never replaces an existing file or buffer");
#ifdef _WIN32
    const auto destinationKey = utf8Path(target).native();
    for (const auto &[path, current] : documents_)
        if (_wcsicmp(utf8Path(path).c_str(), destinationKey.c_str()) == 0)
            throw std::runtime_error("An open buffer already occupies this Windows path");
#endif
    auto &doc = openLocked(key);
    if (params.at("expectedRevision") != doc.revision)
        throw std::runtime_error("RevisionConflict: document changed before Save as");
    const auto bytes = encodeEditorText(doc.text, doc.bom, doc.crlf, doc.crOnly);
    Document saved = doc;
    saved.revision = nextRevision_++;
    saved.saved = saved.text;
    saved.disk = bytes;
    saved.diskExisted = true;
    saved.dirty = false;
    saved.diskConflict.clear();
    // Allocate the new model before touching disk. The engine lock keeps it private.
    auto insertion = documents_.emplace(target, std::move(saved));
    try {
        if (!std::filesystem::is_directory(full.parent_path()))
            throw std::runtime_error("Save as requires an existing destination folder");
        writeTextFileAtomic(full, bytes, std::nullopt);
    } catch (...) {
        documents_.erase(insertion.first);
        throw;
    }
    Json warnings = Json::array();
    try {
        if (!isUntitledDocument(key))
            language_.close(key);
    } catch (const std::exception &e) {
        warnings.push_back(e.what());
    }
    if (isUntitledDocument(key))
        untitled_.erase(key);
    else
        documents_.erase(key);
    auto &current = insertion.first->second;
    events_.publish("document.moved", {{"path", target},
                                       {"from", key},
                                       {"to", target},
                                       {"id", current.identity},
                                       {"revision", current.revision},
                                       {"dirty", false},
                                       {"missing", false},
                                       {"diskConflict", ""},
                                       {"transaction", ""}});
    documentEvent("document.saved", target, current);
    try {
        language_.sync(target, current.text, current.revision, current.identity);
    } catch (const std::exception &e) {
        warnings.push_back(e.what());
    }
    auto result = documentJson(target, current);
    result["previousPath"] = key;
    result["warnings"] = warnings;
    monitor_.invalidate();
    lock.unlock();
    retireRecovery(key);
    return result;
}
} // namespace cycode

#include "cycode/Engine.h"
#include "cycode/ByteOffset.h"
#include "cycode/Error.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <set>

namespace cycode {
bool Engine::retainedChange(const std::string &id) const {
    return std::find(changeUndo_.begin(), changeUndo_.end(), id) != changeUndo_.end() ||
           std::find(changeRedo_.begin(), changeRedo_.end(), id) != changeRedo_.end();
}

void Engine::pruneChangeHistory() {
    auto invalid = [&](const std::string &id) {
        const auto found = changes_.find(id);
        if (found == changes_.end()) return true;
        for (const auto &file : found->second.files) {
            if (file.paged) {
                const auto doc = pagedDocuments_.find(file.path);
                if (doc == pagedDocuments_.end() || doc->second.identity != file.identity ||
                    doc->second.text != file.paged) return true;
            } else {
                const auto doc = documents_.find(file.path);
                if (doc == documents_.end() || doc->second.identity != file.identity) return true;
            }
        }
        return false;
    };
    const auto before = changeUndo_.size() + changeRedo_.size();
    std::erase_if(changeUndo_, invalid);
    std::erase_if(changeRedo_, invalid);
    const auto dropped = before - changeUndo_.size() - changeRedo_.size();
    if (dropped) {
        changeHistoryDropped_ += dropped;
        ++changeHistoryRevision_;
    }
}

Json Engine::changeHistoryOperation(const std::string &method, const Json &params) {
    std::lock_guard lock(mutex_);
    pruneChangeHistory();
    if (method == "changes.historyStatus") {
        auto head = [&](const auto &stack) -> Json {
            if (stack.empty()) return nullptr;
            const auto &id = stack.back();
            const auto &plan = changes_.at(id);
            Json paths = Json::array();
            for (const auto &file : plan.files) paths.push_back(file.path);
            return {{"id", id}, {"title", plan.title}, {"files", std::move(paths)}};
        };
        return {{"revision", changeHistoryRevision_}, {"undoCount", changeUndo_.size()},
                {"redoCount", changeRedo_.size()}, {"undo", head(changeUndo_)},
                {"redo", head(changeRedo_)}, {"retentionLimit", changeHistoryLimit_},
                {"dropped", changeHistoryDropped_}};
    }
    const auto direction = params.at("direction").get<std::string>();
    if (direction != "undo" && direction != "redo")
        throw NativeError(ErrorCode::InvalidParams, "History direction must be undo or redo");
    const bool undo = direction == "undo";
    const auto id = params.at("id").get<std::string>();
    const auto &stack = undo ? changeUndo_ : changeRedo_;
    if (byteOffset(params, "expectedHistoryRevision") != changeHistoryRevision_ ||
        stack.empty() || stack.back() != id)
        throw NativeError(ErrorCode::RevisionConflict, "Native transaction history changed; refresh it");
    const auto &source = changes_.at(id);
    if (source.state != (undo ? "applied" : "undone"))
        throw NativeError(ErrorCode::RevisionConflict, "History transaction is no longer in the expected state");
    const auto &guards = params.at("documents");
    if (!guards.is_array() || guards.size() != source.files.size())
        throw NativeError(ErrorCode::InvalidParams, "History requires one live guard for every affected document");
    std::map<std::string, const Json *> indexed;
    for (const auto &guard : guards) {
        const auto path = guard.at("path").get<std::string>();
        if (!indexed.emplace(path, &guard).second)
            throw NativeError(ErrorCode::InvalidParams, "Duplicate history document guard");
    }
    ChangePlan plan;
    plan.title = (undo ? "Undo " : "Redo ") + source.title;
    plan.historySource = id;
    plan.historyDirection = direction;
    plan.historyRevision = changeHistoryRevision_;
    if (!undo) {
        plan.annotations = source.annotations;
        size_t annotation = 0;
        for (auto &row : plan.annotations) row["id"] = "history-" + std::to_string(annotation++);
    }
    for (const auto &original : source.files) {
        const auto guard = indexed.find(original.path);
        if (guard == indexed.end())
            throw NativeError(ErrorCode::InvalidParams, "History document guard does not match the transaction");
        const auto identity = byteOffset(*guard->second, "expectedDocumentId");
        const auto revision = byteOffset(*guard->second, "expectedRevision");
        if (!identity || !revision || identity != original.identity)
            throw NativeError(ErrorCode::RevisionConflict, "History document identity changed");
        FileChange file;
        file.path = original.path;
        file.identity = identity;
        file.revision = revision;
        if (original.paged) {
            const auto doc = pagedDocuments_.find(file.path);
            if (doc == pagedDocuments_.end() || doc->second.busy || doc->second.identity != identity ||
                doc->second.revision != revision || doc->second.text != original.paged ||
                doc->second.text->snapshot() != (undo ? original.pagedAfter : original.pagedBefore))
                throw NativeError(ErrorCode::RevisionConflict, "Paged history source changed: " + file.path);
            if (!original.paged->matchesDisk())
                throw NativeError(ErrorCode::DiskConflict, "Paged history source changed on disk");
            file.paged = original.paged;
            file.pagedBefore = undo ? original.pagedAfter : original.pagedBefore;
            file.pagedAfter = undo ? original.pagedBefore : original.pagedAfter;
            file.paged->validate(file.pagedAfter);
        } else {
            const auto doc = documents_.find(file.path);
            const auto before = undo ? original.stored->after : original.stored->before;
            if (doc == documents_.end() || doc->second.identity != identity ||
                doc->second.revision != revision || doc->second.text != before->load())
                throw NativeError(ErrorCode::RevisionConflict, "History source changed: " + file.path);
            if (readTextFileBytes(resolve(file.path)) != doc->second.disk)
                throw NativeError(ErrorCode::DiskConflict, "History source changed on disk: " + file.path);
            auto stored = std::make_shared<FileChange::Stored>();
            stored->before = before;
            stored->after = undo ? original.stored->before : original.stored->after;
            stored->disk = original.stored->disk;
            stored->after->validate();
            file.stored = std::move(stored);
        }
        // A snapshot restoration has no synthetic textual edit list. The actual
        // before/after payloads remain available through the normal paged preview.
        file.edits = Json::array();
        plan.files.push_back(std::move(file));
    }
    return storeChange(std::move(plan));
}
} // namespace cycode

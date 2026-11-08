#include "cycode/ProcessText.h"
#include "cycode/GitMerge.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/GitConflicts.h"
#include "cycode/Recovery.h"
#include "cycode/TextEdits.h"
#include <algorithm>
#include <fstream>
#include <set>
namespace cycode {
namespace {
namespace fs = std::filesystem;
std::string trimmed(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    return text;
}
void ordinary(const fs::path &path) {
    const auto absolute = fs::absolute(path).lexically_normal(), canonical = fs::weakly_canonical(path);
#ifdef _WIN32
    const bool same = _wcsicmp(absolute.c_str(), canonical.c_str()) == 0;
#else
    const bool same = absolute == canonical;
#endif
    if (!same || fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Redirected Git merge metadata is unsupported");
}
Json metadata(const fs::path &file, size_t limit) {
    ordinary(file);
    if (!fs::exists(file))
        return nullptr;
    if (!fs::is_regular_file(file) || fs::file_size(file) > limit)
        throw std::runtime_error("Git merge metadata exceeds its size/type limit");
    const auto size = fs::file_size(file);
    if (size > limit)
        throw std::runtime_error("Git merge metadata grew beyond its limit");
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read Git merge metadata");
    std::string raw(static_cast<size_t>(size) + 1, '\0');
    input.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    if (input.bad() || input.gcount() != static_cast<std::streamsize>(size))
        throw std::runtime_error("Git merge metadata changed while reading");
    raw.resize(static_cast<size_t>(size));
    // Private binary records can contain NUL; JSON never needs their original bytes.
    return {{"bytes", raw.size()}, {"guard", RecoveryStore::fingerprint(raw)}};
}
size_t metadataLimit(const std::string &name) {
    if (name == "HEAD" || name == "MERGE_HEAD")
        return 4096;
    if (name == "MERGE_RR")
        return 65536;
    if (name == "COMMIT_EDITMSG")
        return 32768; // Git may append a newline to an accepted 16 KiB message.
    return 16384;
}
std::string textMetadata(const fs::path &path, size_t limit) {
    const auto before = metadata(path, limit);
    if (before.is_null())
        return {};
    const auto size = before.at("bytes").get<size_t>();
    std::ifstream input(path, std::ios::binary);
    std::string text(size + 1, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (input.bad() || input.gcount() != static_cast<std::streamsize>(size))
        throw std::runtime_error("Git merge text changed while reading");
    text.resize(size);
    if (RecoveryStore::fingerprint(text) != before.at("guard").get<std::string>())
        throw std::runtime_error("Git merge text changed during inspection");
    if (text.find('\0') != std::string::npos)
        throw std::runtime_error("Git merge text contains binary data");
    (void)Json(text).dump();
    return text;
}
} // namespace
Json Engine::gitMerge(const std::string &method, const Json &params) {
    if (method != "vcs.merge.state" && method != "vcs.merge.plan" && method != "vcs.merge.commit" &&
        method != "vcs.merge.quit")
        throw std::runtime_error("Unknown Git merge workflow operation");
    const auto selected = gitSelection(params);
    if (!selected.marker)
        throw std::runtime_error("Select an actual Git repository root to manage a merge");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    std::string disabledHooks;
    auto git = [&](std::vector<std::string> args, int seconds = 15, bool hooks = false,
                   bool mutation = false) {
        if (!mutation && std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Git merge inspection exceeded its 45-second budget");
        args.insert(args.begin(),
                    {"git", "--no-pager", "--literal-pathspecs", "-C", pathUtf8(selected.directory), "-c",
                     "core.fsmonitor=false", "-c", "rerere.enabled=false", "-c", "rerere.autoupdate=false",
                     "-c", "maintenance.auto=false", "-c", "gc.auto=0"});
        if (!hooks && !disabledHooks.empty())
            args.insert(args.begin() + 5, {"-c", "core.hooksPath=" + disabledHooks});
        return runProcess(args, selected.directory, std::chrono::seconds(seconds), 8 * 1024 * 1024, nullptr,
                          {{"GIT_OPTIONAL_LOCKS", "0"},
                           {"GIT_TERMINAL_PROMPT", "0"},
                           {"GIT_NO_LAZY_FETCH", "1"},
                           {"GIT_NO_REPLACE_OBJECTS", "1"},
                           {"GIT_ALLOW_PROTOCOL", ":"},
                           {"GIT_EDITOR", ":"}});
    };
    auto checked = [&](std::vector<std::string> args) {
        const auto result = git(std::move(args));
        if (result.exitCode || result.timedOut || result.truncated)
            throw std::runtime_error("Git merge inspection failed: " + result.output);
        return result.output;
    };
    auto ref = [&](const std::string &name) {
        const auto result = git({"rev-parse", "--verify", "--quiet", name});
        if (result.timedOut || result.truncated || (result.exitCode != 0 && result.exitCode != 1))
            throw std::runtime_error("Cannot inspect Git merge reference");
        return result.exitCode ? std::string{} : trimmed(result.output);
    };
    const auto admin =
        fs::absolute(utf8Path(trimmed(checked({"rev-parse", "--absolute-git-dir"})))).lexically_normal();
    ordinary(admin);
    const auto adminIdentity = directoryIdentity(admin);
    disabledHooks = pathUtf8(admin / "HEAD");
    if (metadata(admin / "HEAD", 4096).is_null())
        throw std::runtime_error("Git HEAD metadata is unavailable");
    Json observedIndex;
    std::string observedFlags;
    auto snapshot = [&] {
        Json meta = Json::object();
        for (const auto *name :
             {"HEAD", "ORIG_HEAD", "MERGE_HEAD", "MERGE_MSG", "MERGE_MODE", "MERGE_AUTOSTASH", "MERGE_RR",
              "COMMIT_EDITMSG", "CHERRY_PICK_HEAD", "REVERT_HEAD"})
            meta[name] = metadata(admin / name, metadataLimit(name));
        Json operations = Json::array();
        for (const auto *name : {"rebase-merge", "rebase-apply", "sequencer"}) {
            ordinary(admin / name);
            if (fs::exists(admin / name)) {
                if (!fs::is_directory(admin / name))
                    throw std::runtime_error("Invalid Git operation directory");
                operations.push_back(name);
            }
        }
        if (!meta.at("CHERRY_PICK_HEAD").is_null())
            operations.push_back("cherry-pick");
        if (!meta.at("REVERT_HEAD").is_null())
            operations.push_back("revert");
        const auto head = ref("HEAD"), origin = ref("ORIG_HEAD"), autoMerge = ref("AUTO_MERGE");
        const auto branch =
            head.empty() ? std::string{} : trimmed(checked({"rev-parse", "--symbolic-full-name", "HEAD"}));
        const auto raw = checked({"ls-files", "--stage", "-z"}), flags = checked({"ls-files", "-v", "-z"});
        const auto index = parseGitConflictIndex(raw);
        Json conflicts = Json::array();
        for (const auto &[path, stages] : index.items())
            if (!stages.contains("0")) {
                if (conflicts.size() >= 2048)
                    throw std::runtime_error("Merge workflow supports at most 2048 conflict paths");
                conflicts.push_back(path);
            }
        bool hidden = false;
        size_t at = 0;
        while (at < flags.size()) {
            const auto end = flags.find('\0', at);
            if (end == std::string::npos || end < at + 3 || flags[at + 1] != ' ')
                throw std::runtime_error("Incomplete Git index flag record");
            if (flags[at] != 'H' && flags[at] != 'M')
                hidden = true;
            at = end + 1;
        }
        const auto dirty =
            git({"diff", "--quiet", "--no-ext-diff", "--no-textconv", "--ignore-submodules=none", "--"});
        if (dirty.timedOut || dirty.truncated || (dirty.exitCode != 0 && dirty.exitCode != 1))
            throw std::runtime_error("Cannot inspect unstaged merge files");
        const auto visibleStaged =
            head.empty()
                ? std::string{}
                : checked({"diff", "--cached", "--name-status", "-z", "--no-renames", "--no-ext-diff",
                           "--no-textconv", "--ita-visible-in-index", "HEAD", "--"});
        const auto invisibleStaged =
            head.empty()
                ? std::string{}
                : checked({"diff", "--cached", "--name-status", "-z", "--no-renames", "--no-ext-diff",
                           "--no-textconv", "--ita-invisible-in-index", "HEAD", "--"});
        const auto intentGuard = visibleStaged == invisibleStaged
                                     ? std::string{}
                                     : RecoveryStore::fingerprint(visibleStaged + invisibleStaged);
        hidden = hidden || !intentGuard.empty();
        const auto staged = parseGitMergeChanges(visibleStaged);
        const auto summary = head.empty() ? std::string{}
                                          : checked({"diff", "--cached", "--stat", "--no-renames",
                                                     "--no-ext-diff", "--no-textconv", "HEAD", "--"});
        if (summary.size() > 1024 * 1024)
            throw std::runtime_error("Staged merge summary exceeds 1 MiB");
        const auto heads = parseGitMergeHeads(textMetadata(admin / "MERGE_HEAD", 4096));
        const auto message = textMetadata(admin / "MERGE_MSG", 16384);
        Json documents = Json::array();
        size_t dirtyBuffers = 0, metadataDocuments = 0, openDocuments = 0;
        {
            std::lock_guard lock(mutex_);
            for (const auto &[path, doc] : documents_)
                if (!isUntitledDocument(path)) {
                    const auto file = resolve(path);
                    const bool metadataFile = containsPath(admin, file) ||
                                              resourceContains(".cycode/merge-receipts", path),
                               source = containsPath(selected.directory, file);
                    if (source || metadataFile) {
                        documents.push_back({{"path", path},
                                             {"id", doc.identity},
                                             {"revision", doc.revision},
                                             {"dirty", doc.dirty},
                                             {"diskConflict", doc.diskConflict}});
                        if (metadataFile)
                            ++metadataDocuments;
                        else {
                            ++openDocuments;
                            if (doc.dirty || !doc.diskConflict.empty())
                                ++dirtyBuffers;
                        }
                    }
                }
        }
        const bool active = !meta.at("MERGE_HEAD").is_null();
        const bool autostash = !meta.at("MERGE_AUTOSTASH").is_null();
        Json commitBlockers = Json::array(), quitBlockers = Json::array();
        auto both = [&](const char *code) {
            commitBlockers.push_back(code);
            quitBlockers.push_back(code);
        };
        if (!active)
            both("notMerge");
        if (!operations.empty())
            both("otherOperation");
        if (autostash)
            both("autostash");
        if (metadataDocuments)
            both("metadataOpen");
        if (active && (head.empty() || (!origin.empty() && origin != head)))
            both("headChanged");
        if (!conflicts.empty())
            commitBlockers.push_back("unmerged");
        if (dirty.exitCode)
            commitBlockers.push_back("workingChanges");
        if (dirtyBuffers)
            commitBlockers.push_back("dirtyBuffers");
        if (hidden)
            commitBlockers.push_back("hiddenIndexFlags");
        if (active && heads.size() != 1)
            commitBlockers.push_back("multipleParents");
        const auto metadataGuard = RecoveryStore::fingerprint(meta.dump() + operations.dump() + autoMerge);
        if (raw != checked({"ls-files", "--stage", "-z"}) || flags != checked({"ls-files", "-v", "-z"}) ||
            head != ref("HEAD") || origin != ref("ORIG_HEAD") || directoryIdentity(admin) != adminIdentity ||
            gitSelection(params).identity != selected.identity)
            throw std::runtime_error("Git merge state changed during inspection; refresh");
        for (const auto &[name, value] : meta.items())
            if (metadata(admin / utf8Path(name), metadataLimit(name)) != value)
                throw std::runtime_error("Git merge metadata changed during inspection");
        observedIndex = index;
        observedFlags = flags + intentGuard;
        return Json{
            {"repository", selected.root},
            {"identity", selected.identity},
            {"revision", RecoveryStore::fingerprint(selected.identity + adminIdentity + head + branch +
                                                    metadataGuard + raw + flags + intentGuard +
                                                    documents.dump() + std::to_string(dirty.exitCode))},
            {"kind", active               ? "merge"
                     : operations.empty() ? "none"
                                          : "other"},
            {"head", head},
            {"branch", branch},
            {"mergeHeads", heads},
            {"message", message},
            {"autostash", autostash},
            {"otherOperations", operations},
            {"conflicts", conflicts},
            {"stagedPaths", staged},
            {"stagedSummary", summary},
            {"workingDirty", dirty.exitCode == 1},
            {"hiddenIndexFlags", hidden},
            {"dirtyBuffers", dirtyBuffers},
            {"metadataDocuments", metadataDocuments},
            {"openDocuments", openDocuments},
            {"canCommit", commitBlockers.empty()},
            {"canQuit", quitBlockers.empty()},
            {"commitBlockers", commitBlockers},
            {"quitBlockers", quitBlockers}};
    };
    const auto before = snapshot();
    const auto beforeIndex = observedIndex;
    const auto beforeFlags = observedFlags;
    if (method == "vcs.merge.state")
        return before;
    if (!params.contains("expectedRepository") || params.at("expectedRepository") != selected.identity ||
        params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Merge repository/state changed; refresh before reviewing");
    auto request = gitMergeRequest(params);
    if (method != "vcs.merge.plan" &&
        request.at("action") != (method == "vcs.merge.commit" ? "commit" : "quit"))
        throw std::runtime_error("Merge action does not match the issued operation");
    const bool commit = request.at("action") == "commit", hooks = request.at("runHooks");
    if (!before.at(commit ? "canCommit" : "canQuit").get<bool>())
        throw std::runtime_error("Merge action is blocked; inspect current blockers");
    if (hooks && before.at("openDocuments").get<size_t>())
        throw std::runtime_error("Close repository documents before running configured commit hooks");
    Json parents = Json::array();
    if (commit)
        parents = Json::array({before.at("head"), before.at("mergeHeads")[0]});
    Json plan = {{"repository", selected.root},
                 {"identity", selected.identity},
                 {"stateRevision", before.at("revision")},
                 {"request", request},
                 {"expectedParents", parents},
                 {"stagedPaths", before.at("stagedPaths")},
                 {"stagedSummary", before.at("stagedSummary")},
                 {"preservesWorkingFiles", !hooks},
                 {"preservesIndex", !commit},
                 {"createsCommit", commit},
                 {"mayExecuteHooks", hooks},
                 {"signsCommit", false},
                 {"mayContactRemote", hooks},
                 {"argumentsTemplate", commit ? Json::array({"commit", "--file", "<reviewed-message>",
                                                             "--cleanup=verbatim", "--no-gpg-sign"})
                                              : Json::array({"merge", "--quit"})}};
    plan["revision"] = RecoveryStore::fingerprint(plan.dump());
    if (method == "vcs.merge.plan")
        return plan;
    if (params.at("expectedPlanRevision") != plan.at("revision"))
        throw std::runtime_error("Merge preview changed; review it again");
    const auto folder = root_ / ".cycode/merge-receipts";
    ordinary(root_ / ".cycode");
    ordinary(folder);
    fs::create_directory(root_ / ".cycode");
    fs::create_directory(folder);
    ordinary(folder);
    const auto id =
        "merge-" +
        RecoveryStore::fingerprint(
            plan.dump() + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto receipt = folder / utf8Path(id + ".json"), messageFile = folder / utf8Path(id + ".txt");
    const auto receiptPath = pathUtf8(receipt.lexically_relative(root_));
    const Json journal = {{"schemaVersion", 1},
                          {"repository", selected.root},
                          {"identity", selected.identity},
                          {"action", request.at("action")},
                          {"head", before.at("head")},
                          {"mergeHeads", before.at("mergeHeads")},
                          {"originalMessage", before.at("message")},
                          {"request", request},
                          {"stateRevision", before.at("revision")},
                          {"planRevision", plan.at("revision")}};
    {
        std::lock_guard files(resourcesMutex_);
        std::lock_guard docs(mutex_);
        for (const auto &[path, doc] : documents_)
            if (resourceContains(".cycode/merge-receipts", path))
                throw std::runtime_error("Close managed merge receipt documents before applying");
        writeFileExclusive(receipt, journal.dump(2) + "\n");
        if (commit)
            writeFileExclusive(messageFile, request.at("message").get<std::string>());
    }
    if (snapshot().at("revision") != before.at("revision"))
        throw std::runtime_error(
            "Merge changed before dispatch; review again (the intent receipt was retained)");
    const auto process = git(commit ? std::vector<std::string>{"commit", "--file", pathUtf8(messageFile),
                                                               "--cleanup=verbatim", "--no-gpg-sign"}
                                    : std::vector<std::string>{"merge", "--quit"},
                             120, hooks, true);
    Json result = {{"exitCode", process.exitCode},
                   {"output", processOutputText(process.output, true)},
                   {"timedOut", process.timedOut},
                   {"truncated", process.truncated},
                   {"action", request.at("action")},
                   {"receipt", receiptPath},
                   {"newCommit", ""}};
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    try {
        const auto after = snapshot();
        result["state"] = after;
        if (process.exitCode == 0 && !process.timedOut && !process.truncated) {
            if (after.at("kind") != "none")
                throw std::runtime_error("Git still reports an active operation after merge completion");
            if (commit) {
                const auto committed = after.at("head").get<std::string>();
                // Read the actual object, independent of log signatures/notes or grafted history.
                if (parseGitMergeCommitParents(checked({"cat-file", "commit", committed})) != parents)
                    throw std::runtime_error("Committed merge parents differ from the reviewed heads");
                result["newCommit"] = after.at("head");
                if (parseGitMergeTree(checked({"ls-tree", "-r", "-z", "--full-tree", committed})) !=
                        beforeIndex ||
                    observedIndex != beforeIndex || observedFlags != beforeFlags || ref("HEAD") != committed)
                    throw std::runtime_error(
                        "Committed tree or current index differs from the reviewed staged records");
            } else if (after.at("head") != before.at("head") || observedIndex != beforeIndex ||
                       observedFlags != beforeFlags)
                throw std::runtime_error("HEAD or index records changed while quitting merge tracking");
        }
    } catch (const std::exception &error) {
        result["inspectionError"] = error.what();
    }
    return result;
}
} // namespace cycode

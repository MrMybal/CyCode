#include "cycode/GitReplay.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/GitConflicts.h"
#include "cycode/GitMerge.h"
#include "cycode/Recovery.h"
#include "cycode/TextEdits.h"
namespace cycode {
namespace {
namespace fs = std::filesystem;
std::string trimReplay(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}
void replayOrdinary(const fs::path &path) {
    const auto absolute = fs::absolute(path).lexically_normal(), actual = fs::weakly_canonical(path);
#ifdef _WIN32
    const bool same = _wcsicmp(absolute.c_str(), actual.c_str()) == 0;
#else
    const bool same = absolute == actual;
#endif
    if (!same || fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Redirected Git replay metadata is unsupported");
}
} // namespace
Json Engine::gitReplay(const std::string &method, const Json &params) {
    if (method != "vcs.replay.state" && method != "vcs.replay.plan" && method != "vcs.replay.apply")
        throw std::runtime_error("Unknown Git replay operation");
    const auto selected = gitSelection(params);
    if (!selected.marker)
        throw std::runtime_error("Select an actual Git root to replay commits");
    std::string disabledHooks;
    auto git = [&](std::vector<std::string> args, bool mutation = false) {
        args.insert(args.begin(),
                    {"git", "--no-pager", "--literal-pathspecs", "-C", pathUtf8(selected.directory), "-c",
                     "core.fsmonitor=false", "-c", "rerere.enabled=false", "-c", "rerere.autoupdate=false",
                     "-c", "maintenance.auto=false", "-c", "gc.auto=0", "-c", "commit.gpgSign=false"});
        if (!disabledHooks.empty())
            args.insert(args.begin() + 5, {"-c", "core.hooksPath=" + disabledHooks});
        return runProcess(args, selected.directory, std::chrono::seconds(mutation ? 120 : 15),
                          8 * 1024 * 1024, nullptr,
                          {{"GIT_OPTIONAL_LOCKS", "0"},
                           {"GIT_TERMINAL_PROMPT", "0"},
                           {"GIT_NO_LAZY_FETCH", "1"},
                           {"GIT_NO_REPLACE_OBJECTS", "1"},
                           {"GIT_ALLOW_PROTOCOL", ":"},
                           {"GIT_EDITOR", ":"}});
    };
    auto checked = [&](std::vector<std::string> args) {
        const auto reply = git(std::move(args));
        if (reply.exitCode || reply.timedOut || reply.truncated)
            throw std::runtime_error("Git replay inspection failed: " + reply.output);
        return reply.output;
    };
    const auto admin =
        fs::absolute(utf8Path(trimReplay(checked({"rev-parse", "--absolute-git-dir"})))).lexically_normal();
    replayOrdinary(admin);
    const auto adminIdentity = directoryIdentity(admin);
    disabledHooks = pathUtf8(admin / "HEAD");
    auto snapshot = [&] {
        // The common inspector already bounds and double-checks complete index, flags,
        // metadata and native document revisions. Do not create a second editor model.
        const auto base = gitMerge("vcs.merge.state", params);
        std::string marker;
        const auto &ops = base.at("otherOperations");
        if (base.at("kind") != "merge" && ops.size() == 1 && (ops[0] == "cherry-pick" || ops[0] == "revert"))
            marker = trimReplay(checked(
                {"rev-parse", "--verify", ops[0] == "cherry-pick" ? "CHERRY_PICK_HEAD" : "REVERT_HEAD"}));
        if (gitMerge("vcs.merge.state", params).at("revision") != base.at("revision") ||
            directoryIdentity(admin) != adminIdentity || gitSelection(params).identity != selected.identity)
            throw std::runtime_error("Git replay state changed during inspection; refresh");
        auto result = gitReplayState(base, marker);
        result["revision"] = RecoveryStore::fingerprint(base.at("revision").get<std::string>() +
                                                        result.at("kind").get<std::string>() + marker);
        return result;
    };
    const auto before = snapshot();
    if (method == "vcs.replay.state")
        return before;
    if (params.at("expectedRepository") != selected.identity ||
        params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Git replay repository/state changed; refresh before previewing");
    const auto request = gitReplayRequest(params);
    const auto action = request.at("action").get<std::string>();
    const bool start = action == "cherryPick" || action == "revert", quit = action == "quit";
    if (!before.at(start ? "canStart" : quit ? "canQuit" : "canContinue").get<bool>())
        throw std::runtime_error("Git replay action is blocked; inspect its blockers");
    const auto source =
        start ? request.at("commit").get<std::string>() : before.at("operationCommit").get<std::string>();
    if (trimReplay(checked({"rev-parse", "--verify", source + "^{commit}"})) != source)
        throw std::runtime_error("Git replay source is not the reviewed commit object");
    const auto rawCommit = checked({"cat-file", "commit", source});
    const auto parents = parseGitMergeCommitParents(rawCommit);
    if (parents.size() > 1)
        throw std::runtime_error("Merge-commit replay needs explicit mainline support; use Git externally");
    const auto split = rawCommit.find("\n\n");
    const auto message = rawCommit.substr(split + 2);
    if (message.size() > 16384 || message.find('\0') != std::string::npos)
        throw std::runtime_error("Replay source message exceeds its UTF-8 text bound");
    (void)Json(message).dump();
    const auto command =
        (start ? action : before.at("kind").get<std::string>()) == "cherryPick" ? "cherry-pick" : "revert";
    const auto paths = start ? parseGitMergeChanges(checked({"diff-tree", "--root", "--no-commit-id",
                                                             "--no-renames", "--no-ext-diff", "--no-textconv",
                                                             "--name-status", "-r", "-z", source, "--"}))
                             : before.at("stagedPaths");
    if (!quit)
        for (const auto &entry : paths) {
            const auto file = selected.directory / utf8Path(entry.at("path").get<std::string>());
            if (containsPath(root_ / ".cycode", file) || containsPath(admin, file))
                throw std::runtime_error("Replay cannot rewrite active workspace/Git metadata");
        }
    const auto summary = start ? checked({"diff-tree", "--root", "--no-commit-id", "--no-renames",
                                          "--no-ext-diff", "--no-textconv", "--stat", "-r", source, "--"})
                               : before.at("stagedSummary").get<std::string>();
    if (summary.size() > 1024 * 1024)
        throw std::runtime_error("Replay change summary exceeds 1 MiB");
    const std::vector<std::string> args =
        start ? std::vector<std::string>{command, "--no-edit", "--no-gpg-sign", source}
              : std::vector<std::string>{command, quit ? "--quit" : "--continue"};
    Json plan = {{"repository", selected.root},
                 {"identity", selected.identity},
                 {"stateRevision", before.at("revision")},
                 {"request", request},
                 {"sourceCommit", source},
                 {"sourceMessage", message},
                 {"sourceParents", parents},
                 {"paths", paths},
                 {"summary", summary},
                 {"expectedParent", before.at("head")},
                 {"arguments", args},
                 {"createsCommit", !quit},
                 {"preservesWorkingFiles", quit},
                 {"preservesIndex", quit},
                 {"mayExecuteHooks", false},
                 {"signsCommit", false},
                 {"mayExecuteProjectTools", !quit}};
    plan["revision"] = RecoveryStore::fingerprint(plan.dump());
    if (snapshot().at("revision") != before.at("revision"))
        throw std::runtime_error("Replay changed during preview; refresh");
    if (method == "vcs.replay.plan")
        return plan;
    if (params.at("expectedPlanRevision") != plan.at("revision"))
        throw std::runtime_error("Replay preview changed; review again");
    const auto index = checked({"ls-files", "--stage", "-z"}), flags = checked({"ls-files", "-v", "-z"});
    const auto folder = root_ / ".cycode/merge-receipts";
    replayOrdinary(root_ / ".cycode");
    replayOrdinary(folder);
    fs::create_directory(root_ / ".cycode");
    fs::create_directory(folder);
    replayOrdinary(folder);
    const auto id =
        "replay-" +
        RecoveryStore::fingerprint(
            plan.dump() + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto receipt = folder / utf8Path(id + ".json");
    {
        std::lock_guard files(resourcesMutex_);
        std::lock_guard docs(mutex_);
        for (const auto &[path, doc] : documents_)
            if (resourceContains(".cycode/merge-receipts", path))
                throw std::runtime_error("Close managed replay receipt documents before applying");
        writeFileExclusive(receipt, Json({{"schemaVersion", 1}, {"plan", plan}}).dump(2) + "\n");
    }
    if (snapshot().at("revision") != before.at("revision") ||
        checked({"ls-files", "--stage", "-z"}) != index || checked({"ls-files", "-v", "-z"}) != flags)
        throw std::runtime_error("Replay changed before dispatch; intent retained, review again");
    const auto process = git(args, true);
    Json result = {{"exitCode", process.exitCode},
                   {"output", process.output},
                   {"timedOut", process.timedOut},
                   {"truncated", process.truncated},
                   {"action", action},
                   {"receipt", pathUtf8(receipt.lexically_relative(root_))},
                   {"newCommit", ""}};
    try {
        const auto after = snapshot();
        result["state"] = after;
        if (!process.exitCode && !process.timedOut && !process.truncated) {
            if (after.at("kind") != "none")
                throw std::runtime_error("Git still reports an operation after replay");
            const auto current = after.at("head").get<std::string>();
            if (quit) {
                if (after.at("head") != before.at("head") ||
                    checked({"ls-files", "--stage", "-z"}) != index ||
                    checked({"ls-files", "-v", "-z"}) != flags)
                    throw std::runtime_error("HEAD/index changed while quitting replay tracking");
            } else {
                result["newCommit"] = current;
                if (current == before.at("head").get<std::string>() ||
                    parseGitMergeCommitParents(checked({"cat-file", "commit", current})) !=
                        Json::array({before.at("head")}))
                    throw std::runtime_error(
                        "Replay commit does not have exactly the reviewed previous HEAD parent");
                const auto tree = parseGitMergeTree(checked({"ls-tree", "-r", "-z", "--full-tree", current}));
                if (tree != parseGitConflictIndex(checked({"ls-files", "--stage", "-z"})) ||
                    (!start && tree != parseGitConflictIndex(index)) ||
                    after.at("workingDirty").get<bool>() || after.at("hiddenIndexFlags").get<bool>() ||
                    !after.at("conflicts").empty() || !after.at("stagedPaths").empty() ||
                    trimReplay(checked({"rev-parse", "HEAD"})) != current)
                    throw std::runtime_error(
                        "Replay tree/index/working-file outcome differs from the reviewed action");
            }
        }
    } catch (const std::exception &error) {
        result["inspectionError"] = error.what();
    }
    return result;
}
} // namespace cycode

#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/GitConflicts.h"
#include "cycode/Recovery.h"
#include <charconv>
namespace cycode {
Json Engine::gitCheckout(const std::string &method, const Json &params) {
    if (method != "vcs.checkout.plan" && method != "vcs.checkout.apply" && method != "vcs.checkout.undo")
        throw std::runtime_error("Unknown Git working-file operation");
    if (!params.contains("expectedRepository"))
        throw std::runtime_error("Observe the Git repository identity before a working-file action");
    const auto selected = gitSelection(params);
    if (!selected.marker)
        throw std::runtime_error("Select an actual Git repository root");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    auto checked = [&](std::vector<std::string> args, size_t limit = 8 * 1024 * 1024) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Git working-file inspection exceeded its 45-second budget");
        args.insert(args.begin(), {"git", "--no-pager", "--literal-pathspecs", "-C",
                                   pathUtf8(selected.directory), "-c", "core.fsmonitor=false"});
        const auto result = runProcess(args, selected.directory, std::chrono::seconds(15), limit, nullptr,
                                       {{"GIT_OPTIONAL_LOCKS", "0"},
                                        {"GIT_TERMINAL_PROMPT", "0"},
                                        {"GIT_NO_LAZY_FETCH", "1"},
                                        {"GIT_ALLOW_PROTOCOL", ":"}});
        if (result.exitCode || result.timedOut || result.truncated)
            throw std::runtime_error("Git working-file inspection failed: " + result.output);
        return result.output;
    };
    auto admin = checked({"rev-parse", "--absolute-git-dir"});
    while (!admin.empty() && (admin.back() == '\n' || admin.back() == '\r'))
        admin.pop_back();
    const auto adminIdentity = directoryIdentity(utf8Path(admin));
    std::string observedRaw;
    auto indexRevision = [&] {
        const auto head = checked({"rev-parse", "--verify", "HEAD"}) +
                          checked({"rev-parse", "--symbolic-full-name", "HEAD"});
        const auto raw = checked({"ls-files", "--stage", "-z"});
        (void)parseGitConflictIndex(raw);
        if (raw != checked({"ls-files", "--stage", "-z"}) ||
            head != checked({"rev-parse", "--verify", "HEAD"}) +
                        checked({"rev-parse", "--symbolic-full-name", "HEAD"}) ||
            directoryIdentity(utf8Path(admin)) != adminIdentity ||
            gitSelection(params).identity != selected.identity)
            throw std::runtime_error(
                "Git repository, branch or index changed during working-file inspection");
        observedRaw = raw;
        return RecoveryStore::fingerprint(selected.identity + "\n" + adminIdentity + "\n" + head + raw);
    };
    const auto currentRevision = indexRevision();
    std::lock_guard files(resourcesMutex_);
    auto commit = [&](const Json &plan, const std::function<void()> &action) {
        if (!plan.at("openDocuments").empty())
            throw std::runtime_error("Close affected documents before changing the working file");
        auto guarded = plan;
        guarded["closedRoots"] = Json::array({plan.at("path"), plan.at("destination"), ".cycode/trash"});
        return resourceCommit(guarded, action);
    };
    if (method == "vcs.checkout.plan") {
        const auto path = params.at("path").get<std::string>(),
                   action = params.at("action").get<std::string>();
        (void)gitConflictChoices(Json::array({{{"path", path}, {"choice", "ours"}}}));
        if (action != "materialize" && action != "remove")
            throw std::runtime_error("Choose materialize or remove explicitly");
        const auto documentPath = selected.root == "." ? path : selected.root + "/" + path;
        if (gitFileArgument(root_, selected, resolve(documentPath)) != path)
            throw std::runtime_error("Working file belongs to another repository or redirected path");
        const auto index = parseGitConflictIndex(observedRaw);
        std::optional<std::string> bytes;
        Json version = nullptr;
        if (action == "materialize") {
            if (!index.contains(path) || index.at(path).size() != 1 || !index.at(path).contains("0"))
                throw std::runtime_error(
                    "Resolve and stage this file before synchronizing its working bytes");
            version = index.at(path).at("0");
            if (version.at("mode") != "100644" && version.at("mode") != "100755")
                throw std::runtime_error("Only ordinary staged blobs can be materialized");
            const auto oid = version.at("oid").get<std::string>();
            auto number = checked({"cat-file", "-s", oid});
            while (!number.empty() && (number.back() == '\n' || number.back() == '\r'))
                number.pop_back();
            uint64_t size = 0;
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), size);
            if (number.empty() || parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
                size > 16 * 1024 * 1024)
                throw std::runtime_error(
                    "Staged blob exceeds the 16 MiB materialization limit or has an invalid size");
            bytes = checked({"cat-file", "blob", oid}, 16 * 1024 * 1024 + 1);
            if (bytes->size() != size)
                throw std::runtime_error("Staged blob output differs from its recorded byte count");
        } else if (index.contains(path))
            throw std::runtime_error("Remove the index entry explicitly before reviewing physical removal");
        if (indexRevision() != currentRevision)
            throw std::runtime_error("Git index changed while preparing the working file");
        const auto resource = resources_->previewFileInstall(
            documentPath, std::move(bytes), !version.is_null() && version.at("mode") == "100755",
            resourceDocuments());
        Json plan = {{"id", resource.at("id")},
                     {"repository", selected.root},
                     {"identity", selected.identity},
                     {"indexRevision", currentRevision},
                     {"path", path},
                     {"documentPath", documentPath},
                     {"action", action},
                     {"version", version},
                     {"originalExisted", resource.at("snapshot").at("exists")},
                     {"originalBytes", resource.at("snapshot").at("bytes")},
                     {"replacementBytes", resource.at("replacementBytes")},
                     {"blocked", resource.at("blocked")},
                     {"preservesIndex", true},
                     {"backupsPersist", true},
                     {"rawBytes", true},
                     {"mayContactRemote", false},
                     {"resourceGuard", resource.at("snapshot")},
                     {"parentIdentity", resource.at("parentIdentity")}};
        plan["revision"] = RecoveryStore::fingerprint(plan.dump());
        if (gitCheckoutPlans_.size() >= 16)
            gitCheckoutPlans_.erase(gitCheckoutPlans_.begin());
        gitCheckoutPlans_[resource.at("id").get<std::string>()] = plan;
        plan.erase("resourceGuard");
        plan.erase("parentIdentity");
        return plan;
    }
    const auto id = params.at("id").get<std::string>();
    const auto found = gitCheckoutPlans_.find(id);
    if (found == gitCheckoutPlans_.end())
        throw std::runtime_error("Working-file receipt expired; use persistent project trash for recovery");
    const auto plan = found->second;
    if (plan.at("repository") != selected.root || plan.at("identity") != selected.identity ||
        plan.at("revision") != params.at("expectedPlanRevision"))
        throw std::runtime_error("Working-file action belongs to another repository or preview");
    if (method == "vcs.checkout.apply" && plan.at("indexRevision") != currentRevision)
        throw std::runtime_error("Git index changed; prepare a new working-file preview");
    if (indexRevision() != currentRevision)
        throw std::runtime_error("Git state changed before the working-file action");
    const auto resource = method == "vcs.checkout.apply"
                              ? resources_->applyFileInstall(id, commit)
                              : resources_->undoFileInstall(id, resourceDocuments(), commit);
    Json result = {{"id", id},
                   {"repository", selected.root},
                   {"identity", selected.identity},
                   {"path", plan.at("path")},
                   {"documentPath", plan.at("documentPath")},
                   {"revision", plan.at("revision")},
                   {"state", resource.at("state")},
                   {"backup", resource.value("backup", std::string{})},
                   {"undoBackup", resource.value("undoBackup", std::string{})},
                   {"error", resource.value("error", std::string{})},
                   {"preservesIndex", true},
                   {"rawBytes", true},
                   {"canUndo", method == "vcs.checkout.apply" && resource.at("state") == "applied"}};
    try {
        if (indexRevision() != currentRevision)
            throw std::runtime_error("Git changed during working-file synchronization; inspect actual state");
    } catch (const std::exception &error) {
        result["inspectionError"] = error.what();
        result["canUndo"] = false;
    }
    return result;
}
} // namespace cycode

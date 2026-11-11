#include "cycode/ProcessText.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
namespace cycode {
namespace {
namespace fs = std::filesystem;
std::string trimmed(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}
Json processResult(const ProcessResult &r) {
    return {
        {"exitCode", r.exitCode}, {"output", processOutputText(r.output, true)}, {"truncated", r.truncated}, {"timedOut", r.timedOut}};
}
} // namespace
GitRepositorySelection Engine::gitSelection(const Json &params) const {
    for (const auto *name : {"GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE",
                             "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES"})
        if (const auto *value = std::getenv(name); value && *value)
            throw std::runtime_error(
                "Unset Git repository-redirect environment variables before using the IDE Git service");
    const auto folder = resolve(params.value("repository", std::string(".")));
    if (!fs::is_directory(folder))
        throw std::runtime_error("Selected Git folder is unavailable");
    const auto selection = gitRepositoryAt(root_, folder, false);
    if (params.contains("expectedRepository") && params.at("expectedRepository") != selection.identity)
        throw std::runtime_error("Git repository identity changed; refresh before acting");
    if (selection.marker) {
        const auto probe =
            runProcess({"git", "-C", pathUtf8(folder), "rev-parse", "--show-toplevel"}, folder);
        if (probe.exitCode || probe.truncated || probe.timedOut ||
            !fs::equivalent(utf8Path(trimmed(probe.output)), folder))
            throw std::runtime_error("Git reports a different repository root; refresh the selection: " +
                Json({{"exitCode",probe.exitCode},{"timedOut",probe.timedOut},{"truncated",probe.truncated},
                      {"output",probe.output}}).dump());
    }
    return selection;
}
Json Engine::gitWorktrees(const std::string &method, const Json &params) {
    if (method == "vcs.worktree.plan" || method == "vcs.worktree.move" || method == "vcs.worktree.repair")
        return gitWorktreeMaintenance(method, params);
    const auto selected = gitSelection(params);
    auto git = [&](std::vector<std::string> args, int seconds = 15) {
        args.insert(args.begin(),
                    {"git", "--no-pager", "--literal-pathspecs", "-C", pathUtf8(selected.directory)});
        return runProcess(args, selected.directory, std::chrono::seconds(seconds), 2 * 1024 * 1024);
    };
    auto checked = [&](std::vector<std::string> args) {
        const auto result = git(std::move(args));
        if (result.exitCode || result.timedOut || result.truncated)
            throw std::runtime_error("Git worktree inspection failed: " + result.output);
        return trimmed(result.output);
    };
    if (!fs::equivalent(utf8Path(checked({"rev-parse", "--show-toplevel"})), selected.directory))
        throw std::runtime_error("Select a repository root to manage linked worktrees");
    const auto common =
        fs::canonical(utf8Path(checked({"rev-parse", "--path-format=absolute", "--git-common-dir"})));
    const auto admin = fs::canonical(utf8Path(checked({"rev-parse", "--absolute-git-dir"})));
    const auto commonIdentity = directoryIdentity(common), adminIdentity = directoryIdentity(admin);
    auto catalogue = [&] {
        const auto raw = git({"worktree", "list", "--porcelain", "-z"});
        if (raw.exitCode || raw.timedOut || raw.truncated)
            throw std::runtime_error("Cannot read complete Git worktree catalogue: " + raw.output);
        auto entries = parseGitWorktrees(raw.output);
        std::set<std::string> paths;
        for (auto &entry : entries) {
            auto path = utf8Path(entry.at("path")).lexically_normal();
            if (!path.is_absolute())
                throw std::runtime_error("Git worktree catalogue contains a relative path");
            if (containsPath(root_, path))
                path = fs::weakly_canonical(path);
            if (!paths.insert(pathUtf8(path)).second)
                throw std::runtime_error("Duplicate Git worktree path");
            const bool inside = containsPath(root_, path);
            entry["path"] = pathUtf8(path);
            entry["insideWorkspace"] = inside;
            entry["root"] = inside ? pathUtf8(path.lexically_relative(root_)) : std::string{};
            entry["current"] = path == selected.directory;
            entry["available"] = inside && fs::is_directory(path);
            entry["directoryIdentity"] =
                entry.at("available").get<bool>() ? directoryIdentity(path) : std::string{};
        }
        const auto revision =
            RecoveryStore::fingerprint(selected.identity + "\n" + commonIdentity + "\n" + adminIdentity +
                                       "\n" + raw.output + "\n" + entries.dump());
        return Json{{"repository", selected.root},
                    {"identity", selected.identity},
                    {"revision", revision},
                    {"entries", entries}};
    };
    const auto before = catalogue();
    if (method == "vcs.worktrees")
        return before;
    if (params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Git worktree catalogue changed; refresh before editing");
    auto verify = [&] {
        if (gitSelection(params).identity != selected.identity ||
            directoryIdentity(common) != commonIdentity || directoryIdentity(admin) != adminIdentity ||
            catalogue().at("revision") != before.at("revision"))
            throw std::runtime_error("Git worktree selection changed before commit");
    };
    auto noOpenDocuments = [&](const fs::path &path) {
        std::lock_guard lock(mutex_);
        for (const auto &[key, doc] : documents_)
            if (containsPath(path, resolve(key)))
                throw std::runtime_error(
                    "Close all documents in the worktree destination before changing it");
    };
    ProcessResult result;
    auto allowedDestination = [&](const fs::path &target) {
        for (const auto &part : target.lexically_relative(root_)) {
            auto name = pathUtf8(part);
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name == ".git" || name == ".cycode")
                throw std::runtime_error("Worktrees cannot modify application/Git metadata");
        }
    };
    if (method == "vcs.worktree.add") {
        const auto target = resolve(params.at("destination"));
        allowedDestination(target);
        if (target == root_ || target == selected.directory || containsPath(target, selected.directory) ||
            !fs::is_directory(target.parent_path()) || fs::exists(fs::symlink_status(target)))
            throw std::runtime_error("Choose a new worktree directory with an existing workspace parent");
        // Directory identity rejects redirected parents; Git never receives a caller shell string.
        const auto parentIdentity = directoryIdentity(target.parent_path());
        const auto relative = target.lexically_relative(root_);
        for (const auto &part : relative)
            if (part == ".git" || part == ".cycode")
                throw std::runtime_error("Worktrees cannot be created inside application/Git metadata");
        noOpenDocuments(target);
        const auto mode = params.value("mode", std::string("newBranch"));
        const auto branch = params.value("branch", std::string{});
        auto validBranch = [&] {
            if (branch.empty() || branch.size() > 256 || branch.front() == '-' ||
                branch.find('\0') != std::string::npos ||
                git({"check-ref-format", "--branch", branch}).exitCode)
                throw std::runtime_error("Invalid worktree branch name");
        };
        std::vector<std::string> args = {"worktree", "add"};
        std::string revision;
        if (mode == "existingBranch") {
            validBranch();
            revision = branch;
            checked({"rev-parse", "--verify", "--end-of-options", "refs/heads/" + branch + "^{commit}"});
        } else {
            const auto start = params.value("start", std::string("HEAD"));
            if (start.empty() || start.size() > 512 || start.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid worktree start revision");
            revision = checked({"rev-parse", "--verify", "--end-of-options", start + "^{commit}"});
            if (mode == "newBranch") {
                validBranch();
                args.insert(args.end(), {"-b", branch, "--no-track"});
            } else if (mode == "detached")
                args.push_back("--detach");
            else
                throw std::runtime_error("Unsupported worktree creation mode");
        }
        verify();
        if (directoryIdentity(target.parent_path()) != parentIdentity || !fs::create_directory(target))
            throw std::runtime_error("Worktree destination changed before creation");
        const auto ownedIdentity = directoryIdentity(target);
        args.insert(args.end(), {"--", pathUtf8(target), revision});
        result = git(args, 180);
        // Only remove our still-empty reservation after failure, never a checkout/payload.
        if (result.exitCode || result.timedOut)
            try {
                if (directoryIdentity(target) == ownedIdentity)
                    fs::remove(target);
            } catch (...) {
            }
    } else {
        const auto target = resolve(params.at("path"));
        allowedDestination(target);
        Json entry;
        for (const auto &candidate : before.at("entries"))
            if (candidate.at("insideWorkspace").get<bool>() && utf8Path(candidate.at("path")) == target)
                entry = candidate;
        if (entry.is_null() || !entry.at("insideWorkspace").get<bool>() || entry.at("primary").get<bool>() ||
            entry.at("bare").get<bool>() || entry.at("current").get<bool>() || target == root_ ||
            containsPath(target, selected.directory) || !entry.at("available").get<bool>())
            throw std::runtime_error("Select another available linked worktree inside this workspace");
        const auto targetIdentity = directoryIdentity(target);
        std::vector<std::string> args = {"worktree"};
        if (method == "vcs.worktree.remove") {
            if (entry.at("locked").get<bool>())
                throw std::runtime_error("Unlock this worktree explicitly before removal");
            noOpenDocuments(target);
            args.push_back("remove");
        } else if (method == "vcs.worktree.lock") {
            auto reason = params.value("reason", std::string{});
            if (reason.size() > 512 || reason.find('\0') != std::string::npos)
                throw std::runtime_error("Worktree lock reason exceeds its limit");
            args.push_back("lock");
            if (!reason.empty())
                args.insert(args.end(), {"--reason", reason});
        } else if (method == "vcs.worktree.unlock")
            args.push_back("unlock");
        else
            throw std::runtime_error("Unsupported Git worktree operation");
        verify();
        if (directoryIdentity(target) != targetIdentity)
            throw std::runtime_error("Worktree directory identity changed");
        args.insert(args.end(), {"--", pathUtf8(target)});
        result = git(args, 30);
    }
    auto output = processResult(result);
    try {
        output["catalog"] = catalogue();
    } catch (const std::exception &e) {
        output["catalogError"] = e.what();
    }
    monitor_.invalidate();
    return output;
}
} // namespace cycode

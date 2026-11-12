#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/GitSafety.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <set>
namespace cycode {
Json Engine::gitHunks(const std::string &method, const Json &params) {
    const auto selected=gitSelection(params);
    const auto &gitRoot=selected.directory;
    auto git = [&](std::vector<std::string> args) {
        args.insert(args.begin(), {"git", "--no-pager", "--literal-pathspecs", "-C", pathUtf8(gitRoot), "-c",
                                   "core.quotepath=true"});
        hardenGitArguments(args);
        return runProcess(args, gitRoot, std::chrono::seconds(15), 2 * 1024 * 1024);
    };
    auto checked = [&](std::vector<std::string> args) {
        const auto r = git(std::move(args));
        if (r.exitCode || r.truncated || r.timedOut)
            throw std::runtime_error("Git hunk operation failed: " + r.output);
        return r.output;
    };
    auto root = checked({"rev-parse", "--show-toplevel"});
    while (!root.empty() && (root.back() == '\n' || root.back() == '\r'))
        root.pop_back();
    if (!std::filesystem::equivalent(utf8Path(root), gitRoot))
        throw std::runtime_error("Open the Git repository root to stage hunks");
    auto diff = [&](const std::string &path, bool staged) {
        std::vector<std::string> args = {
            "diff",         "--no-ext-diff", "--no-textconv",          "--no-color",      "--no-renames",
            "--full-index", "--unified=3",   "--inter-hunk-context=0", "--src-prefix=a/", "--dst-prefix=b/"};
        if (staged)
            args.push_back("--cached");
        args.insert(args.end(), {"--", gitFileArgument(root_,selected,resolve(path))});
        return checked(args);
    };
    auto index = [&](const std::string &path) { return checked({"ls-files", "--stage", "-z", "--", gitFileArgument(root_,selected,resolve(path))}); };
    auto clean = [&](const std::string &path) {
        std::lock_guard lock(mutex_);
        auto it = documents_.find(path);
        if (it != documents_.end() && (it->second.dirty || !it->second.diskConflict.empty()))
            throw std::runtime_error("Save or reconcile the file before staging hunks");
    };
    const auto now = std::chrono::steady_clock::now();
    for (auto it = hunkPlans_.begin(); it != hunkPlans_.end();)
        if (now - it->second.created > std::chrono::minutes(5))
            it = hunkPlans_.erase(it);
        else
            ++it;
    if (method == "vcs.hunks") {
        const auto path = pathUtf8(resolve(params.at("path")).lexically_relative(root_));
        clean(path);
        if (!checked({"ls-files", "--unmerged", "--", gitFileArgument(root_,selected,resolve(path))}).empty())
            throw std::runtime_error("Resolve merge conflicts before staging hunks");
        HunkPlan plan;
        plan.repository=selected.root;plan.repositoryIdentity=selected.identity;
        plan.path = path;
        plan.staged = params.value("staged", false);
        plan.index = index(path);
        plan.patch = diff(path, plan.staged);
        std::string reason;
        for (const auto *marker : {"GIT binary patch", "Binary files ", "new file mode ",
                                   "deleted file mode ", "old mode ", "new mode "})
            if (plan.patch.find(marker) != std::string::npos)
                reason = "Use whole-file staging for binary, added, deleted or mode-changing files";
        if (plan.patch.find('\0') != std::string::npos)
            reason = "Hunks require UTF-8 text";
        if (reason.empty()) {
            validateEditorText(plan.patch);
            size_t at = plan.patch.find("\n@@ ");
            while (at != std::string::npos) {
                const auto next = plan.patch.find("\n@@ ", at + 1);
                plan.hunks.push_back(
                    plan.patch.substr(at + 1, next == std::string::npos ? std::string::npos : next - at));
                at = next;
                if (plan.hunks.size() > 1000)
                    throw std::runtime_error("Too many diff hunks");
            }
        }
        Json hunks = Json::array();
        for (size_t i = 0; i < plan.hunks.size(); ++i)
            hunks.push_back({{"index", i},
                             {"header", plan.hunks[i].substr(0, plan.hunks[i].find('\n'))},
                             {"patch", plan.hunks[i]}});
        if (hunkPlans_.size() >= 16)
            hunkPlans_.erase(hunkPlans_.begin());
        const auto id = "hunks-" + std::to_string(nextHunkPlan_++);
        auto header = plan.patch.substr(0, plan.patch.find("\n@@ ") == std::string::npos
                                               ? plan.patch.size()
                                               : plan.patch.find("\n@@ ") + 1);
        Json result = {{"id", id},         {"path", path},   {"staged", plan.staged},
                       {"header", header}, {"hunks", hunks}, {"reason", reason}};
        hunkPlans_.emplace(id, std::move(plan));
        return result;
    }
    const auto id = params.at("id").get<std::string>();
    const auto it = hunkPlans_.find(id);
    if (it == hunkPlans_.end())
        throw std::runtime_error("Hunk preview expired; refresh it");
    const auto &plan = it->second;
    if(plan.repository!=selected.root||plan.repositoryIdentity!=selected.identity)
        throw std::runtime_error("Hunk preview belongs to another repository; refresh it");
    clean(plan.path);
    const auto &chosenHunks = params.at("hunks");
    if (!chosenHunks.is_array() || chosenHunks.empty() || chosenHunks.size() > plan.hunks.size())
        throw std::runtime_error("Select at least one issued hunk");
    std::set<size_t> indices;
    for (const auto &v : chosenHunks) {
        if (!v.is_number_unsigned() && !v.is_number_integer())
            throw std::runtime_error("Invalid hunk index");
        const auto n = v.get<int64_t>();
        if (n < 0 || size_t(n) >= plan.hunks.size() || !indices.insert(size_t(n)).second)
            throw std::runtime_error("Invalid or duplicate hunk index");
    }
    if (index(plan.path) != plan.index || diff(plan.path, plan.staged) != plan.patch)
        throw std::runtime_error("Git index or file changed; refresh the hunk preview");
    std::string patch = plan.patch.substr(0, plan.patch.find("\n@@ ") + 1);
    for (auto i : indices)
        patch += plan.hunks[i];
    // Git owns the atomic index lock and validates the exact old text. The patch
    // is host-issued and cannot contain a caller-supplied path or command.
    const auto folder = resolve(".cycode/hunk-patches");
    std::filesystem::create_directories(folder);
    const auto file = folder / utf8Path(id + "-" + std::to_string(now.time_since_epoch().count()) + ".patch");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{file};
    writeTextFileAtomic(file, patch, std::nullopt);
    std::vector<std::string> args = {"apply", "--cached", "--whitespace=nowarn"};
    if (plan.staged)
        args.push_back("--reverse");
    args.insert(args.end(), {"--", pathUtf8(file)});
    const auto output = checked(args);
    hunkPlans_.erase(it);
    return {{"exitCode", 0}, {"output", output}, {"timedOut", false}, {"truncated", false}};
}
} // namespace cycode

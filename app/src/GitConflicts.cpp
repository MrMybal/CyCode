#include "cycode/GitConflicts.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/Recovery.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <charconv>
namespace cycode {
Json Engine::gitConflicts(const std::string &method, const Json &params) {
    const auto selected = gitSelection(params);
    if (!selected.marker)
        throw std::runtime_error("Select an actual Git repository root to inspect conflicts");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    std::string hooksPath;
    auto git = [&](std::vector<std::string> args) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Conflict inspection exceeded 45 seconds");
        args.insert(args.begin(), {"git", "--no-pager", "--literal-pathspecs", "-C",
                                   pathUtf8(selected.directory), "-c", "core.fsmonitor=false"});
        if (!hooksPath.empty())
            args.insert(args.begin() + 7, {"-c", "core.hooksPath=" + hooksPath});
        return runProcess(args, selected.directory, std::chrono::seconds(15), 8 * 1024 * 1024, nullptr,
                          {{"GIT_OPTIONAL_LOCKS", "0"},
                           {"GIT_TERMINAL_PROMPT", "0"},
                           {"GIT_NO_LAZY_FETCH", "1"},
                           {"GIT_ALLOW_PROTOCOL", ":"}});
    };
    auto checked = [&](std::vector<std::string> args) {
        const auto result = git(std::move(args));
        if (result.exitCode || result.timedOut || result.truncated)
            throw std::runtime_error("Git conflict inspection failed: " + result.output);
        return result.output;
    };
    hooksPath = checked({"rev-parse", "--path-format=absolute", "--git-path", "index"});
    while (!hooksPath.empty() && (hooksPath.back() == '\r' || hooksPath.back() == '\n'))
        hooksPath.pop_back();
    if (!std::filesystem::is_regular_file(utf8Path(hooksPath)) ||
        std::filesystem::is_symlink(std::filesystem::symlink_status(utf8Path(hooksPath))))
        throw std::runtime_error("Conflict resolution requires an ordinary existing Git index");
    // An existing regular file cannot contain post-index-change or other hooks.
    const auto adminIdentity = directoryIdentity(utf8Path(hooksPath).parent_path());
    Json observedIndex;
    auto catalogue = [&] {
        const auto head = checked({"rev-parse", "--verify", "HEAD"}) +
                          checked({"rev-parse", "--symbolic-full-name", "HEAD"});
        const auto raw = checked({"ls-files", "--stage", "-z"});
        const auto index = parseGitConflictIndex(raw);
        Json entries = Json::array();
        for (const auto &[path, stages] : index.items()) {
            if (stages.contains("0"))
                continue;
            if (entries.size() >= 512)
                throw std::runtime_error("More than 512 unresolved conflict paths");
            const int mask = (stages.contains("1") ? 1 : 0) | (stages.contains("2") ? 2 : 0) |
                             (stages.contains("3") ? 4 : 0);
            static const char *statuses[] = {"", "DD", "AU", "UD", "UA", "DU", "AA", "UU"};
            Json entry = {{"path", path},
                          {"documentPath", selected.root == "." ? path : selected.root + "/" + path},
                          {"status", statuses[mask]},
                          {"base", stages.value("1", Json(nullptr))},
                          {"ours", stages.value("2", Json(nullptr))},
                          {"theirs", stages.value("3", Json(nullptr))},
                          {"supported", true},
                          {"reason", ""}};
            try {
                (void)gitConflictChoices(Json::array({{{"path", path}, {"choice", "ours"}}}));
                for (const auto &[stage, version] : stages.items())
                    if (version.at("mode") != "100644" && version.at("mode") != "100755")
                        throw std::runtime_error(
                            "Symlink, gitlink and sparse-directory conflicts require an external resolver");
            } catch (const std::exception &error) {
                entry["supported"] = false;
                entry["reason"] = error.what();
            }
            entries.push_back(std::move(entry));
        }
        if (head != checked({"rev-parse", "--verify", "HEAD"}) +
                        checked({"rev-parse", "--symbolic-full-name", "HEAD"}) ||
            raw != checked({"ls-files", "--stage", "-z"}) ||
            directoryIdentity(utf8Path(hooksPath).parent_path()) != adminIdentity)
            throw std::runtime_error("Git index or HEAD changed during inspection; refresh");
        observedIndex = index;
        return Json{{"repository", selected.root},
                    {"identity", selected.identity},
                    {"revision", RecoveryStore::fingerprint(selected.identity + "\n" + adminIdentity + "\n" +
                                                            head + raw)},
                    {"entries", entries}};
    };
    const auto before = catalogue();
    const auto originalIndex = observedIndex;
    if (method == "vcs.conflict.list")
        return before;
    if (!params.contains("expectedRevision") || params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Conflict catalogue changed; refresh before reviewing");
    if (method == "vcs.conflict.inspect") {
        const auto path = params.at("path").get<std::string>();
        Json entry;
        for (const auto &candidate : before.at("entries"))
            if (candidate.at("path") == path)
                entry = candidate;
        if (entry.is_null())
            throw std::runtime_error("Conflict is no longer available");
        Json versions = Json::object();
        for (const auto *name : {"base", "ours", "theirs"}) {
            const auto &stage = entry.at(name);
            Json version = {{"kind", "absent"}, {"bytes", 0}, {"text", nullptr}};
            if (!stage.is_null()) {
                version["kind"] = "unsupported";
                if (stage.at("mode") == "100644" || stage.at("mode") == "100755") {
                    const auto oid = stage.at("oid").get<std::string>(),
                               sizeText = checked({"cat-file", "-s", oid});
                    uint64_t bytes = 0;
                    const auto end = sizeText.find_first_of("\r\n");
                    const auto number = sizeText.substr(0, end);
                    auto parsed = std::from_chars(number.data(), number.data() + number.size(), bytes);
                    if (number.empty() || parsed.ec != std::errc{} ||
                        parsed.ptr != number.data() + number.size())
                        throw std::runtime_error("Invalid Git blob size");
                    version["bytes"] = bytes;
                    version["kind"] = "large";
                    if (bytes <= 4 * 1024 * 1024) {
                        const auto blob = checked({"cat-file", "blob", oid});
                        if (blob.size() != bytes)
                            throw std::runtime_error("Git blob output differs from its recorded size");
                        version["kind"] = "binary";
                        try {
                            validateEditorText(blob);
                            version["text"] = blob;
                            (void)version.dump();
                            version["kind"] = "text";
                        } catch (const std::exception &) {
                            version["text"] = nullptr;
                        }
                    }
                }
            }
            versions[name] = std::move(version);
        }
        if (catalogue().at("revision") != before.at("revision"))
            throw std::runtime_error("Conflict changed during blob inspection");
        Json result = {{"catalogRevision", before.at("revision")}, {"entry", entry}, {"versions", versions}};
        if (result.dump().size() > 16 * 1024 * 1024)
            throw std::runtime_error("Conflict text preview exceeds the 16 MiB encoded response budget");
        return result;
    }
    if (method != "vcs.conflict.plan" && method != "vcs.conflict.apply")
        throw std::runtime_error("Unknown Git conflict operation");
    if (!params.contains("expectedRepository") || params.at("expectedRepository") != selected.identity)
        throw std::runtime_error("Observe the Git repository identity before conflict resolution");
    auto plan = gitConflictIndexPlan(before.at("entries"), params.at("choices"));
    plan["repository"] = selected.root;
    plan["identity"] = selected.identity;
    plan["catalogRevision"] = before.at("revision");
    plan["revision"] = RecoveryStore::fingerprint(plan.dump());
    if (method == "vcs.conflict.plan")
        return plan;
    if (params.at("expectedPlanRevision") != plan.at("revision"))
        throw std::runtime_error("Conflict preview changed; review it again");
    for (const auto &change : plan.at("changes"))
        if (!change.at("version").is_null())
            checked({"cat-file", "-e", change.at("version").at("oid").get<std::string>() + "^{blob}"});
    if (gitSelection(params).identity != selected.identity ||
        catalogue().at("revision") != before.at("revision"))
        throw std::runtime_error("Git repository/index changed before conflict resolution");
    const auto process = git(plan.at("arguments").get<std::vector<std::string>>());
    Json result = {{"exitCode", process.exitCode},  {"output", process.output},
                   {"timedOut", process.timedOut},  {"truncated", process.truncated},
                   {"preservesWorkingFiles", true}, {"preservesBuffers", true},
                   {"changes", plan.at("changes")}};
    // Never turn a completed/uncertain mutation into a successful inferred outcome.
    try {
        const auto after = catalogue();
        if (gitSelection(params).identity != selected.identity)
            throw std::runtime_error(
                "Git repository identity changed after conflict resolution; inspect actual state");
        const auto actual = parseGitConflictIndex(checked({"ls-files", "--stage", "-z"}));
        auto expectedIndex = originalIndex;
        for (const auto &change : plan.at("changes")) {
            const auto path = change.at("path").get<std::string>();
            if (change.at("deleted").get<bool>()) {
                expectedIndex.erase(path);
                if (actual.contains(path))
                    throw std::runtime_error("Reviewed deletion is not reflected in the actual index");
            } else {
                expectedIndex[path] = {{"0", change.at("version")}};
                if (!actual.contains(path) || actual.at(path).size() != 1 ||
                    actual.at(path).value("0", Json(nullptr)) != change.at("version"))
                    throw std::runtime_error("Actual index does not match the reviewed conflict version");
            }
        }
        if (actual != expectedIndex || observedIndex != actual)
            throw std::runtime_error(
                "Unselected index records changed or postflight was unstable; inspect Git state");
        result["catalog"] = after;
    } catch (const std::exception &error) {
        result["catalogError"] = error.what();
    }
    return result;
}
} // namespace cycode

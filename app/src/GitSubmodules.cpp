#include "cycode/ProcessText.h"
#include "cycode/GitSubmodules.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
namespace fs = std::filesystem;
std::string trimmed(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}
void ordinary(const fs::path &path) {
    if (fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Redirected submodule paths are unsupported");
#ifdef _WIN32
    const auto flags = GetFileAttributesW(path.c_str());
    if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Redirected submodule paths are unsupported");
#endif
}
Json newTargetParents(const fs::path &target, bool requireParent) {
    ordinary(target);
    if (fs::exists(fs::symlink_status(target)))
        throw std::runtime_error("Submodule checkout and metadata destinations must be new");
    Json parents = Json::array();
    auto parent = target.parent_path();
    if (requireParent && !fs::is_directory(parent))
        throw std::runtime_error("Create the submodule destination parent folder first");
    for (size_t depth = 0; !parent.empty(); ++depth) {
        if (depth >= 256)
            throw std::runtime_error("Submodule destination exceeds the directory depth limit");
        ordinary(parent);
        if (fs::exists(parent))
            parents.push_back({{"path", pathUtf8(parent)}, {"identity", directoryIdentity(parent)}});
        else
            parents.push_back({{"path", pathUtf8(parent)}, {"identity", ""}});
        const auto next = parent.parent_path();
        if (next == parent)
            break;
        parent = next;
    }
    return parents;
}
Json existingDirectories(fs::path path) {
    Json result = Json::array();
    for (size_t depth = 0; !path.empty(); ++depth) {
        if (depth >= 256)
            throw std::runtime_error("Submodule metadata exceeds the directory depth limit");
        ordinary(path);
        result.push_back({{"path", pathUtf8(path)}, {"identity", directoryIdentity(path)}});
        const auto next = path.parent_path();
        if (next == path)
            break;
        path = next;
    }
    return result;
}
std::string folded(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
bool overlaps(const std::string &a, const std::string &b) {
    const auto left = folded(a), right = folded(b);
    return left == right || left.starts_with(right + "/") || right.starts_with(left + "/");
}
std::string gitmodulesLines(const std::string &bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (size_t at = 0; at < bytes.size(); ++at) {
        if (bytes[at] == '\r' && at + 1 < bytes.size() && bytes[at + 1] == '\n')
            continue;
        result.push_back(bytes[at]);
    }
    return result;
}
} // namespace
Json Engine::gitSubmodules(const std::string &method, const Json &params) {
    const bool inspection = method == "vcs.submodules";
    const std::string action = inspection ? "" : method.substr(std::string("vcs.submodule.").size());
    const bool adding = action == "addPlan" || action == "add";
    const bool deinitializing = action == "deinitPlan" || action == "deinit";
    const bool configuring = action == "settingsPlan" || action == "settings";
    if (!inspection && !adding && !deinitializing && !configuring && action != "init" && action != "sync" &&
        action != "update")
        throw std::runtime_error("Unsupported Git submodule operation");
    if (!params.is_object() || params.dump().size() > 16384)
        throw std::runtime_error("Submodule request exceeds 16 KiB");
    for (const auto &[key, value] : params.items())
        if (key != "provider" && key != "contextPath" && key != "repository" && key != "expectedRepository" &&
            key != "root" && key != "workspace" &&
            (inspection ||
             (key != "path" && key != "expectedRevision" && !(action == "update" && key == "fetch") &&
              !(adding && (key == "url" || key == "name" || key == "branch")) &&
              !(configuring && (key == "setting" || key == "value")) &&
              !((action == "add" || action == "deinit" || action == "settings") &&
                key == "expectedPlanRevision"))))
            throw std::runtime_error("Unknown submodule request field: " + key);
    if (!inspection && (!params.contains("expectedRepository") || !params.contains("expectedRevision")))
        throw std::runtime_error("Observe the repository identity and submodule revision before acting");
    if (params.contains("fetch") && !params.at("fetch").is_boolean())
        throw std::runtime_error("Submodule fetch must be a boolean");
    const auto addModel = adding ? gitSubmoduleAddModel(params) : Json::object();
    const auto settingsModel = configuring ? gitSubmoduleSettingsModel(params) : Json::object();
    const auto selected = gitSelection(params);
    if (!selected.marker)
        throw std::runtime_error("Select a Git repository root before inspecting submodules");
    std::chrono::steady_clock::time_point inspectionDeadline{};
    auto git = [&](const fs::path &directory, std::vector<std::string> args, int seconds = 15,
                   bool transport = true) {
        // check-ignore consumes literal filenames, not pathspecs. Forcing Git's
        // literal pathspec magic makes that command reject every filename.
        const bool pathspecs = args.empty() || args.front() != "check-ignore";
        if (pathspecs) args.insert(args.begin(), "--literal-pathspecs");
        args.insert(args.begin(), {"git", "--no-pager", "--no-optional-locks", "-c",
                                   "submodule.recurse=false", "-C", pathUtf8(directory)});
        auto timeout = std::chrono::milliseconds(seconds * 1000);
        if (inspectionDeadline != std::chrono::steady_clock::time_point{}) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                inspectionDeadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                throw std::runtime_error("Submodule catalogue exceeded 30 seconds");
            timeout = std::min(timeout, remaining);
        }
        std::map<std::string, std::string> environment = {{"GIT_TERMINAL_PROMPT", "0"}};
        if (!pathspecs) environment["GIT_LITERAL_PATHSPECS"] = "0";
        // Nonempty environment value survives Windows CRT handling of empty values.
        // ':' is a list of empty names, so it authorizes no Git transport protocol.
        if (!transport)
            environment["GIT_ALLOW_PROTOCOL"] = ":";
        return runProcess(args, directory, timeout, 2 * 1024 * 1024, nullptr, environment);
    };
    auto checked = [&](const fs::path &directory, const std::vector<std::string> &args, bool absent = false) {
        const auto response = git(directory, args, 15, false);
        if ((response.exitCode != 0 && !(absent && response.exitCode == 1)) || response.timedOut ||
            response.truncated)
            throw std::runtime_error("Cannot inspect complete submodule state" +
                                     (args.front() == "config" || args.front() == "show"
                                          ? std::string(": Git metadata query failed")
                                          : std::string(": ") + response.output));
        return response.output;
    };
    const auto modules = selected.directory / ".gitmodules";
    auto moduleBytes = [&] {
        ordinary(modules);
        if (!fs::exists(modules))
            return std::string{};
        if (!fs::is_regular_file(modules) || fs::file_size(modules) > 1024 * 1024)
            throw std::runtime_error(".gitmodules must be an ordinary file of at most 1 MiB");
        return readTextFileBytes(modules);
    };
    auto catalogue = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        inspectionDeadline = deadline;
        const auto bytes = moduleBytes();
        const auto configuration = bytes.empty()
                                       ? std::string{}
                                       : checked(selected.directory,
                                                 {"config", "--no-includes", "--null", "--file",
                                                  ".gitmodules", "--get-regexp", "^submodule\\..*\\.path$"},
                                                 true);
        auto entries = parseGitSubmodules(configuration, "");
        std::vector<std::string> indexArgs = {"ls-files", "--stage", "-z", "--"};
        for (const auto &entry : entries)
            indexArgs.push_back(entry.at("path"));
        const auto index = entries.empty() ? std::string{} : checked(selected.directory, indexArgs);
        entries = parseGitSubmodules(configuration, index);
        // Includes effective local/worktree/global settings without exposing URL credentials.
        const auto settings = checked(selected.directory, {"config", "--null", "--list"});
        std::map<std::string, std::string> registered;
        size_t at = 0;
        while (at < settings.size()) {
            const auto end = settings.find('\0', at), separator = settings.find('\n', at);
            if (end == std::string::npos)
                throw std::runtime_error("Invalid Git configuration records");
            if (separator == std::string::npos || separator > end)
                registered[settings.substr(at, end - at)] = "";
            else
                registered[settings.substr(at, separator - at)] =
                    settings.substr(separator + 1, end - separator - 1);
            at = end + 1;
        }
        for (auto &entry : entries) {
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error(
                    "Submodule catalogue exceeded 30 seconds; select a smaller repository");
            const auto path = entry.at("path").get<std::string>();
            auto target = selected.directory;
            // Check every lexical component before canonical resolution can hide redirects.
            for (const auto &part : utf8Path(path)) {
                target /= part;
                ordinary(target);
                if (fs::exists(target))
                    (void)directoryIdentity(target);
            }
            target = target.lexically_normal();
            if (!containsPath(root_, target) || target == selected.directory)
                throw std::runtime_error("Submodule path leaves its workspace");
            entry["root"] = pathUtf8(target.lexically_relative(root_));
            entry["directoryIdentity"] = fs::is_directory(target) ? directoryIdentity(target) : std::string{};
            entry["repositoryIdentity"] = "";
            entry["head"] = "";
            entry["branch"] = "";
            entry["dirty"] = nullptr;
            entry["checkedOut"] = false;
            entry["error"] = "";
            entry["registered"] =
                !registered["submodule." + entry.at("name").get<std::string>() + ".url"].empty();
            if (entry.at("state") != "uninitialized")
                continue;
            try {
                if (!fs::exists(target))
                    continue;
                if (!fs::is_directory(target))
                    throw std::runtime_error("Submodule destination is not a directory");
                const auto child = gitRepositoryAt(root_, target, false);
                if (!child.marker) {
                    if (!fs::is_empty(target))
                        throw std::runtime_error("Uninitialized submodule folder is not empty");
                    continue;
                }
                const auto owner = trimmed(checked(target, {"rev-parse", "--show-toplevel"}));
                if (!fs::equivalent(utf8Path(owner), target))
                    throw std::runtime_error("Git reports another submodule root");
                entry["checkedOut"] = true;
                const auto admin =
                    fs::canonical(utf8Path(trimmed(checked(target, {"rev-parse", "--absolute-git-dir"}))));
                entry["repositoryIdentity"] =
                    RecoveryStore::fingerprint(child.identity + "\n" + directoryIdentity(admin) + "\n" +
                                               checked(target, {"config", "--null", "--list"}));
                const auto head = trimmed(checked(target, {"rev-parse", "--verify", "HEAD"}));
                const auto branch = trimmed(checked(target, {"symbolic-ref", "-q", "--short", "HEAD"}, true));
                if ((head.size() != 40 && head.size() != 64) ||
                    head.find_first_not_of("0123456789abcdef") != std::string::npos || branch.size() > 512 ||
                    branch.find('\0') != std::string::npos)
                    throw std::runtime_error("Invalid or oversized submodule HEAD/branch");
                entry["head"] = head;
                entry["branch"] = branch;
                entry["dirty"] = !checked(target, {"status", "--porcelain=v1", "-z", "--untracked-files=all",
                                                   "--ignore-submodules=none"})
                                      .empty();
                entry["state"] = entry.at("head") == entry.at("expectedCommit") ? "matching" : "different";
            } catch (const std::exception &error) {
                entry["state"] = "unavailable";
                entry["error"] = std::string(error.what()).size() <= 8192
                                     ? error.what()
                                     : "Submodule inspection error exceeds 8192 bytes";
            }
        }
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Submodule catalogue exceeded 30 seconds");
        if (bytes != moduleBytes() ||
            gitRepositoryAt(root_, selected.directory, false).identity != selected.identity)
            throw std::runtime_error("Submodule configuration or repository changed while inspecting");
        inspectionDeadline = {};
        return Json{
            {"repository", selected.root},
            {"identity", selected.identity},
            {"revision", RecoveryStore::fingerprint(selected.identity + "\n" + bytes + "\n" + configuration +
                                                    "\n" + index + "\n" + settings + "\n" + entries.dump())},
            {"entries", entries}};
    };
    const auto before = catalogue();
    if (inspection)
        return before;
    if (params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Submodule state changed; refresh and review the action again");
    if (adding) {
        const auto &model = addModel;
        const auto request = model.at("request");
        const auto path = request.at("path").get<std::string>(), name = request.at("name").get<std::string>();
        const auto target = (selected.directory / utf8Path(path)).lexically_normal();
        if (!containsPath(root_, target) || target == selected.directory || before.at("entries").size() >= 64)
            throw std::runtime_error(
                "Submodule destination is outside the workspace or the catalogue is full");
        for (const auto &entry : before.at("entries"))
            if (overlaps(path, entry.at("path")) || overlaps(name, entry.at("name")))
                throw std::runtime_error("Submodule destination/name overlaps an existing module");
        auto documentsClosed = [&] {
            std::lock_guard lock(mutex_);
            for (const auto &[key, doc] : documents_) {
                if (isUntitledDocument(key))
                    continue;
                const auto full = resolve(key);
                if (full == modules || containsPath(target, full))
                    throw std::runtime_error(
                        "Close .gitmodules and destination documents before reviewing or adding a submodule");
            }
        };
        auto plan = [&] {
            inspectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            const auto parents = newTargetParents(target, true);
            if (gitFileArgument(root_, selected, target, true) != path)
                throw std::runtime_error("Destination belongs to another Git repository");
            if (!checked(selected.directory, {"ls-files", "--stage", "-z", "--", path}).empty())
                throw std::runtime_error("Destination is already tracked in the parent index");
            const auto ignored =
                git(selected.directory, {"check-ignore", "--quiet", "--no-index", "--", path}, 15, false);
            if (ignored.exitCode != 1 || ignored.timedOut || ignored.truncated)
                throw std::runtime_error("Destination is ignored or Git could not check its ignore status");
            const auto moduleIndex =
                checked(selected.directory, {"ls-files", "--stage", "-z", "--", ".gitmodules"});
            if ((fs::exists(modules) && moduleIndex.empty()) ||
                !checked(selected.directory,
                         {"status", "--porcelain=v1", "-z", "--untracked-files=all", "--", ".gitmodules"})
                     .empty())
                throw std::runtime_error(
                    "Commit or reconcile existing .gitmodules changes before adding a module");
            const auto bytes = moduleBytes();
            std::string indexedBytes;
            if (fs::exists(modules)) {
                if ((!moduleIndex.starts_with("100644 ") && !moduleIndex.starts_with("100755 ")) ||
                    moduleIndex.find('\0') != moduleIndex.size() - 1 ||
                    !moduleIndex.ends_with(std::string(" 0\t.gitmodules") + char(0)))
                    throw std::runtime_error(".gitmodules must be a normal stage-zero indexed file");
                if (checked(selected.directory, {"ls-files", "-v", "-z", "--", ".gitmodules"}) !=
                    std::string("H .gitmodules") + char(0))
                    throw std::runtime_error(
                        "Clear skip-worktree/assume-unchanged flags on .gitmodules before adding");
                indexedBytes = checked(selected.directory, {"show", "--no-ext-diff", "--no-textconv",
                                                            "--no-color", ":0:.gitmodules"});
                if (gitmodulesLines(bytes) != gitmodulesLines(indexedBytes))
                    throw std::runtime_error(
                        ".gitmodules differs from its indexed content; reconcile it before adding");
            }
            if (bytes.size() + 128 + 2 * (request.dump().size()) > 1024 * 1024)
                throw std::runtime_error("Adding this module would exceed the .gitmodules size limit");
            // Do not silently reuse orphan URL/branch/update configuration sections.
            auto noName = [&](const std::vector<std::string> &args) {
                const auto keys = checked(selected.directory, args, true);
                size_t at = 0;
                while (at < keys.size()) {
                    const auto end = keys.find('\0', at);
                    if (end == std::string::npos)
                        throw std::runtime_error("Invalid submodule configuration key records");
                    const auto key = keys.substr(at, end - at);
                    const auto last = key.rfind('.');
                    if (key.starts_with("submodule.") && last > 10 &&
                        folded(key.substr(10, last - 10)) == folded(name))
                        throw std::runtime_error(
                            "Module name already has Git configuration; choose another name");
                    at = end + 1;
                }
            };
            noName({"config", "--null", "--name-only", "--get-regexp", "^submodule\\."});
            if (!bytes.empty())
                noName({"config", "--no-includes", "--null", "--file", ".gitmodules", "--name-only",
                        "--get-regexp", "^submodule\\."});
            const auto branch = request.at("branch").get<std::string>();
            if (!branch.empty())
                (void)checked(selected.directory, {"check-ref-format", "--branch", branch});
            // Relative URLs depend on the parent's active branch/default remote.
            const auto parentHead =
                trimmed(checked(selected.directory, {"rev-parse", "--verify", "--quiet", "HEAD"}, true));
            const auto parentReference =
                trimmed(checked(selected.directory, {"symbolic-ref", "-q", "HEAD"}, true));
            if ((!parentHead.empty() &&
                 ((parentHead.size() != 40 && parentHead.size() != 64) ||
                  parentHead.find_first_not_of("0123456789abcdef") != std::string::npos)) ||
                parentReference.size() > 8192 || parentReference.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid parent Git HEAD/reference");
            auto metadataRoot =
                utf8Path(trimmed(checked(selected.directory, {"rev-parse", "--git-path", "modules"})));
            if (metadataRoot.is_relative())
                metadataRoot = selected.directory / metadataRoot;
            metadataRoot = metadataRoot.lexically_normal();
            const auto metadataTarget = (metadataRoot / utf8Path(name)).lexically_normal();
            const auto metadataParents = newTargetParents(metadataTarget, false);
            for (auto parent = metadataTarget.parent_path();
                 parent != metadataRoot && containsPath(metadataRoot, parent); parent = parent.parent_path())
                if (fs::exists(parent / "HEAD"))
                    throw std::runtime_error("Module name enters an existing Git metadata repository");
            documentsClosed();
            inspectionDeadline = {};
            return Json{{"repository", selected.root},
                        {"identity", selected.identity},
                        {"catalogRevision", before.at("revision")},
                        {"revision", RecoveryStore::fingerprint(
                                         before.dump() + "\n" + model.dump() + "\n" + parents.dump() + "\n" +
                                         metadataParents.dump() + "\n" + moduleIndex + "\n" + bytes + "\n" +
                                         indexedBytes + "\n" + parentHead + "\n" + parentReference)},
                        {"request", request},
                        {"root", pathUtf8(target.lexically_relative(root_))},
                        {"arguments", model.at("arguments")},
                        {"createsCheckout", true},
                        {"stagesGitmodules", true},
                        {"stagesGitlink", true},
                        {"mayContactRemote", true},
                        {"remoteCommit", nullptr}};
        };
        const auto reviewed = plan();
        if (action == "addPlan")
            return reviewed;
        if (!params.contains("expectedPlanRevision") ||
            params.at("expectedPlanRevision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule add preview changed; review it again before cloning");
        if (catalogue().at("revision") != before.at("revision") ||
            plan().at("revision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule repository/destination changed before adding");
        documentsClosed();
        const auto result =
            git(selected.directory, model.at("arguments").get<std::vector<std::string>>(), 180);
        Json output = {{"exitCode", result.exitCode},
                       {"output", processOutputText(result.output, true)},
                       {"timedOut", result.timedOut},
                       {"truncated", result.truncated}};
        try {
            output["catalog"] = catalogue();
        } catch (const std::exception &error) {
            output["catalogError"] = error.what();
        }
        monitor_.invalidate();
        return output;
    }
    const auto path = params.at("path").get<std::string>();
    const auto found = std::find_if(before.at("entries").begin(), before.at("entries").end(),
                                    [&](const Json &e) { return e.at("path") == path; });
    if (found == before.at("entries").end())
        throw std::runtime_error("Select an actual configured submodule");
    const auto state = found->at("state").get<std::string>();
    if (state == "conflicted" || state == "notIndexed" || state == "unavailable")
        throw std::runtime_error("Resolve the submodule index/path problem before changing it");
    if (configuring) {
        const auto &request = settingsModel.at("request");
        const auto setting = request.at("setting").get<std::string>(),
                   value = request.at("value").get<std::string>();
        const auto key = "submodule." + found->at("name").get<std::string>() + "." + setting;
        const auto target = selected.directory / utf8Path(path);
        auto documentsClosed = [&] {
            std::lock_guard lock(mutex_);
            for (const auto &[name, doc] : documents_)
                if (!isUntitledDocument(name)) {
                    const auto full = resolve(name);
                    if (full == modules || (setting == "url" && containsPath(target, full)))
                        throw std::runtime_error(
                            "Close .gitmodules and URL-target submodule documents before editing settings");
                }
        };
        auto configured = [&] {
            const auto records = checked(
                selected.directory,
                {"config", "--no-includes", "--null", "--file", ".gitmodules", "--get-all", key}, true);
            if (!records.empty() && (records.back() != '\0' || records.find('\0') != records.size() - 1))
                throw std::runtime_error("Resolve duplicate submodule settings before editing");
            return records;
        };
        std::string reviewedIndex, reviewedRemainder, reviewedPointer;
        auto plan = [&] {
            inspectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            documentsClosed();
            const auto bytes = moduleBytes();
            const auto index =
                checked(selected.directory, {"ls-files", "--stage", "-z", "--", ".gitmodules"});
            const auto flags = checked(selected.directory, {"ls-files", "-v", "-z", "--", ".gitmodules"});
            if ((!index.starts_with("100644 ") && !index.starts_with("100755 ")) ||
                index.find('\0') != index.size() - 1 ||
                !index.ends_with(std::string(" 0\t.gitmodules") + char(0)) ||
                flags != std::string("H .gitmodules") + char(0))
                throw std::runtime_error("Keep .gitmodules normally tracked without conflicts or hidden "
                                         "index flags before editing settings");
            const auto indexed = checked(selected.directory, {"show", "--no-ext-diff", "--no-textconv",
                                                              "--no-color", ":0:.gitmodules"});
            if (bytes.size() + 128 + 2 * request.dump().size() > 1024 * 1024)
                throw std::runtime_error("Editing this setting would exceed the .gitmodules size limit");
            const auto configuration = checked(
                selected.directory, {"config", "--no-includes", "--null", "--file", ".gitmodules", "--list"});
            const auto remainder = gitSubmoduleSettingsRemainder(configuration, key);
            const auto previous = configured();
            Json previousValue = nullptr;
            if (!previous.empty()) {
                const auto oldValue = previous.substr(0, previous.size() - 1);
                try {
                    auto oldRequest = request;
                    oldRequest["value"] = oldValue;
                    (void)gitSubmoduleSettingsModel(oldRequest);
                    if (setting == "branch" && !oldValue.empty() && oldValue != ".")
                        (void)checked(selected.directory, {"check-ref-format", "--branch", oldValue});
                    previousValue = oldValue;
                } catch (const std::exception &) { /* Keep credentials and unsupported old values private. */
                }
            }
            if (setting == "branch" && !value.empty() && value != ".")
                (void)checked(selected.directory, {"check-ref-format", "--branch", value});
            const bool localOverride =
                setting == "branch" &&
                !checked(selected.directory, {"config", "--null", "--get-all", key}, true).empty();
            Json directories = existingDirectories(selected.directory);
            std::string pointer;
            if (setting == "url") {
                auto config =
                    utf8Path(trimmed(checked(selected.directory, {"rev-parse", "--git-path", "config"})));
                if (config.is_relative())
                    config = selected.directory / config;
                directories.push_back(existingDirectories(config.parent_path().lexically_normal()));
                ordinary(config);
                if (!fs::is_regular_file(config))
                    throw std::runtime_error("Parent Git config must be an ordinary file");
                if (found->at("checkedOut") == true) {
                    const auto marker = target / ".git";
                    ordinary(marker);
                    if (!fs::is_regular_file(marker) || fs::file_size(marker) > 1024 * 1024)
                        throw std::runtime_error(
                            "URL synchronization requires an absorbed submodule pointer");
                    pointer = readTextFileBytes(marker);
                    const auto name = found->at("name").get<std::string>();
                    if (!checked(selected.directory,
                                 {"config", "--null", "--get-all", "submodule." + name + ".gitdir"}, true)
                             .empty())
                        throw std::runtime_error("Custom submodule Git directory mappings are unsupported "
                                                 "for URL synchronization");
                    // Reuse the ordinary logical-name/path validation without adopting metadata.
                    (void)gitSubmoduleArguments("init", name);
                    auto metadata = utf8Path(
                        trimmed(checked(selected.directory, {"rev-parse", "--git-path", "modules"})));
                    if (metadata.is_relative())
                        metadata = selected.directory / metadata;
                    metadata = (metadata / utf8Path(name)).lexically_normal();
                    directories.push_back(existingDirectories(metadata));
                    const auto admin =
                        utf8Path(trimmed(checked(target, {"rev-parse", "--absolute-git-dir"})));
                    if (!fs::equivalent(admin, metadata))
                        throw std::runtime_error("Submodule URL metadata belongs to another repository");
                    ordinary(metadata / "config");
                    if (!fs::is_regular_file(metadata / "config"))
                        throw std::runtime_error("Child Git config must be an ordinary file");
                    const auto owner =
                        trimmed(checked(target, {"rev-parse", "--show-superproject-working-tree"}));
                    if (owner.empty() || !fs::equivalent(utf8Path(owner), selected.directory))
                        throw std::runtime_error("Submodule belongs to another superproject");
                }
            }
            const auto parentHead =
                trimmed(checked(selected.directory, {"rev-parse", "--verify", "--quiet", "HEAD"}, true));
            const auto parentRef = trimmed(checked(selected.directory, {"symbolic-ref", "-q", "HEAD"}, true));
            const bool already =
                setting == "branch" && (value.empty() ? previous.empty() : previous == value + char(0));
            if (bytes != moduleBytes() || std::chrono::steady_clock::now() > inspectionDeadline)
                throw std::runtime_error(
                    "Submodule metadata changed or settings inspection exceeded 30 seconds");
            reviewedIndex = index;
            reviewedRemainder = remainder;
            reviewedPointer = pointer;
            inspectionDeadline = {};
            return Json{{"repository", selected.root},
                        {"identity", selected.identity},
                        {"catalogRevision", before.at("revision")},
                        {"revision", RecoveryStore::fingerprint(before.dump() + "\n" + settingsModel.dump() +
                                                                "\n" + bytes + "\n" + index + "\n" + indexed +
                                                                "\n" + directories.dump() + "\n" + pointer +
                                                                "\n" + parentHead + "\n" + parentRef)},
                        {"request", request},
                        {"entry", *found},
                        {"previousValue", previousValue},
                        {"previousRedacted", !previous.empty() && previousValue.is_null()},
                        {"hasConfiguredValue", !previous.empty()},
                        {"hasLocalBranchOverride", localOverride},
                        {"alreadyConfigured", already},
                        {"arguments", settingsModel.at("arguments")},
                        {"writesGitmodules", !already},
                        {"maySynchronizeLocalUrl", setting == "url"},
                        {"stagesGitmodules", false},
                        {"changesCheckout", false},
                        {"mayContactRemote", false}};
        };
        const auto reviewed = plan();
        if (action == "settingsPlan")
            return reviewed;
        if (!params.contains("expectedPlanRevision") ||
            params.at("expectedPlanRevision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule settings preview changed; review it again");
        if (reviewed.at("alreadyConfigured") == true)
            throw std::runtime_error("Tracking branch already has the requested configuration");
        if (catalogue().at("revision") != before.at("revision") ||
            plan().at("revision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule state changed before editing settings");
        documentsClosed();
        const auto result = git(selected.directory,
                                settingsModel.at("arguments").get<std::vector<std::string>>(), 180, false);
        Json output = {{"exitCode", result.exitCode},
                       {"output", processOutputText(result.output, true)},
                       {"timedOut", result.timedOut},
                       {"truncated", result.truncated}};
        try {
            output["catalog"] = catalogue();
            const auto desired = setting == "branch" && value.empty() ? std::string{} : value + char(0);
            if (configured() != desired || checked(selected.directory, {"ls-files", "--stage", "-z", "--",
                                                                        ".gitmodules"}) != reviewedIndex)
                throw std::runtime_error(
                    "Submodule settings/index outcome was not confirmed; inspect the actual metadata");
            if (gitSubmoduleSettingsRemainder(
                    checked(selected.directory,
                            {"config", "--no-includes", "--null", "--file", ".gitmodules", "--list"}),
                    key) != reviewedRemainder)
                throw std::runtime_error(
                    "Unrelated .gitmodules settings changed; inspect the actual metadata");
            const auto &entries = output.at("catalog").at("entries");
            const auto after = std::find_if(entries.begin(), entries.end(),
                                            [&](const Json &e) { return e.at("path") == path; });
            if (after == entries.end() || after->at("expectedCommit") != found->at("expectedCommit") ||
                after->at("head") != found->at("head") ||
                after->at("checkedOut") != found->at("checkedOut") ||
                after->at("directoryIdentity") != found->at("directoryIdentity"))
                throw std::runtime_error("Submodule checkout changed while editing settings");
            if (setting == "url" && found->at("checkedOut") == true) {
                ordinary(target / ".git");
                if (readTextFileBytes(target / ".git") != reviewedPointer)
                    throw std::runtime_error("Submodule Git pointer changed while editing URL settings");
            }
        } catch (const std::exception &error) {
            output["catalogError"] = error.what();
        }
        monitor_.invalidate();
        return output;
    }
    if (deinitializing) {
        const auto target = selected.directory / utf8Path(path);
        fs::path retainedAdmin;
        std::string retainedIdentity, retainedModules;
        auto documentsClosed = [&] {
            std::lock_guard lock(mutex_);
            for (const auto &[key, doc] : documents_)
                if (!isUntitledDocument(key)) {
                    const auto full = resolve(key);
                    if (full == modules || containsPath(target, full))
                        throw std::runtime_error(
                            "Close .gitmodules and all submodule documents before deinitializing");
                }
        };
        auto plan = [&] {
            inspectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            const auto index = checked(target, {"ls-files", "--stage", "-z"});
            const auto flags = checked(target, {"ls-files", "-v", "-z"});
            const auto model = gitSubmoduleDeinitModel(*found, index, flags);
            if (!checked(selected.directory,
                         {"config", "--null", "--get-all",
                          "submodule." + found->at("name").get<std::string>() + ".gitdir"},
                         true)
                     .empty())
                throw std::runtime_error(
                    "Custom submodule Git directory mappings are unsupported for deinitialization");
            const auto marker = target / ".git";
            ordinary(marker);
            if (!fs::is_regular_file(marker) || fs::file_size(marker) > 1024 * 1024)
                throw std::runtime_error("Deinitialization requires an absorbed submodule .git pointer file");
            const auto markerBytes = readTextFileBytes(marker);
            auto metadataRoot =
                utf8Path(trimmed(checked(selected.directory, {"rev-parse", "--git-path", "modules"})));
            if (metadataRoot.is_relative())
                metadataRoot = selected.directory / metadataRoot;
            const auto expectedAdmin = (metadataRoot / utf8Path(found->at("name"))).lexically_normal();
            const auto admin =
                utf8Path(trimmed(checked(target, {"rev-parse", "--absolute-git-dir"}))).lexically_normal();
            const auto directories = existingDirectories(expectedAdmin);
            if (containsPath(target, admin) || !fs::equivalent(admin, expectedAdmin))
                throw std::runtime_error("Submodule metadata must remain in its parent repository");
            ordinary(expectedAdmin / "config");
            if (!fs::is_regular_file(expectedAdmin / "config"))
                throw std::runtime_error("Submodule metadata config must be an ordinary file");
            auto parentConfig =
                utf8Path(trimmed(checked(selected.directory, {"rev-parse", "--git-path", "config"})));
            if (parentConfig.is_relative())
                parentConfig = selected.directory / parentConfig;
            const auto parentDirectories = existingDirectories(parentConfig.parent_path().lexically_normal());
            ordinary(parentConfig);
            if (!fs::is_regular_file(parentConfig))
                throw std::runtime_error("Parent repository config must be an ordinary file");
            const auto owner = trimmed(checked(target, {"rev-parse", "--show-superproject-working-tree"}));
            if (owner.empty() || !fs::equivalent(utf8Path(owner), selected.directory))
                throw std::runtime_error("Submodule belongs to another superproject");
            if (!checked(target, {"-c", "core.fsmonitor=false", "-c", "core.untrackedCache=false", "-c",
                                  "core.ignorestat=false", "status", "--porcelain=v1", "-z",
                                  "--untracked-files=all", "--ignored=matching", "--ignore-submodules=none"})
                     .empty())
                throw std::runtime_error(
                    "Preserve modified, untracked and ignored submodule files before deinitializing");
            const auto moduleIndex =
                checked(selected.directory, {"ls-files", "--stage", "-z", "--", ".gitmodules"});
            const auto moduleFlags =
                checked(selected.directory, {"ls-files", "-v", "-z", "--", ".gitmodules"});
            if (moduleIndex.empty() || moduleFlags != std::string("H .gitmodules\0", 14) ||
                !checked(selected.directory, {"status", "--porcelain=v1", "-z", "--", ".gitmodules"}).empty())
                throw std::runtime_error(
                    "Keep .gitmodules normally tracked and unchanged before deinitializing");
            const auto bytes = moduleBytes();
            const auto indexedBytes = checked(selected.directory, {"show", "--no-ext-diff", "--no-textconv",
                                                                   "--no-color", ":0:.gitmodules"});
            if (gitmodulesLines(bytes) != gitmodulesLines(indexedBytes))
                throw std::runtime_error("Save and reconcile .gitmodules before deinitializing");
            retainedAdmin = expectedAdmin;
            retainedIdentity = directoryIdentity(expectedAdmin);
            retainedModules = bytes;
            documentsClosed();
            if (std::chrono::steady_clock::now() > inspectionDeadline)
                throw std::runtime_error("Submodule deinitialization preflight exceeded 30 seconds");
            inspectionDeadline = {};
            return Json{{"repository", selected.root},
                        {"identity", selected.identity},
                        {"catalogRevision", before.at("revision")},
                        {"revision", RecoveryStore::fingerprint(
                                         before.dump() + "\n" + index + "\n" + flags + "\n" + markerBytes +
                                         "\n" + directories.dump() + "\n" + parentDirectories.dump() + "\n" +
                                         moduleIndex + "\n" + bytes + "\n" + indexedBytes)},
                        {"entry", *found},
                        {"arguments", model.at("arguments")},
                        {"trackedFiles", model.at("trackedFiles")},
                        {"removesCheckout", true},
                        {"unregistersLocally", true},
                        {"preservesGitlink", true},
                        {"preservesGitmodules", true},
                        {"preservesRepository", true},
                        {"mayContactRemote", false}};
        };
        const auto reviewed = plan();
        if (action == "deinitPlan")
            return reviewed;
        if (!params.contains("expectedPlanRevision") ||
            params.at("expectedPlanRevision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule deinitialization preview changed; review it again");
        if (catalogue().at("revision") != before.at("revision") ||
            plan().at("revision") != reviewed.at("revision"))
            throw std::runtime_error("Submodule state changed before deinitializing");
        documentsClosed();
        const auto result =
            git(selected.directory, reviewed.at("arguments").get<std::vector<std::string>>(), 180, false);
        Json output = {{"exitCode", result.exitCode},
                       {"output", processOutputText(result.output, true)},
                       {"timedOut", result.timedOut},
                       {"truncated", result.truncated}};
        try {
            output["catalog"] = catalogue();
        } catch (const std::exception &error) {
            output["catalogError"] = error.what();
        }
        try {
            ordinary(retainedAdmin);
            if (directoryIdentity(retainedAdmin) != retainedIdentity || moduleBytes() != retainedModules)
                throw std::runtime_error(
                    "Retained repository or .gitmodules changed during deinitialization");
        } catch (const std::exception &error) {
            output["catalogError"] = error.what();
        }
        // Git may report successful dispatch even when clearing a directory failed.
        if (output.contains("catalog")) {
            const auto &entries = output.at("catalog").at("entries");
            const auto after = std::find_if(entries.begin(), entries.end(),
                                            [&](const Json &e) { return e.at("path") == path; });
            if (after == entries.end() || after->at("registered") != false ||
                after->at("checkedOut") != false || after->at("state") != "uninitialized" ||
                after->at("expectedCommit") != found->at("expectedCommit"))
                output["catalogError"] = "Submodule deinitialization was not confirmed; inspect the actual "
                                         "checkout and configuration";
        }
        monitor_.invalidate();
        return output;
    }
    if (action == "sync" && !found->at("registered").get<bool>())
        throw std::runtime_error("Register the submodule before synchronizing its URL");
    if (action == "update" && found->at("dirty") == true)
        throw std::runtime_error("Commit or stash submodule modifications before updating");
    if (action == "update" && !params.value("fetch", false) && !found->at("checkedOut").get<bool>())
        throw std::runtime_error("Allow clone/fetch to populate an uninitialized submodule");
    const auto args = gitSubmoduleArguments(action, path, params.value("fetch", false));
    if (catalogue().at("revision") != before.at("revision"))
        throw std::runtime_error("Submodule state changed before commit; review it again");
    {
        std::lock_guard lock(mutex_);
        for (const auto &[key, doc] : documents_) {
            if (isUntitledDocument(key))
                continue;
            const auto full = resolve(key);
            if (full == modules &&
                (doc.dirty || !doc.diskConflict.empty() || readTextFileBytes(full) != doc.disk))
                throw std::runtime_error("Save and reconcile .gitmodules before acting");
            if (action == "update" && containsPath(selected.directory / utf8Path(path), full))
                throw std::runtime_error("Close all documents in the selected submodule before updating");
        }
    }
    const auto result =
        git(selected.directory, args, 180, action != "update" || params.value("fetch", false));
    Json output = {{"exitCode", result.exitCode},
                   {"output", processOutputText(result.output, true)},
                   {"timedOut", result.timedOut},
                   {"truncated", result.truncated}};
    try {
        output["catalog"] = catalogue();
    } catch (const std::exception &error) {
        output["catalogError"] = error.what();
    }
    monitor_.invalidate();
    return output;
}
} // namespace cycode

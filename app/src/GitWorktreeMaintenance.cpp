#include "cycode/ProcessText.h"
#include "cycode/Engine.h"
#include "cycode/FileTransactions.h"
#include "cycode/GitSafety.h"
#include "cycode/TextEdits.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
namespace fs = std::filesystem;
std::string line(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    return text;
}
void ordinary(const fs::path &path) {
    if (fs::is_symlink(fs::symlink_status(path)))
        throw std::runtime_error("Redirected worktree paths are unsupported");
#ifdef _WIN32
    const auto flags = GetFileAttributesW(path.c_str());
    if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Redirected worktree paths are unsupported");
#endif
}
Json directories(fs::path path) {
    Json result = Json::array();
    for (size_t depth = 0; !path.empty(); ++depth) {
        if (depth >= 256)
            throw std::runtime_error("Worktree directory depth exceeds 256");
        ordinary(path);
        result.push_back({{"path", pathUtf8(path)}, {"identity", directoryIdentity(path)}});
        const auto parent = path.parent_path();
        if (parent == path)
            break;
        path = parent;
    }
    return result;
}
Json missingParents(fs::path path) {
    Json result = Json::array();
    for (size_t depth = 0; !fs::exists(path); ++depth) {
        if (depth >= 256 || path.empty() || path.parent_path() == path)
            throw std::runtime_error("Missing worktree path exceeds its parent limit");
        ordinary(path);
        result.push_back({{"path", pathUtf8(path)}, {"identity", ""}});
        path = path.parent_path();
    }
    result.push_back(directories(path));
    return result;
}
std::string bytes(const fs::path &file, size_t limit, bool optional = false) {
    ordinary(file);
    if (optional && !fs::exists(file))
        return {};
    if (!fs::is_regular_file(file) || fs::file_size(file) > limit)
        throw std::runtime_error("Worktree metadata is missing, redirected or exceeds its size limit");
    const auto size = fs::file_size(file);
    if (size > limit)
        throw std::runtime_error("Worktree metadata grew beyond its limit");
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot read worktree metadata");
    std::string result(static_cast<size_t>(size) + 1, '\0');
    stream.read(result.data(), static_cast<std::streamsize>(result.size()));
    if (stream.bad() || stream.gcount() != static_cast<std::streamsize>(size))
        throw std::runtime_error("Worktree metadata changed while reading");
    result.resize(static_cast<size_t>(size));
    return result;
}
fs::path reference(const std::string &text, const fs::path &base, bool marker) {
    auto value = line(text);
    if (marker) {
        if (!value.starts_with("gitdir: "))
            throw std::runtime_error("Invalid worktree Git pointer");
        value.erase(0, 8);
    }
    if (value.empty() || value.find_first_of("\r\n") != std::string::npos ||
        value.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid worktree metadata reference");
    auto path = utf8Path(value);
    path = (path.is_absolute() ? path : base / path).lexically_normal();
    // ../.. is a directory reference and can retain a trailing empty component.
    // Strip that spelling difference without resolving links or changing roots.
    while (path.filename().empty() && path.parent_path() != path)
        path = path.parent_path();
    return path;
}
} // namespace
Json gitWorktreeMaintenanceRequest(const Json &parameters) {
    if (!parameters.is_object() || parameters.dump().size() > 16384)
        throw std::runtime_error("Worktree request exceeds 16 KiB");
    const auto action = parameters.at("action").get<std::string>();
    if (action != "move" && action != "repair")
        throw std::runtime_error("Unsupported worktree maintenance action");
    Json request = {{"action", action}};
    for (const auto *key : {"path", "destination"}) {
        const auto value = parameters.at(key).get<std::string>();
        if (value.empty() || value.size() > 4096 || value.front() == '/' ||
            value.find_first_of("\\:") != std::string::npos ||
            std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("Use ordinary workspace-relative worktree paths");
        size_t at = 0;
        while (at < value.size()) {
            const auto end = value.find('/', at);
            auto part = value.substr(at, end == std::string::npos ? end : end - at);
            std::transform(part.begin(), part.end(), part.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (part.empty() || part == "." || part == ".." || part == ".git" || part == ".cycode" ||
                part.back() == '.' || part.back() == ' ')
                throw std::runtime_error("Worktree path crosses metadata or a nonportable component");
            if (end == std::string::npos)
                break;
            at = end + 1;
            if (at == value.size())
                throw std::runtime_error("Worktree path ends in a separator");
        }
        request[key] = value;
    }
    if (request.at("path") == request.at("destination"))
        throw std::runtime_error("Choose a different worktree destination");
    return request;
}
Json Engine::gitWorktreeMaintenance(const std::string &method, const Json &params) {
    const bool preview = method == "vcs.worktree.plan";
    if (!params.is_object() || params.dump().size() > 16384)
        throw std::runtime_error("Worktree request exceeds 16 KiB");
    for (const auto &[key, value] : params.items())
        if (key != "repository" && key != "expectedRepository" && key != "expectedRevision" &&
            key != "path" && key != "destination" && key != "provider" && key != "contextPath" &&
            key != "root" && key != "workspace" && !(preview && key == "action") &&
            !(!preview && key == "expectedPlanRevision"))
            throw std::runtime_error("Unknown worktree maintenance field: " + key);
    if (!params.contains("expectedRepository") || !params.contains("expectedRevision"))
        throw std::runtime_error("Observe repository identity and worktree catalogue before maintenance");
    auto input = params;
    if (!preview)
        input["action"] = method == "vcs.worktree.move" ? "move" : "repair";
    const auto request = gitWorktreeMaintenanceRequest(input);
    const bool moving = request.at("action") == "move";
    const auto before = gitWorktrees("vcs.worktrees", params);
    if (params.at("expectedRevision") != before.at("revision"))
        throw std::runtime_error("Worktree catalogue changed; review again");
    const auto selected = gitSelection(params);
    const auto source = (root_ / utf8Path(request.at("path"))).lexically_normal();
    const auto destination = (root_ / utf8Path(request.at("destination"))).lexically_normal();
    const auto &entries = before.at("entries");
    auto found = std::find_if(entries.begin(), entries.end(),
                              [&](const Json &entry) { return entry.at("path") == pathUtf8(source); });
    if (found == entries.end() || found->at("primary") == true || found->at("current") == true ||
        found->at("bare") == true || found->at("insideWorkspace") != true || found->at("locked") == true ||
        containsPath(source, selected.directory) || containsPath(source, destination) ||
        containsPath(destination, source) || containsPath(destination, selected.directory))
        throw std::runtime_error(
            "Select another unlocked linked worktree and a separate workspace destination");
    for (const auto &entry : entries) {
        const auto other = utf8Path(entry.at("path"));
        if (other == destination || containsPath(destination, other) ||
            (entry.at("primary") != true && other != source &&
             (containsPath(other, destination) || containsPath(source, other))))
            throw std::runtime_error("Worktree paths overlap another registered checkout");
    }
    std::vector<fs::path> documentFolders{source, destination};
    auto noDocuments = [&] {
        std::lock_guard lock(mutex_);
        for (const auto &[name, doc] : documents_)
            if (!isUntitledDocument(name)) {
                const auto full = resolve(name);
                for (const auto &folder : documentFolders)
                    if (containsPath(folder, full))
                        throw std::runtime_error(
                            "Close documents in reviewed worktrees and Git metadata first");
            }
    };
    fs::path reviewedAdmin;
    Json reviewedSiblings = Json::array();
    std::string reviewedIndex, reviewedHead, payloadIdentity;
    auto plan = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
        auto git = [&](const fs::path &folder, std::vector<std::string> args) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
                throw std::runtime_error("Worktree maintenance inspection exceeded 45 seconds");
            args.insert(args.begin(), {"git", "--no-pager", "--no-optional-locks", "--literal-pathspecs",
                                       "-c", "worktree.useRelativePaths=false", "-C", pathUtf8(folder)});
            hardenGitArguments(args);
            const auto result = runProcess(
                args, folder, std::min(std::chrono::milliseconds(15000), remaining), 2 * 1024 * 1024, nullptr,
                {{"GIT_ALLOW_PROTOCOL", ":"}, {"GIT_TERMINAL_PROMPT", "0"}});
            if (result.exitCode || result.timedOut || result.truncated)
                throw std::runtime_error("Cannot inspect complete worktree maintenance state");
            return result.output;
        };
        noDocuments();
        Json identities = moving ? directories(source.parent_path()) : missingParents(source.parent_path());
        identities.push_back(directories(destination.parent_path()));
        ordinary(source);
        ordinary(destination);
        if (moving ? (!fs::is_directory(source) || fs::exists(fs::symlink_status(destination)))
                   : (fs::exists(fs::symlink_status(source)) || !fs::is_directory(destination)))
            throw std::runtime_error("Move needs an existing source/new destination; repair needs a missing "
                                     "source/moved destination");
        const auto payload = moving ? source : destination;
        identities.push_back(directories(payload));
        const auto common = utf8Path(line(git(selected.directory,
                                              {"rev-parse", "--path-format=absolute", "--git-common-dir"})))
                                .lexically_normal();
        identities.push_back(directories(common));
        const auto metadataFolder = common / "worktrees";
        if (std::find(documentFolders.begin(), documentFolders.end(), metadataFolder) ==
            documentFolders.end())
            documentFolders.push_back(metadataFolder);
        const auto marker = bytes(payload / ".git", 4096);
        const auto admin = reference(marker, payload, true);
        identities.push_back(directories(admin));
        if (!containsPath(common / "worktrees", admin) || admin.parent_path() != common / "worktrees")
            throw std::runtime_error("Worktree pointer does not belong to this repository's linked metadata");
        if (reference(bytes(admin / "commondir", 4096), admin, false) != common ||
            reference(bytes(admin / "gitdir", 32768), admin, false) != source / ".git")
            throw std::runtime_error("Worktree reciprocal metadata does not identify the reviewed source");
        if (!fs::equivalent(utf8Path(line(git(payload, {"rev-parse", "--show-toplevel"}))), payload))
            throw std::runtime_error("Git reports another worktree payload root");
        if (fs::exists(admin / "locked") || fs::exists(admin / "modules"))
            throw std::runtime_error(
                "Locked worktrees and submodule metadata are unsupported for maintenance");
        const auto modes = git(payload, {"ls-files", "--format=%(objectmode)", "-z"});
        size_t at = 0, count = 0;
        while (at < modes.size()) {
            const auto end = modes.find('\0', at);
            if (end == std::string::npos || ++count > 200000)
                throw std::runtime_error("Incomplete/oversized worktree index mode list");
            const auto mode = modes.substr(at, end - at);
            if (mode != "100644" && mode != "100755" && mode != "120000")
                throw std::runtime_error("Nested submodules or unsupported index modes block maintenance");
            at = end + 1;
        }
        const auto index = bytes(admin / "index", 64 * 1024 * 1024, true);
        const auto head = bytes(admin / "HEAD", 4096);
        size_t indexBudget = 64 * 1024 * 1024 - index.size();
        Json checkedRoots = Json::array({request.at("path"), request.at("destination")}),
             siblings = Json::array();
        if (!moving) {
            checkedRoots = Json::array({request.at("destination")});
            std::set<std::string> verified{pathUtf8(admin)};
            for (const auto &entry : entries) {
                if (entry.at("insideWorkspace") != true || entry.at("bare") == true)
                    throw std::runtime_error(
                        "Repair requires every registered worktree inside this workspace");
                const auto folder = utf8Path(entry.at("path"));
                if (folder == source)
                    continue;
                if (entry.at("available") != true)
                    throw std::runtime_error("Repair requires every other registered worktree available");
                identities.push_back(directories(folder));
                if (entry.at("primary") == true) {
                    if (entry.at("current") != true)
                        throw std::runtime_error("Request worktree repair from the main checkout");
                    ordinary(folder / ".git");
                    if (!fs::is_directory(folder / ".git") || !fs::equivalent(folder / ".git", common))
                        throw std::runtime_error("Repair requires an ordinary main checkout");
                    continue;
                }
                if (entry.at("locked") == true)
                    throw std::runtime_error("Unlock all linked worktrees before repair");
                (void)gitWorktreeMaintenanceRequest(
                    {{"action", "repair"}, {"path", request.at("path")}, {"destination", entry.at("root")}});
                const auto pointer = bytes(folder / ".git", 4096);
                const auto otherAdmin = reference(pointer, folder, true);
                identities.push_back(directories(otherAdmin));
                if (otherAdmin.parent_path() != common / "worktrees" ||
                    reference(bytes(otherAdmin / "gitdir", 32768), otherAdmin, false) != folder / ".git" ||
                    reference(bytes(otherAdmin / "commondir", 4096), otherAdmin, false) != common ||
                    fs::exists(otherAdmin / "locked"))
                    throw std::runtime_error("Repair found another broken or redirected worktree link");
                if (std::find(documentFolders.begin(), documentFolders.end(), folder) ==
                    documentFolders.end())
                    documentFolders.push_back(folder);
                verified.insert(pathUtf8(otherAdmin));
                const auto otherIndex = bytes(otherAdmin / "index", indexBudget, true);
                indexBudget -= otherIndex.size();
                siblings.push_back({{"root", pathUtf8(folder)},
                                    {"admin", pathUtf8(otherAdmin)},
                                    {"directoryIdentity", directoryIdentity(folder)},
                                    {"adminIdentity", directoryIdentity(otherAdmin)},
                                    {"head", bytes(otherAdmin / "HEAD", 4096)},
                                    {"index", RecoveryStore::fingerprint(otherIndex)},
                                    {"pointer", pointer},
                                    {"backlink", bytes(otherAdmin / "gitdir", 32768)}});
                checkedRoots.push_back(entry.at("root"));
            }
            size_t visited = 0;
            for (const auto &item : fs::directory_iterator(common / "worktrees")) {
                ordinary(item.path());
                if (++visited > 256 || !item.is_directory() ||
                    !verified.contains(pathUtf8(item.path().lexically_normal())))
                    throw std::runtime_error("Unreviewed/orphan worktree metadata blocks repair");
            }
        }
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Worktree maintenance inspection exceeded 45 seconds");
        noDocuments();
        reviewedAdmin = admin;
        reviewedIndex = index;
        reviewedHead = head;
        payloadIdentity = directoryIdentity(payload);
        reviewedSiblings = siblings;
        Json arguments =
            moving ? Json::array({"worktree", "move", "--", pathUtf8(source), pathUtf8(destination)})
                   : Json::array({"worktree", "repair", "--", pathUtf8(destination)});
        return Json{
            {"repository", before.at("repository")},
            {"identity", before.at("identity")},
            {"catalogRevision", before.at("revision")},
            {"revision", RecoveryStore::fingerprint(before.dump() + request.dump() + identities.dump() +
                                                    marker + index + head + siblings.dump())},
            {"request", request},
            {"entry", *found},
            {"arguments", arguments},
            {"checkedRoots", checkedRoots},
            {"movesDirectory", moving},
            {"repairsLinks", !moving},
            {"preservesLocalFiles", true},
            {"preservesIndex", true},
            {"mayContactRemote", false}};
    };
    const auto reviewed = plan();
    if (preview)
        return reviewed;
    if (!params.contains("expectedPlanRevision") ||
        params.at("expectedPlanRevision") != reviewed.at("revision"))
        throw std::runtime_error("Worktree maintenance preview changed; review again");
    if (gitWorktrees("vcs.worktrees", params).at("revision") != before.at("revision") ||
        plan().at("revision") != reviewed.at("revision"))
        throw std::runtime_error("Worktree maintenance state changed before dispatch");
    noDocuments();
    auto args = reviewed.at("arguments").get<std::vector<std::string>>();
    args.insert(args.begin(), {"git", "--no-pager", "--no-optional-locks", "--literal-pathspecs", "-c",
                               "worktree.useRelativePaths=false", "-C", pathUtf8(selected.directory)});
    hardenGitArguments(args);
    const auto result = runProcess(args, selected.directory, std::chrono::seconds(180), 2 * 1024 * 1024,
                                   nullptr, {{"GIT_ALLOW_PROTOCOL", ":"}, {"GIT_TERMINAL_PROMPT", "0"}});
    Json output = {{"exitCode", result.exitCode},
                   {"output", processOutputText(result.output, true)},
                   {"timedOut", result.timedOut},
                   {"truncated", result.truncated}};
    try {
        output["catalog"] = gitWorktrees("vcs.worktrees", params);
        ordinary(source);
        ordinary(destination);
        if (fs::exists(fs::symlink_status(source)) || directoryIdentity(destination) != payloadIdentity ||
            bytes(reviewedAdmin / "index", 64 * 1024 * 1024, true) != reviewedIndex ||
            bytes(reviewedAdmin / "HEAD", 4096) != reviewedHead ||
            reference(bytes(destination / ".git", 4096), destination, true) != reviewedAdmin ||
            reference(bytes(reviewedAdmin / "gitdir", 32768), reviewedAdmin, false) != destination / ".git")
            throw std::runtime_error(
                "Worktree maintenance outcome is unconfirmed; inspect actual directories and Git links");
        bool confirmed = false;
        for (const auto &entry : output.at("catalog").at("entries"))
            if (entry.at("path") == pathUtf8(destination) && entry.at("head") == found->at("head") &&
                entry.at("branch") == found->at("branch"))
                confirmed = true;
        if (!confirmed)
            throw std::runtime_error("Worktree destination was not confirmed in the fresh catalogue");
        for (const auto &sibling : reviewedSiblings) {
            const auto folder = utf8Path(sibling.at("root")), metadata = utf8Path(sibling.at("admin"));
            directories(folder);
            directories(metadata);
            if (reference(bytes(folder / ".git", 4096), folder, true) != metadata ||
                reference(bytes(metadata / "gitdir", 32768), metadata, false) != folder / ".git" ||
                Json(directoryIdentity(folder)) != sibling.at("directoryIdentity") ||
                Json(directoryIdentity(metadata)) != sibling.at("adminIdentity") ||
                Json(bytes(metadata / "HEAD", 4096)) != sibling.at("head") ||
                Json(RecoveryStore::fingerprint(bytes(metadata / "index", 64 * 1024 * 1024, true))) !=
                    sibling.at("index"))
                throw std::runtime_error("Another reviewed worktree link is unconfirmed after repair");
        }
    } catch (const std::exception &error) {
        output["catalogError"] = error.what();
    }
    monitor_.invalidate();
    return output;
}
} // namespace cycode

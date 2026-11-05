#include "MercurialClient.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace cycode::mercurial {
namespace {
namespace fs = std::filesystem;
constexpr size_t maxOutput = 512 * 1024;
bool inside(const fs::path &root, const fs::path &path) {
    auto a = root.begin(), b = path.begin();
    for (; a != root.end(); ++a, ++b) {
        if (b == path.end())
            return false;
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()))
            return false;
#else
        if (*a != *b)
            return false;
#endif
    }
    return true;
}
std::string relativePath(const fs::path &root, const fs::path &path) {
    if (!inside(root, path))
        throw std::runtime_error("Mercurial path is outside the selected root");
    auto part = path.begin();
    for (auto parent = root.begin(); parent != root.end(); ++parent)
        ++part;
    fs::path result;
    for (; part != path.end(); ++part)
        result /= *part;
    return result.empty() ? "." : pathUtf8(result);
}
std::string bounded(const Json &j, const char *key, size_t limit, const std::string &fallback = {}) {
    const auto value = j.value(key, fallback);
    if (value.size() > limit || value.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("Invalid Mercurial ") + key);
    return value;
}
fs::path local(const fs::path &root, const std::string &name) {
    const auto path = utf8Path(name);
    if (name.empty() || path.is_absolute() || path.has_root_name() || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos)
        throw std::runtime_error("Mercurial expects a workspace-relative local path");
    auto walk = root;
    for (const auto &part : path) {
        if (part == ".." ||
#ifdef _WIN32
            _wcsicmp(part.c_str(), L".hg") == 0)
#else
            part == ".hg")
#endif
            throw std::runtime_error("Mercurial path traverses a parent or administrative directory");
        if (part == ".")
            continue;
        walk /= part;
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(walk, error)))
            throw std::runtime_error("Mercurial operations through symbolic links are unsupported");
    }
    const auto canonical = fs::weakly_canonical(root / path);
    if (!inside(root, canonical))
        throw std::runtime_error("Mercurial path escaped its root");
    return canonical;
}
bool ok(const ProcessResult &r) {
    return r.exitCode == 0 && !r.timedOut && !r.truncated && !r.cancelled;
}
Json process(const ProcessResult &r) {
    const auto code = r.exitCode == 0 && (r.timedOut || r.truncated || r.cancelled) ? 124 : r.exitCode;
    return {{"provider", "cycode.mercurial.provider"},
            {"exitCode", code},
            {"output", r.output},
            {"timedOut", r.timedOut},
            {"truncated", r.truncated},
            {"cancelled", r.cancelled}};
}
Json json(const ProcessResult &r) {
    if (!ok(r) || r.output.size() > maxOutput || r.output.find('\0') != std::string::npos)
        throw std::runtime_error("Mercurial response is incomplete");
    return Json::parse(r.output, [](int depth, Json::parse_event_t, Json &) {
        if (depth > 16)
            throw std::runtime_error("Mercurial response nesting exceeds its limit");
        return true;
    });
}
Json array(const ProcessResult &r, size_t limit) {
    auto result = json(r);
    if (!result.is_array() || result.size() > limit)
        throw std::runtime_error("Invalid or excessive Mercurial record list");
    for (const auto &entry : result)
        if (!entry.is_object() || entry.size() > 128)
            throw std::runtime_error("Invalid Mercurial record");
    return result;
}
bool node(const std::string &value) {
    return value.size() == 40 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
           });
}
bool name(const std::string &value, bool reference) {
    if (reference && value == ".")
        return true;
    auto first = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };
    return !value.empty() && value.size() <= 128 && first(value.front()) &&
           std::all_of(value.begin(), value.end(),
                       [&](unsigned char c) {
                           return first(c) || c == '_' || c == '-' || c == '.' || (reference && c == '/');
                       }) &&
           value.find("..") == std::string::npos && value.find("//") == std::string::npos;
}
std::string trimLine(std::string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
        text.pop_back();
    return text;
}
Json files(const ProcessResult &r, const fs::path &workspace, const fs::path &root) {
    Json result = Json::array();
    for (const auto &entry : array(r, 20000)) {
        const auto state = bounded(entry, "status", 1);
        auto path = bounded(entry, "path", 8192);
#ifdef _WIN32
        // Mercurial may apply native separators to formatted status paths.
        std::replace(path.begin(), path.end(), '\\', '/');
#endif
        if (state.empty() || std::string("MAR!?").find(state) == std::string::npos)
            throw std::runtime_error("Unknown Mercurial file status");
        const auto full = local(root, path);
        result.push_back({{"status", state}, {"path", relativePath(workspace, full)}});
    }
    return result;
}
} // namespace

Client::Client(std::string executable, Runner runner)
    : executable_(std::move(executable)), runner_(std::move(runner)) {}

Json Client::run(const std::string &action, const Json &p) const {
    const std::set<std::string> reads = {"status", "inspect", "diff", "log", "history", "blame"};
    const std::set<std::string> supported = {"status",        "inspect", "diff",   "log",
                                             "history",       "blame",   "add",    "forget",
                                             "revert",        "commit",  "update", "bookmark.create",
                                             "branch.create", "pull",    "push"};
    if (!supported.contains(action))
        throw std::runtime_error("Unsupported Mercurial action: " + action);
    const auto workspace = fs::canonical(utf8Path(bounded(p, "workspace", 32768)));
    const auto root = local(workspace, bounded(p, "root", 8192, "."));
    if (!fs::is_directory(root))
        throw std::runtime_error("Mercurial root must be a directory");
    const auto path = p.contains("path") ? local(workspace, bounded(p, "path", 8192)) : root;
    if (!inside(root, path))
        throw std::runtime_error("Mercurial target is outside its mapping");
    for (auto parent = path; parent != root && inside(root, parent); parent = parent.parent_path())
        if (fs::exists(parent / ".hg"))
            throw std::runtime_error("Map the nested Mercurial repository separately");
    const bool fileAction = action == "add" || action == "forget" || action == "revert" || action == "blame";
    if ((fileAction || path != root) && (!p.contains("path") || path == root || fs::is_directory(path)))
        throw std::runtime_error("Select exactly one local file for this Mercurial action");
    if (action == "add" && !fs::is_regular_file(path))
        throw std::runtime_error("Mercurial add requires an existing regular file");
    if ((action == "update" || action == "bookmark.create" || action == "branch.create" || action == "pull" ||
         action == "push") &&
        path != root)
        throw std::runtime_error("This Mercurial action targets the mapped repository");
    if (p.value("staged", false))
        throw std::runtime_error("Mercurial has no Git staging area");
    const auto message = bounded(p, "message", 16384), reference = bounded(p, "revision", 128, "tip"),
               newName = bounded(p, "name", 128), remote = bounded(p, "remote", 128, "default");
    if (action == "commit" &&
        std::all_of(message.begin(), message.end(), [](unsigned char c) { return std::isspace(c); }))
        throw std::runtime_error("A nonempty Mercurial commit message is required");
    if (action == "revert" && bounded(p, "confirmation", 16) != "REVERT")
        throw std::runtime_error("Type REVERT to discard the selected file's Mercurial changes");
    if (action == "update" && !name(reference, true))
        throw std::runtime_error("Use a single Mercurial revision ID or simple named reference");
    if ((action == "branch.create" || action == "bookmark.create") && !name(newName, false))
        throw std::runtime_error("Use a simple nonempty Mercurial branch/bookmark name");
    if ((action == "pull" || action == "push") && !name(remote, false))
        throw std::runtime_error("Select a configured Mercurial remote alias");
    const auto limit = p.value("limit", Json(50));
    if (!limit.is_number_integer() || limit < 1 || limit > 200)
        throw std::runtime_error("Mercurial history limit must be 1 through 200");
    const auto metadata = root / ".hg";
    const bool mutation = !reads.contains(action);
    const bool repository = fs::is_directory(metadata) && !fs::is_symlink(fs::symlink_status(metadata)) &&
                            inside(root, fs::weakly_canonical(metadata));
    if (mutation && !repository)
        throw std::runtime_error("Open the exact Mercurial repository root first");
    if (mutation) {
        if (fs::exists(metadata / "sharedpath") || fs::exists(root / ".hgsub") ||
            fs::exists(root / ".hgsubstate"))
            throw std::runtime_error(
                "Shared Mercurial repositories and subrepositories require the external client");
        for (const auto *state :
             {"rebasestate", "histedit-state", "graftstate", "shelvedstate", "unshelverebasestate"})
            if (fs::exists(metadata / state))
                throw std::runtime_error("Finish the current Mercurial operation in the external client");
    }
    const std::map<std::string, std::string> environment = {
        {"HGPLAIN", "1"}, {"HGPLAINEXCEPT", ""}, {"HGENCODING", "utf-8"}, {"HGENCODINGMODE", "strict"}};
    auto invoke = [&](std::vector<std::string> args, int seconds = 15) {
        if (executable_.empty())
            return ProcessResult{-1, "Mercurial CLI not found. Install hg or set CYCODE_HG."};
        args.insert(args.begin(), {executable_, "--cwd", pathUtf8(root), "--noninteractive", "--pager",
                                   "never", "--config", "color.mode=off", "--config",
                                   "merge.checkunknown=abort", "--config", "merge.checkignored=abort"});
        return runner_(args, root, seconds, environment);
    };
    auto failure = [&](const ProcessResult &r, bool available) {
        auto result = process(r);
        if (action == "status" || action == "inspect")
            result.update({{"repository", available},
                           {"branch", ""},
                           {"files", Json::array()},
                           {"message", r.output.substr(0, 4096)}});
        return result;
    };
    if (!repository)
        return failure(ProcessResult{-1, "Select a folder containing a local .hg repository."}, false);
    const auto located = invoke({"root", "--template", "{hgpath|json}"});
    if (!ok(located))
        return failure(located, false);
    const auto locatedRoot = json(located);
    const auto locatedText = locatedRoot.is_string() ? locatedRoot.get<std::string>() : std::string{};
    if (locatedText.empty() || locatedText.size() > 32768 || locatedText.find('\0') != std::string::npos ||
        !utf8Path(locatedText).is_absolute() || !fs::equivalent(utf8Path(locatedText), metadata))
        throw std::runtime_error("Map the exact Mercurial repository root inside this workspace");
    const auto parentResult = invoke({"log", "--rev", "parents()", "--template", "json"});
    if (!ok(parentResult))
        return failure(parentResult, true);
    const auto parents = array(parentResult, 2);
    for (const auto &parent : parents)
        if (!node(bounded(parent, "node", 40)))
            throw std::runtime_error("Invalid Mercurial parent identity");
    if (mutation && parents.size() > 1)
        throw std::runtime_error("Finish or abort the Mercurial merge with the external client");
    auto status = [&] { return invoke({"status", "--template", "json"}); };
    if (action == "status" || action == "inspect") {
        const auto branch = invoke({"branch"});
        if (!ok(branch))
            return failure(branch, true);
        const auto current = status();
        if (!ok(current))
            return failure(current, true);
        const auto title = trimLine(branch.output);
        if (title.empty() || title.size() > 4096 || title.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Mercurial branch response");
        auto result = process(current);
        result.update(
            {{"repository", true},
             {"branch", title},
             {"files", files(current, workspace, root)},
             {"merge", parents.size() > 1},
             {"head", parents.empty() ? std::string{} : parents[0].at("node").get<std::string>()},
             {"message", parents.size() > 1
                             ? "Mercurial merge in progress; use the external client to finish or abort."
                             : ""}});
        return result;
    }
    std::string pinned;
    if (action == "update") {
        const auto current = status();
        if (!ok(current))
            return failure(current, true);
        for (const auto &file : files(current, workspace, root))
            if (file.at("status") != "?")
                throw std::runtime_error("Mercurial update requires a clean tracked working copy");
        const auto lookup = invoke({"log", "--rev", reference, "--limit", "2", "--template", "json"});
        if (!ok(lookup))
            return process(lookup);
        const auto revisions = array(lookup, 2);
        if (revisions.size() != 1 || !node(pinned = bounded(revisions[0], "node", 40)))
            throw std::runtime_error("Mercurial update requires one verified changeset");
    }
    if (action == "pull" || action == "push") {
        const auto configured = invoke({"paths", "--template", "{name}\n", "--", remote});
        if (!ok(configured))
            return process(configured);
        if (trimLine(configured.output) != remote)
            throw std::runtime_error("Mercurial remote alias is not uniquely configured");
    }
    const auto selector = "filepath:" + relativePath(root, path);
    std::vector<std::string> args;
    if (action == "diff")
        args = {"diff", "--git", "--nodates", "--", path == root ? "path:." : selector};
    else if (action == "log" || action == "history") {
        args = {"log", "--limit", std::to_string(limit.get<int>()), "--template", "json"};
        if (path != root)
            args.insert(args.end(), {"--", selector});
    } else if (action == "blame")
        args = {"annotate", "--changeset", "--user", "--number", "--date", "--", selector};
    else if (action == "add" || action == "forget")
        args = {action, "--", selector};
    else if (action == "revert")
        args = {"revert", "--no-backup", "--", selector};
    else if (action == "commit") {
        args = {"commit", "--message", message};
        if (path != root)
            args.insert(args.end(), {"--", selector});
    } else if (action == "update")
        args = {"update", "--check", "--rev", pinned};
    else if (action == "bookmark.create")
        args = {"bookmarks", "--", newName};
    else if (action == "branch.create")
        args = {"branch", "--", newName};
    else
        args = {action, "--", remote};
    auto execution = invoke(std::move(args), action == "pull" || action == "push" ? 180 : 30);
    auto result = process(execution);
    if (p.contains("path"))
        result["path"] = relativePath(workspace, path);
    if ((action == "push" || action == "commit") && execution.exitCode == 1 && !execution.timedOut &&
        !execution.truncated && !execution.cancelled) {
        result["nativeExitCode"] = 1;
        result["exitCode"] = 0;
        result["noChanges"] = true;
    }
    if ((action == "log" || action == "history") && ok(execution)) {
        result["commits"] = Json::array();
        std::string output;
        for (const auto &record : array(execution, size_t(limit.get<int>()))) {
            const auto id = bounded(record, "node", 40), author = bounded(record, "user", 4096),
                       description = bounded(record, "desc", maxOutput);
            if (!node(id) || !record.at("date").is_array() || record.at("date").size() != 2 ||
                !record.at("date")[0].is_number())
                throw std::runtime_error("Invalid Mercurial history metadata");
            const auto date = record.at("date")[0].dump();
            result["commits"].push_back({{"hash", id},
                                         {"author", author},
                                         {"date", date},
                                         {"subject", description.substr(0, description.find('\n'))}});
            output += id.substr(0, 12) + " | " + author + " | " + date + "\n" + description + "\n\n";
        }
        result["output"] = std::move(output);
    }
    return result;
}

Json descriptor() {
    auto action = [](const char *id, const char *title, const char *scope, Json fields = nullptr) {
        if (fields.is_null())
            fields = Json::array();
        return Json{{"id", id}, {"title", title}, {"scope", scope}, {"fields", std::move(fields)}};
    };
    const auto named = Json::array({{{"id", "name"}, {"title", "Mercurial name"}, {"required", true}}});
    const auto remote = Json::array({{{"id", "remote"},
                                      {"title", "Configured Mercurial remote"},
                                      {"required", true},
                                      {"default", "default"}}});
    return {{"id", "cycode.mercurial.provider"},
            {"title", "Mercurial (Hg)"},
            {"capabilities",
             {"status", "inspect", "diff", "log", "history", "blame", "add", "forget", "revert", "commit",
              "update", "bookmark.create", "branch.create", "pull", "push"}},
            {"actions",
             Json::array(
                 {action("diff", "Mercurial working-copy diff", "repository"),
                  action("log", "Mercurial history", "repository"),
                  action("blame", "Mercurial file annotations", "file"),
                  action("add", "Add file to Mercurial", "file"),
                  action("forget", "Stop tracking Mercurial file", "file"),
                  action("revert", "Revert Mercurial file", "file",
                         Json::array({{{"id", "confirmation"},
                                       {"title", "Type REVERT to discard local changes"},
                                       {"required", true}}})),
                  action("commit", "Commit Mercurial changes", "repository",
                         Json::array({{{"id", "message"}, {"title", "Commit message"}, {"required", true}}})),
                  action("update", "Update clean Mercurial working copy", "repository",
                         Json::array({{{"id", "revision"},
                                       {"title", "Mercurial revision"},
                                       {"required", true},
                                       {"default", "tip"}}})),
                  action("bookmark.create", "Create Mercurial bookmark", "repository", named),
                  action("branch.create", "Create Mercurial named branch", "repository", named),
                  action("pull", "Pull Mercurial changes without updating files", "repository", remote),
                  action("push", "Push Mercurial changes", "repository", remote)})}};
}
} // namespace cycode::mercurial

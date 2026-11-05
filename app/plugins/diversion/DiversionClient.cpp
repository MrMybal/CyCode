#include "DiversionClient.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace cycode::diversion {
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
        throw std::runtime_error("Diversion path is outside its mapping");
    auto part = path.begin();
    for (auto parent = root.begin(); parent != root.end(); ++parent)
        ++part;
    fs::path result;
    for (; part != path.end(); ++part)
        result /= *part;
    return result.empty() ? "." : pathUtf8(result);
}
std::string bounded(const Json &p, const char *key, size_t max, const std::string &fallback = {}) {
    auto value = p.value(key, fallback);
    if (value.size() > max || value.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("Invalid Diversion ") + key);
    return value;
}
bool controls(const std::string &text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
fs::path local(const fs::path &root, const std::string &text) {
    const auto path = utf8Path(text);
    if (text.empty() || controls(text) || path.is_absolute() || path.has_root_name() ||
        text.find_first_of("\\:") != std::string::npos)
        throw std::runtime_error("Diversion expects a workspace-relative local path");
    auto walk = root;
    for (const auto &part : path) {
        if (part == ".." ||
#ifdef _WIN32
            _wcsicmp(part.c_str(), L".diversion") == 0)
#else
            part == ".diversion")
#endif
            throw std::runtime_error("Diversion path traverses a parent or administrative directory");
        if (part == ".")
            continue;
        walk /= part;
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(walk, error)))
            throw std::runtime_error("Diversion operations through symbolic links are unsupported");
    }
    const auto result = fs::weakly_canonical(root / path);
    if (!inside(root, result))
        throw std::runtime_error("Diversion path escaped its mapping");
    return result;
}
bool ok(const ProcessResult &r) {
    return r.exitCode == 0 && !r.timedOut && !r.truncated && !r.cancelled;
}
Json process(const ProcessResult &r) {
    const auto code = r.exitCode == 0 && (r.timedOut || r.truncated || r.cancelled) ? 124 : r.exitCode;
    return {{"provider", "cycode.diversion.provider"},
            {"exitCode", code},
            {"output", r.output},
            {"timedOut", r.timedOut},
            {"truncated", r.truncated},
            {"cancelled", r.cancelled}};
}
// The public CLI documents text, not a JSON/porcelain switch. Strip only SGR
// colors; other escapes/control sequences must not authorize metadata or writes.
std::string plain(const ProcessResult &r) {
    if (!ok(r) || r.output.size() > maxOutput || r.output.find('\0') != std::string::npos)
        throw std::runtime_error("Diversion response is incomplete");
    std::string text;
    for (size_t i = 0; i < r.output.size(); ++i) {
        const unsigned char c = r.output[i];
        if (c == 27) {
            const auto start = i;
            if (++i == r.output.size() || r.output[i] != '[')
                throw std::runtime_error("Unsupported Diversion terminal escape");
            while (++i < r.output.size() && i - start <= 64 &&
                   ((r.output[i] >= '0' && r.output[i] <= '9') || r.output[i] == ';')) {
            }
            if (i >= r.output.size() || i - start > 64 || r.output[i] != 'm')
                throw std::runtime_error("Unsupported Diversion terminal escape");
        } else if (c == '\r') {
            if (i + 1 == r.output.size() || r.output[i + 1] != '\n')
                throw std::runtime_error("Unsupported Diversion progress output");
        } else if ((c < 32 && c != '\n' && c != '\t') || c == 127)
            throw std::runtime_error("Invalid Diversion text control");
        else
            text += char(c);
    }
    // Validate UTF-8 before using it for paths or identities.
    (void)Json(text).dump();
    return text;
}
bool id(const std::string &value, const std::string &prefix) {
    return value.starts_with(prefix) && value.size() > prefix.size() && value.size() <= 128 &&
           std::all_of(value.begin() + prefix.size(), value.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
           });
}
bool reference(const std::string &value) {
    return !value.empty() && value.size() <= 128 && std::isalnum(static_cast<unsigned char>(value.front())) &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char c) {
                           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/';
                       }) &&
           value.find("..") == std::string::npos && value.find("//") == std::string::npos;
}
struct State {
    std::string repository, workspace, branch, branchId, cloud, local;
    bool ready = true;
    bool sameIdentity(const State &other) const {
        return repository == other.repository && workspace == other.workspace && branchId == other.branchId &&
               cloud == other.cloud && local == other.local;
    }
};
State state(const ProcessResult &r) {
    State result;
    auto assign = [](std::string &field, const std::string &value) {
        if (!field.empty() || value.empty() || value.size() > 4096 || controls(value))
            throw std::runtime_error("Invalid or duplicate Diversion status identity");
        field = value;
    };
    std::istringstream input(plain(r));
    std::string line;
    while (std::getline(input, line)) {
        if (line.size() > 16384)
            throw std::runtime_error("Excessive Diversion status line");
        if (line.starts_with("In repo ")) {
            const auto at = line.rfind(" dv.repo.");
            if (at == std::string::npos)
                throw std::runtime_error("Missing Diversion repository ID");
            assign(result.repository, line.substr(at + 1));
        } else if (line.starts_with("On branch ")) {
            const auto at = line.rfind(" dv.branch.");
            if (at == std::string::npos)
                throw std::runtime_error("Missing Diversion branch ID");
            assign(result.branch, line.substr(10, at - 10));
            assign(result.branchId, line.substr(at + 1));
        } else if (line.starts_with("Working in workspace ")) {
            const auto at = line.rfind(" (dv.ws.");
            if (at == std::string::npos || line.back() != ')')
                throw std::runtime_error("Missing Diversion workspace ID");
            assign(result.workspace, line.substr(at + 2, line.size() - at - 3));
        } else if (line.starts_with("Cloud workspace is over commit "))
            assign(result.cloud, line.substr(std::string("Cloud workspace is over commit ").size()));
        else if (line.starts_with("Local workspace is synced to commit: "))
            assign(result.local, line.substr(std::string("Local workspace is synced to commit: ").size()));
        auto lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (lower.find("sync in progress") != std::string::npos ||
            lower.find("paused") != std::string::npos || lower.find("conflict") != std::string::npos ||
            lower.find("merge in progress") != std::string::npos ||
            lower.find("error:") != std::string::npos || lower.find("offline") != std::string::npos ||
            lower.starts_with("outbound changes:") || lower.starts_with("inbound changes:") ||
            lower.starts_with("currently syncing:") ||
            (lower.starts_with("queued filesystem events:") && lower != "queued filesystem events: 0"))
            result.ready = false;
    }
    if (!id(result.repository, "dv.repo.") || !id(result.workspace, "dv.ws.") ||
        !id(result.branchId, "dv.branch.") || !id(result.cloud, "dv.commit."))
        throw std::runtime_error("Unsupported Diversion status format; no working-copy state inferred");
    result.ready = result.ready && id(result.local, "dv.commit.") && result.local == result.cloud;
    return result;
}
Json files(const ProcessResult &r, const fs::path &workspace, const fs::path &root) {
    Json result = Json::array();
    std::set<std::string> seen;
    auto append = [&](const std::string &status, const std::string &path) {
        if (result.size() >= 20000 || path.empty() || path.front() == '"')
            throw std::runtime_error("Unsupported or excessive Diversion path record");
        const auto full = local(root, path);
        const auto name = relativePath(workspace, full);
        if (!seen.insert(name).second)
            throw std::runtime_error("Duplicate Diversion path record");
        result.push_back({{"status", status}, {"path", name}});
    };
    std::istringstream input(plain(r));
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty())
            continue;
        if (line.size() > 16384 || line.size() < 3 ||
            std::string("AMDR").find(line[0]) == std::string::npos || (line[1] != '\t' && line[1] != ' '))
            throw std::runtime_error(
                "Unsupported Diversion name-status record; inspect with the external client");
        const auto path = line.substr(2);
        if (path.empty() || path.front() == ' ' || path.front() == '\t' || path.back() == ' ')
            throw std::runtime_error("Ambiguous Diversion whitespace in path record");
        if (line[0] == 'R') {
            const auto split = path.find('\t');
            if (split == std::string::npos || path.find('\t', split + 1) != std::string::npos)
                throw std::runtime_error("Unsupported Diversion rename record");
            append("D", path.substr(0, split));
            append("A", path.substr(split + 1));
        } else
            append(std::string(1, line[0]), path);
    }
    return result;
}
} // namespace

Client::Client(std::string executable, Runner runner)
    : executable_(std::move(executable)), runner_(std::move(runner)) {}

Json Client::run(const std::string &action, const Json &p) const {
    const std::set<std::string> supported = {"status", "inspect", "diff",   "log",           "history",
                                             "blame",  "commit",  "revert", "branch.create", "branch.switch",
                                             "update", "lock",    "unlock"};
    if (!supported.contains(action))
        throw std::runtime_error("Unsupported Diversion action: " + action);
    const std::set<std::string> reads = {"status", "inspect", "diff", "log", "history", "blame"};
    const bool mutation = !reads.contains(action);
    const auto workspace = fs::canonical(utf8Path(bounded(p, "workspace", 32768)));
    const auto root = local(workspace, bounded(p, "root", 8192, "."));
    if (!fs::is_directory(root))
        throw std::runtime_error("Diversion root must be a directory");
    const auto path = p.contains("path") ? local(workspace, bounded(p, "path", 8192)) : root;
    if (!inside(root, path))
        throw std::runtime_error("Diversion target is outside its mapping");
    for (auto parent = path; parent != root && inside(root, parent); parent = parent.parent_path())
        if (fs::exists(parent / ".diversion"))
            throw std::runtime_error("Map the nested Diversion workspace separately");
    const bool file = path != root;
    if ((action == "revert" || action == "blame" || action == "lock" || action == "unlock") && !file)
        throw std::runtime_error("Select exactly one file for this Diversion action");
    if (file &&
        (fs::is_directory(path) || relativePath(root, path).find_first_of("*?[]{}\"") != std::string::npos))
        throw std::runtime_error(
            "Diversion file selectors must be literal single paths without pattern metacharacters");
    if ((action == "branch.create" || action == "branch.switch" || action == "update") && file)
        throw std::runtime_error("This Diversion action targets the mapped workspace");
    if (p.value("staged", false))
        throw std::runtime_error("Diversion tracks changes automatically; no staging area exists");
    const auto message = bounded(p, "message", 16384), name = bounded(p, "name", 128),
               revision = bounded(p, "revision", 128);
    if (action == "commit" &&
        std::all_of(message.begin(), message.end(), [](unsigned char c) { return std::isspace(c); }))
        throw std::runtime_error("A nonempty Diversion commit message is required");
    if (action == "revert" && bounded(p, "confirmation", 16) != "REVERT")
        throw std::runtime_error("Type REVERT to discard the selected file's Diversion changes");
    if ((action == "branch.create" && !reference(name)) ||
        (action == "branch.switch" && !reference(revision)))
        throw std::runtime_error("Use a simple Diversion branch name or reference");
    const auto limit = p.value("limit", Json(50));
    if (!limit.is_number_integer() || limit < 1 || limit > 200)
        throw std::runtime_error("Diversion history limit must be 1 through 200");
    const auto metadata = root / ".diversion";
    const bool repository = fs::is_directory(metadata) && !fs::is_symlink(fs::symlink_status(metadata)) &&
                            inside(root, fs::weakly_canonical(metadata));
    auto failure = [&](const ProcessResult &r, bool available) {
        auto result = process(r);
        if (action == "status" || action == "inspect")
            result.update({{"repository", available},
                           {"branch", ""},
                           {"files", Json::array()},
                           {"message", r.output.substr(0, 4096)}});
        return result;
    };
    if (!repository) {
        if (mutation)
            throw std::runtime_error("Map the exact local Diversion workspace root first");
        return failure(ProcessResult{-1, "Select a folder containing a local .diversion workspace."}, false);
    }
    const std::map<std::string, std::string> environment = {
        {"NO_COLOR", "1"}, {"CLICOLOR", "0"}, {"TERM", "dumb"}};
    auto invoke = [&](std::vector<std::string> argv, int seconds = 30) {
        if (executable_.empty())
            return ProcessResult{-1, "Diversion CLI not found. Install Diversion or set CYCODE_DV."};
        argv.insert(argv.begin(), executable_);
        return runner_(argv, root, seconds, environment);
    };
    auto status = [&] { return invoke({"status", "--nowait", "--no-limit"}); };
    const auto identity = status();
    if (!ok(identity))
        return failure(identity, false);
    const auto before = state(identity);
    const auto summary = invoke({"diff", "--name-status", "--color", "never"});
    if (!ok(summary))
        return failure(summary, true);
    const auto changes = files(summary, workspace, root);
    if (action == "status" || action == "inspect") {
        auto result = process(identity);
        result.update(
            {{"repository", true},
             {"branch", before.branch},
             {"files", changes},
             {"repositoryId", before.repository},
             {"workspaceId", before.workspace},
             {"branchId", before.branchId},
             {"head", before.cloud},
             {"localHead", before.local},
             {"syncReady", before.ready},
             {"message",
              before.ready
                  ? "Diversion tracks and syncs changes automatically; no staging or push/pull is required."
                  : "Diversion sync/conflict state is not ready. Status is a cloud comparison; writes "
                    "require a fully synced workspace."}});
        return result;
    }
    if (mutation) {
        if (!before.ready)
            throw std::runtime_error(
                "Finish Diversion syncing/conflict resolution with the external client first");
        if ((action == "update" || action == "branch.switch") && !changes.empty())
            throw std::runtime_error("Diversion update/checkout requires a clean working copy");
        const auto checked = status();
        if (!ok(checked))
            return process(checked);
        const auto current = state(checked);
        if (!current.ready || !before.sameIdentity(current))
            throw std::runtime_error("Diversion workspace changed during preflight; refresh before retrying");
    }
    // Prefix a local operand so a leading '-' cannot become an option. The CLI
    // owns its path matching; unsupported metacharacters are refused above.
    const auto selector = "./" + relativePath(root, path);
    std::vector<std::string> args;
    if (action == "diff") {
        args = {"diff", "--color", "never"};
        if (file)
            args.push_back(selector);
    } else if (action == "log" || action == "history") {
        args = {"log", "-n", std::to_string(limit.get<int>()), "--date", "iso"};
        if (file)
            args.push_back(selector);
    } else if (action == "blame")
        args = {"annotate", selector};
    else if (action == "commit") {
        args = {"commit", "-m", message};
        args.push_back(file ? selector : "-a");
    } else if (action == "revert")
        args = {"reset", selector, "-f"};
    else if (action == "branch.create")
        args = {"branch", "-c", name, "--no-checkout"};
    else if (action == "branch.switch")
        args = {"checkout", revision, "--ignore-shelf"};
    else if (action == "update")
        args = {"update", "--conflict_resolution", "manual"};
    else if (action == "lock")
        args = {"lock", selector};
    else
        args = {"lock", "-d", selector};
    const auto execution = invoke(std::move(args), mutation ? 180 : 30);
    auto result = process(execution);
    if (file)
        result["path"] = relativePath(workspace, path);
    // Human-facing log/annotations remain read-only text, never guessed JSON.
    if (ok(execution) && !mutation)
        result["output"] = plain(execution);
    return result;
}

Json descriptor() {
    auto action = [](const char *id, const char *title, const char *scope, Json fields = nullptr) {
        if (fields.is_null())
            fields = Json::array();
        return Json{{"id", id}, {"title", title}, {"scope", scope}, {"fields", std::move(fields)}};
    };
    return {
        {"id", "cycode.diversion.provider"},
        {"title", "Diversion"},
        {"capabilities",
         {"status", "inspect", "diff", "log", "history", "blame", "commit", "revert", "branch.create",
          "branch.switch", "update", "lock", "unlock"}},
        {"actions",
         Json::array(
             {action("diff", "Diversion working-copy diff", "repository"),
              action("log", "Diversion history", "repository"),
              action("blame", "Diversion file annotations", "file"),
              action("commit", "Commit Diversion changes (auto-sync)", "repository",
                     Json::array({{{"id", "message"}, {"title", "Commit message"}, {"required", true}}})),
              action("revert", "Reset Diversion file", "file",
                     Json::array({{{"id", "confirmation"},
                                   {"title", "Type REVERT to discard local changes"},
                                   {"required", true}}})),
              action("branch.create", "Create Diversion branch without switching", "repository",
                     Json::array({{{"id", "name"}, {"title", "Diversion branch name"}, {"required", true}}})),
              action(
                  "branch.switch", "Switch clean Diversion workspace", "repository",
                  Json::array({{{"id", "revision"}, {"title", "Diversion reference"}, {"required", true}}})),
              action("update", "Update clean Diversion workspace", "repository"),
              action("lock", "Lock Diversion file", "file"),
              action("unlock", "Unlock Diversion file", "file")})}};
}
} // namespace cycode::diversion

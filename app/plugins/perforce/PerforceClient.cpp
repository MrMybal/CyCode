#include "PerforceClient.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <set>

namespace cycode::perforce {
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
        throw std::runtime_error("Perforce path escaped its root");
    auto item = path.begin();
    for (auto parent = root.begin(); parent != root.end(); ++parent)
        ++item;
    fs::path relative;
    for (; item != path.end(); ++item)
        relative /= *item;
    return relative.empty() ? "." : pathUtf8(relative);
}
std::string unescape(std::string_view input) {
    std::string result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            auto pair = std::string(input.substr(i + 1, 2));
            for (auto &c : pair)
                c = char(std::toupper(static_cast<unsigned char>(c)));
            if (pair == "25" || pair == "40" || pair == "23" || pair == "2A") {
                result += pair == "25" ? '%' : pair == "40" ? '@' : pair == "23" ? '#' : '*';
                i += 2;
                continue;
            }
        }
        result += input[i];
    }
    return result;
}
std::string bounded(const Json &j, const char *key, size_t limit, const std::string &fallback = {}) {
    const auto value = j.value(key, fallback);
    if (value.size() > limit || value.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("Invalid Perforce ") + key);
    return value;
}
fs::path local(const fs::path &root, const std::string &input) {
    const auto path = utf8Path(input);
    if (input.empty() || path.is_absolute() || path.has_root_name() ||
        input.find('\\') != std::string::npos || input.find(':') != std::string::npos ||
        input.find("...") != std::string::npos)
        throw std::runtime_error(
            "Perforce expects a relative local path without revision or recursive selectors");
    auto walk = root;
    for (const auto &part : path) {
        if (part == "..")
            throw std::runtime_error("Perforce path traverses a parent");
        if (part == ".")
            continue;
        walk /= part;
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(walk, error)))
            throw std::runtime_error("Perforce symbolic-link traversal is unsupported");
    }
    const auto canonical = fs::weakly_canonical(root / path);
    if (!inside(root, canonical))
        throw std::runtime_error("Perforce path escaped its root");
    return canonical;
}
std::string specification(const fs::path &path) {
    std::string result;
    for (const auto c : pathUtf8(path)) {
        if (c == '%')
            result += "%25";
        else if (c == '@')
            result += "%40";
        else if (c == '#')
            result += "%23";
        else if (c == '*')
            result += "%2A";
        else
            result += c;
    }
    return result;
}
int number(const Json &record, const char *name, int fallback) {
    if (!record.contains(name))
        return fallback;
    if (record.at(name).is_number_integer())
        return record.at(name).get<int>();
    const auto text = record.at(name).get<std::string>();
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid Perforce numeric record field");
    return value;
}
struct Reply {
    ProcessResult execution;
    Json records = Json::array();
    std::string text;
    bool ok = false;
};
Reply decode(ProcessResult execution, bool allowEmpty, bool unifiedDiff = false) {
    Reply reply;
    reply.execution = std::move(execution);
    auto &result = reply.execution;
    if (result.output.size() > maxOutput || result.timedOut || result.truncated || result.cancelled) {
        if (result.exitCode == 0)
            result.exitCode = 124;
        return reply;
    }
    bool emptyWarning = false, failure = false;
    size_t start = 0, count = 0;
    while (start < result.output.size()) {
        const auto end = result.output.find('\n', start);
        auto line = result.output.substr(start, end == std::string::npos ? end : end - start);
        start = end == std::string::npos ? result.output.size() : end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        if (++count > 20000)
            throw std::runtime_error("Perforce response exceeds 20000 records");
        if (line.front() != '{') {
            if (result.exitCode != 0)
                return reply;
            throw std::runtime_error("Expected complete Perforce -Mj tagged JSON output");
        }
        const auto record = Json::parse(line, [](int depth, Json::parse_event_t, Json &) {
            if (depth > 16)
                throw std::runtime_error("Perforce JSON nesting exceeds its limit");
            return true;
        });
        if (!record.is_object() || record.size() > 512)
            throw std::runtime_error("Invalid Perforce record");
        const auto code = bounded(record, "code", 16, "stat");
        const auto severity = number(record, "severity", code == "error" ? 3 : 0);
        if (severity >= 2 || code == "error") {
            // EV_EMPTY is a language-independent no-results response. Never
            // reinterpret permission/authentication/configuration errors as empty.
            if (allowEmpty && severity == 2 && number(record, "generic", 0) == 17)
                emptyWarning = true;
            else
                failure = true;
            continue;
        }
        if (code == "stat") {
            reply.records.push_back(record);
            if (unifiedDiff && record.contains("depotFile") && record.contains("clientFile"))
                reply.text += "--- " + bounded(record, "depotFile", 8192) + "\n+++ " +
                              bounded(record, "clientFile", 8192) + "\n";
        } else if (code != "info" && code != "text")
            throw std::runtime_error("Unknown Perforce record type");
        if (record.contains("data"))
            reply.text += bounded(record, "data", maxOutput);
        if (reply.text.size() > maxOutput)
            throw std::runtime_error("Perforce text response exceeds its limit");
    }
    if (emptyWarning && !failure && result.exitCode == 1)
        result.exitCode = 0;
    reply.ok = result.exitCode == 0 && !failure;
    if (failure && result.exitCode == 0)
        result.exitCode = 1;
    return reply;
}
Json process(const Reply &reply) {
    return {{"provider", "cycode.perforce.provider"},
            {"exitCode", reply.execution.exitCode},
            {"output", reply.ok ? reply.text : reply.execution.output},
            {"timedOut", reply.execution.timedOut},
            {"truncated", reply.execution.truncated},
            {"cancelled", reply.execution.cancelled}};
}
std::string changelist(const Json &parameters) {
    const auto value = bounded(parameters, "changelist", 12, "default");
    if (value != "default" &&
        (value.empty() || value.front() == '0' ||
         !std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })))
        throw std::runtime_error("Perforce changelist must be default or a positive decimal ID");
    return value;
}
Json files(const Reply &reply, const fs::path &workspace, const fs::path &root) {
    Json output = Json::array();
    for (const auto &record : reply.records) {
        const auto action = bounded(record, "action", 64);
        if (action.empty())
            continue;
        const auto name = bounded(record, "clientFile", 8192);
        auto path = utf8Path(name);
        if (!path.is_absolute() || !inside(root, path.lexically_normal()) ||
            !inside(root, fs::weakly_canonical(path)))
            throw std::runtime_error("Perforce status returned an unmapped local file");
        std::string code = action == "edit"          ? "M"
                           : action == "add"         ? "A"
                           : action == "delete"      ? "D"
                           : action == "move/add"    ? "A"
                           : action == "move/delete" ? "D"
                                                     : "M";
        if (number(record, "unresolved", 0) > 0)
            code = "U";
        output.push_back({{"path", relativePath(workspace, path.lexically_normal())},
                          {"status", code},
                          {"action", action},
                          {"changelist", bounded(record, "change", 24, "default")},
                          {"type", bounded(record, "type", 128)}});
    }
    return output;
}
} // namespace

Client::Client(std::string executable, Runner runner)
    : executable_(std::move(executable)), runner_(std::move(runner)) {}

Json Client::run(const std::string &action, const Json &p) const {
    const std::set<std::string> supported = {"status", "inspect",  "diff", "log",    "history",
                                             "blame",  "checkout", "add",  "revert", "reopen",
                                             "commit", "sync",     "lock", "unlock"};
    if (!supported.contains(action))
        throw std::runtime_error("Unsupported Perforce action: " + action);
    const auto workspace = fs::canonical(utf8Path(bounded(p, "workspace", 32768)));
    if (pathUtf8(workspace).starts_with("//"))
        throw std::runtime_error("Perforce UNC roots are unsupported");
    const auto root = local(workspace, bounded(p, "root", 8192, "."));
    if (!fs::is_directory(root))
        throw std::runtime_error("Perforce root must be a directory");
    if (pathUtf8(root).find("...") != std::string::npos)
        throw std::runtime_error("Perforce roots containing a literal ... are unsupported");
    const auto path = p.contains("path") ? local(workspace, bounded(p, "path", 8192)) : root;
    if (!inside(root, path))
        throw std::runtime_error("Perforce target escaped the selected mapping");
    const bool fileAction = action == "blame" || action == "checkout" || action == "add" ||
                            action == "revert" || action == "reopen" || action == "commit" ||
                            action == "lock" || action == "unlock";
    if (fileAction && (!p.contains("path") || path == root || fs::is_directory(path)))
        throw std::runtime_error("Select a single local file for this Perforce action");
    if ((action == "add" || action == "checkout") && !fs::is_regular_file(path))
        throw std::runtime_error("Perforce add/checkout requires an existing regular file");
    if (action == "sync" && path != root)
        throw std::runtime_error("Sync targets the mapped project folder");
    if (action == "revert" && bounded(p, "confirmation", 16) != "REVERT")
        throw std::runtime_error("Type REVERT to discard the selected file's Perforce changes");
    const auto change = changelist(p), message = bounded(p, "message", 16384);
    if (action == "commit" &&
        std::all_of(message.begin(), message.end(), [](unsigned char c) { return std::isspace(c); }))
        throw std::runtime_error("A nonempty Perforce submit description is required");
    const auto limit = p.value("limit", Json(50));
    if (!limit.is_number_integer() || limit < 1 || limit > 200)
        throw std::runtime_error("Perforce history limit must be 1 through 200");
    if (p.value("staged", false))
        throw std::runtime_error("Perforce has no Git staging area");
    std::string client, user;
    auto invoke = [&](std::vector<std::string> arguments, int seconds = 15, bool empty = false) {
        if (executable_.empty())
            return decode(ProcessResult{-1, "Perforce CLI not found. Install p4 or set CYCODE_P4."}, false);
        std::vector<std::string> args = {executable_, "-ztag", "-Mj", "-d", pathUtf8(root)};
        if (!client.empty())
            args.insert(args.end(), {"-c", client, "-u", user});
        args.insert(args.end(), arguments.begin(), arguments.end());
        return decode(runner_(args, root, seconds), empty, !arguments.empty() && arguments.front() == "diff");
    };
    const auto info = invoke({"info"});
    auto unavailable = [&](const Reply &reply) {
        auto result = process(reply);
        if (action == "status" || action == "inspect")
            result.update({{"repository", false},
                           {"branch", ""},
                           {"files", Json::array()},
                           {"message", reply.execution.output.substr(0, 4096)}});
        return result;
    };
    if (!info.ok)
        return unavailable(info);
    if (info.records.size() != 1)
        throw std::runtime_error("Perforce info did not identify one workspace");
    const auto identity = info.records.front();
    client = bounded(identity, "clientName", 256);
    user = bounded(identity, "userName", 256);
    const auto clientRoot = utf8Path(bounded(identity, "clientRoot", 8192));
    if (client.empty() || client.front() == '*' || user.empty() || !clientRoot.is_absolute() ||
        !inside(fs::weakly_canonical(clientRoot), root))
        throw std::runtime_error("Select a configured Perforce client containing this project");
    auto mapping = [&](const fs::path &file) {
        const auto where = invoke({"where", specification(file)});
        if (!where.ok)
            throw std::runtime_error("Perforce mapping failed: " + where.execution.output.substr(0, 4096));
        if (where.records.size() != 1 || where.records[0].contains("unmap") ||
            !bounded(where.records[0], "depotFile", 8192).starts_with("//") ||
            !bounded(where.records[0], "clientFile", 8192).starts_with("//" + client + "/"))
            throw std::runtime_error(
                "Perforce requires one positive mapping; overlays/exclusions need external tools");
        const auto value = bounded(where.records[0], "path", 8192);
        auto matches = [&](const std::string &name) {
            const auto candidate = utf8Path(name);
            if (!candidate.is_absolute())
                return false;
            const auto mapped = fs::weakly_canonical(candidate);
            return inside(root, mapped) && inside(file, mapped) && inside(mapped, file);
        };
        if (!matches(value) && !matches(unescape(value)))
            throw std::runtime_error("Perforce mapping points outside the selected local path");
    };
    mapping(root);
    if (path != root)
        mapping(path);
    const auto tree = specification(root) + "/...";
    const auto spec = path == root ? tree : specification(path);
    auto opened = [&](const std::string &target) {
        return invoke({"fstat", "-Ro", "-T", "clientFile,action,change,type,unresolved", target}, 30, true);
    };
    if (action == "status" || action == "inspect") {
        const auto current = opened(tree);
        auto result = process(current);
        result.update({{"repository", true},
                       {"branch", client},
                       {"client", client},
                       {"files", current.ok ? files(current, workspace, root) : Json::array()},
                       {"message", current.ok ? "" : current.execution.output.substr(0, 4096)}});
        return result;
    }
    if (action == "revert" || action == "commit") {
        const auto current = opened(spec);
        if (!current.ok)
            return process(current);
        const auto entries = files(current, workspace, root);
        if (entries.size() != 1)
            throw std::runtime_error("Select exactly one file opened in this Perforce client");
        const auto selected = local(workspace, entries[0].at("path"));
        if (!inside(selected, path) || !inside(path, selected))
            throw std::runtime_error("Perforce opened-file response refers to another target");
        const std::string kind = entries[0].at("action");
        if (kind != "edit" && kind != "add" && kind != "delete")
            throw std::runtime_error("Move/integration Perforce changes require the external client");
        if (action == "commit" &&
            (entries[0].at("changelist") != "default" || entries[0].at("status") == "U"))
            throw std::runtime_error("Submit requires a resolved file in the default changelist");
    }
    if (action == "sync") {
        const auto current = opened(tree);
        if (!current.ok)
            return process(current);
        if (!files(current, workspace, root).empty())
            throw std::runtime_error("Perforce sync requires no opened project files");
        const auto preview = invoke({"reconcile", "-n", "-a", "-e", "-d", tree}, 180, true);
        if (!preview.ok)
            return process(preview);
        if (!preview.records.empty() || !preview.text.empty())
            throw std::runtime_error("Reconcile local Perforce project changes before sync");
    }
    std::vector<std::string> args;
    if (action == "diff")
        args = {"diff", "-du", spec};
    else if (action == "blame")
        args = {"annotate", "-q", spec + "#have"};
    else if (action == "log" || action == "history")
        args = {"changes", "-l", "-s", "submitted", "-m", std::to_string(limit.get<int>()), spec};
    else if (action == "checkout")
        args = {"edit", "-c", change, spec};
    else if (action == "add")
        args = {"add", "-f", "-c", change, pathUtf8(path)};
    else if (action == "revert")
        args = {"revert", spec};
    else if (action == "reopen")
        args = {"reopen", "-c", change, spec};
    else if (action == "commit")
        args = {"submit", "-Sx", "-d", message, spec};
    else if (action == "sync")
        args = {"sync", tree};
    else
        args = {action, spec};
    const auto execution = invoke(
        std::move(args), 180, action == "log" || action == "history" || action == "diff" || action == "sync");
    auto result = process(execution);
    if (p.contains("path"))
        result["path"] = relativePath(workspace, path);
    if (!execution.ok)
        return result;
    if (action == "log" || action == "history") {
        result["commits"] = Json::array();
        std::string output;
        for (const auto &record : execution.records) {
            if (result["commits"].size() >= size_t(limit.get<int>()))
                throw std::runtime_error("Perforce history exceeded its limit");
            const auto id = bounded(record, "change", 32), author = bounded(record, "user", 256),
                       date = bounded(record, "time", 32), description = bounded(record, "desc", maxOutput);
            result["commits"].push_back({{"hash", id},
                                         {"author", author},
                                         {"date", date},
                                         {"subject", description.substr(0, description.find('\n'))}});
            output += "Change " + id + " | " + author + " | " + date + "\n" + description + "\n\n";
        }
        result["output"] = std::move(output);
    } else if (action == "blame") {
        std::string output = execution.text;
        for (const auto &record : execution.records)
            if (record.contains("line"))
                output += bounded(record, "lower", 32) + ": " + bounded(record, "line", maxOutput) + "\n";
        result["output"] = std::move(output);
    } else if (result.at("output").get_ref<const std::string &>().empty())
        result["output"] = execution.execution.output;
    return result;
}

Json descriptor() {
    auto action = [](const char *id, const char *title, const char *scope, Json fields = nullptr) {
        if (fields.is_null())
            fields = Json::array();
        return Json{{"id", id}, {"title", title}, {"scope", scope}, {"fields", std::move(fields)}};
    };
    const Json change = Json::array({{{"id", "changelist"},
                                      {"title", "Perforce changelist"},
                                      {"default", "default"},
                                      {"required", true}}});
    return {
        {"id", "cycode.perforce.provider"},
        {"title", "Perforce (P4)"},
        {"capabilities",
         {"status", "inspect", "diff", "log", "history", "blame", "checkout", "add", "revert", "reopen",
          "commit", "sync", "lock", "unlock"}},
        {"actions",
         Json::array(
             {action("diff", "Perforce project diff", "repository"),
              action("log", "Perforce submitted history", "repository"),
              action("blame", "Perforce file annotations", "file"),
              action("checkout", "Check out Perforce file", "file", change),
              action("add", "Add file to Perforce", "file", change),
              action("reopen", "Move file to Perforce changelist", "file", change),
              action("revert", "Revert Perforce file", "file",
                     Json::array({{{"id", "confirmation"},
                                   {"title", "Type REVERT to discard local changes"},
                                   {"required", true}}})),
              action("commit", "Submit Perforce file", "file",
                     Json::array({{{"id", "message"}, {"title", "Submit description"}, {"required", true}}})),
              action("sync", "Sync clean Perforce project", "repository"),
              action("lock", "Lock Perforce file", "file"),
              action("unlock", "Unlock Perforce file", "file")})}};
}
} // namespace cycode::perforce

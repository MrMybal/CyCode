#include "SvnClient.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <tinyxml2.h>

namespace cycode::svn {
namespace {
namespace fs = std::filesystem;
constexpr size_t maxOutput = 512 * 1024;
std::string text(const tinyxml2::XMLElement *node) {
    return node && node->GetText() ? node->GetText() : "";
}
std::string attribute(const tinyxml2::XMLElement *node, const char *key) {
    return node && node->Attribute(key) ? node->Attribute(key) : "";
}
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
std::string bounded(const Json &parameters, const char *key, size_t maximum,
                    const std::string &fallback = {}) {
    const auto value = parameters.value(key, fallback);
    if (value.size() > maximum || value.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("Invalid SVN ") + key);
    return value;
}
fs::path localPath(const fs::path &root, const std::string &input) {
    auto relative = utf8Path(input);
    if (input.empty() || relative.is_absolute() || relative.has_root_name() ||
        input.find("://") != std::string::npos || input.find('\\') != std::string::npos)
        throw std::runtime_error("SVN expects a workspace-relative local path");
#ifdef _WIN32
    if (input.find(':') != std::string::npos)
        throw std::runtime_error("SVN alternate data stream paths are unsupported");
#endif
    for (const auto &part : relative)
        if (part == ".." ||
#ifdef _WIN32
            _wcsicmp(part.c_str(), L".svn") == 0)
#else
            part == ".svn")
#endif
            throw std::runtime_error("SVN path traverses a parent or administrative directory");
    const auto lexical = (root / relative).lexically_normal();
    auto walk = root;
    for (const auto &part : relative) {
        if (part == ".")
            continue;
        walk /= part;
        std::error_code ec;
        if (fs::is_symlink(fs::symlink_status(walk, ec)))
            throw std::runtime_error("SVN operations through symbolic links are unsupported");
    }
    const auto canonical = fs::weakly_canonical(lexical);
    if (!inside(root, canonical))
        throw std::runtime_error("SVN path is outside its root");
    return canonical;
}
std::string target(const fs::path &root, const fs::path &path) {
    // ./ prevents URL/option interpretation; the final @ escapes literal peg syntax.
    return "./" + pathUtf8(path.lexically_relative(root)) + "@";
}
bool successful(const ProcessResult &result) {
    return result.exitCode == 0 && !result.timedOut && !result.truncated && !result.cancelled;
}
Json process(const ProcessResult &r) {
    const int code = r.exitCode == 0 && (r.timedOut || r.truncated || r.cancelled) ? 124 : r.exitCode;
    return {{"exitCode", code},         {"output", r.output},       {"timedOut", r.timedOut},
            {"truncated", r.truncated}, {"cancelled", r.cancelled}, {"provider", "cycode.svn.provider"}};
}
const tinyxml2::XMLElement *xml(tinyxml2::XMLDocument &document, const ProcessResult &r, const char *root) {
    if (!successful(r) || r.output.size() > maxOutput || r.output.find('\0') != std::string::npos ||
        document.Parse(r.output.data(), r.output.size()) != tinyxml2::XML_SUCCESS ||
        !document.FirstChildElement(root))
        throw std::runtime_error(std::string("Incomplete or invalid SVN ") + root + " XML response");
    return document.FirstChildElement(root);
}
Json files(const ProcessResult &result, const fs::path &workspace, const fs::path &root) {
    tinyxml2::XMLDocument document;
    const auto status = xml(document, result, "status");
    Json output = Json::array();
    const std::map<std::string, std::string> codes = {
        {"added", "A"},   {"deleted", "D"},    {"modified", "M"},    {"replaced", "R"}, {"conflicted", "U"},
        {"missing", "!"}, {"obstructed", "~"}, {"unversioned", "?"}, {"ignored", "I"},  {"external", "X"},
        {"normal", " "},  {"none", " "},       {"incomplete", "!"}};
    // Entries in named changelists are emitted separately from <target>.
    for (auto group = status->FirstChildElement(); group; group = group->NextSiblingElement()) {
        if (std::string(group->Name()) != "target" && std::string(group->Name()) != "changelist")
            throw std::runtime_error("Unknown SVN status group");
        for (auto entry = group->FirstChildElement("entry"); entry;
             entry = entry->NextSiblingElement("entry")) {
            if (output.size() >= 20000)
                throw std::runtime_error("SVN status exceeds 20000 entries");
            const auto wc = entry->FirstChildElement("wc-status");
            if (!wc)
                throw std::runtime_error("SVN status entry is missing wc-status");
            auto name = attribute(entry, "path");
            // svn may emit absolute paths even when invoked with a local target.
            auto path = utf8Path(name);
            if (path.is_absolute()) {
                if (!inside(root, path.lexically_normal()))
                    throw std::runtime_error("SVN status escaped its root");
                name = pathUtf8(path.lexically_relative(root));
            }
            // Symlink entries may be displayed but are never operated on by this provider.
            const auto lexical = (root / utf8Path(name)).lexically_normal();
            if (name.empty() || !inside(root, lexical))
                throw std::runtime_error("Invalid SVN status path");
            const auto item = attribute(wc, "item"), props = attribute(wc, "props");
            auto code = codes.contains(item) ? codes.at(item) : std::string("!");
            const bool conflict =
                item == "conflicted" || props == "conflicted" || attribute(wc, "tree-conflicted") == "true";
            if (conflict)
                code = "U";
            else if (props == "modified" && code == " ")
                code = "M";
            const bool external = attribute(wc, "file-external") == "true";
            if (external && code == " ")
                code = "X";
            if (code == " ")
                continue;
            output.push_back({{"status", code},
                              {"path", pathUtf8(lexical.lexically_relative(workspace))},
                              {"item", item},
                              {"properties", props},
                              {"conflict", conflict},
                              {"fileExternal", external}});
        }
    }
    return output;
}
} // namespace

Client::Client(std::string executable, Runner runner)
    : executable_(std::move(executable)), runner_(std::move(runner)) {}

Json Client::run(const std::string &action, const Json &p) const {
    const std::set<std::string> supported = {"status", "inspect", "diff",   "log",    "history",
                                             "blame",  "add",     "revert", "commit", "update"};
    if (!supported.contains(action))
        throw std::runtime_error("Unsupported SVN action: " + action);
    const auto workspace = fs::canonical(utf8Path(bounded(p, "workspace", 32768)));
    const auto root = localPath(workspace, bounded(p, "root", 8192, "."));
    if (!fs::is_directory(root))
        throw std::runtime_error("SVN root must be a directory");
    const auto path = p.contains("path") ? localPath(workspace, bounded(p, "path", 8192)) : root;
    if (!inside(root, path))
        throw std::runtime_error("SVN target is outside the selected root");
    // A nested checkout/external is a separate provider mapping, never an implicit target.
    for (auto parent = path; parent != root && inside(root, parent); parent = parent.parent_path())
        if (fs::is_directory(parent / ".svn"))
            throw std::runtime_error("Map the nested SVN working copy separately");
    if (p.value("staged", false))
        throw std::runtime_error("SVN has no Git staging area");
    const bool fileAction = action == "add" || action == "revert" || action == "blame";
    if (fileAction && (!p.contains("path") || path == root || fs::is_directory(path)))
        throw std::runtime_error("Select a single file for this SVN action");
    if ((action == "commit" || action == "update") && path != root)
        throw std::runtime_error("SVN commit/update use the whole mapped working copy");
    const auto message = bounded(p, "message", 16384);
    if (action == "commit" &&
        std::all_of(message.begin(), message.end(), [](unsigned char c) { return std::isspace(c); }))
        throw std::runtime_error("A nonempty SVN commit message is required");
    if (action == "revert" && bounded(p, "confirmation", 16) != "REVERT")
        throw std::runtime_error("Type REVERT to discard the selected file's local changes");
    const auto limit = p.value("limit", Json(50));
    if (!limit.is_number_integer() || limit < 1 || limit > 200)
        throw std::runtime_error("SVN history limit must be 1 through 200");
    auto invoke = [&](std::vector<std::string> args, int seconds = 15) {
        if (executable_.empty())
            return ProcessResult{-1, "SVN executable not found. Install Subversion or set CYCODE_SVN."};
        args.insert(args.begin(), {executable_, "--non-interactive", "--no-auth-cache"});
        return runner_(args, root, seconds);
    };
    const auto info = invoke({"info", "--xml", "--depth", "empty", "--", "./.@"});
    if (!successful(info)) {
        auto result = process(info);
        if (action != "status" && action != "inspect")
            return result;
        result.update(
            {{"repository", false}, {"branch", ""}, {"files", Json::array()}, {"message", info.output}});
        return result;
    }
    tinyxml2::XMLDocument information;
    const auto entry = xml(information, info, "info")->FirstChildElement("entry");
    if (!entry || attribute(entry, "kind") != "dir")
        throw std::runtime_error("SVN root is not a versioned directory");
    const auto wc = entry->FirstChildElement("wc-info");
    const auto wcRoot = wc ? text(wc->FirstChildElement("wcroot-abspath")) : std::string{};
    if (wcRoot.empty() || !fs::equivalent(utf8Path(wcRoot), root))
        throw std::runtime_error("Map the exact SVN working-copy root inside this workspace");
    auto readStatus = [&] { return invoke({"status", "--xml", "--ignore-externals", "--", "./.@"}); };
    if (path != root && action != "status" && action != "inspect") {
        const auto selected =
            invoke({"status", "--xml", "--depth", "empty", "--ignore-externals", "--", target(root, path)});
        if (!successful(selected))
            return process(selected);
        for (const auto &entry : files(selected, workspace, root))
            if (entry.at("fileExternal").get<bool>())
                throw std::runtime_error("SVN file externals are unsupported by this provider");
    }
    if (action == "status" || action == "inspect") {
        auto status = readStatus();
        auto result = process(status);
        result.update({{"repository", true},
                       {"branch", text(entry->FirstChildElement("relative-url"))},
                       {"revision", attribute(entry, "revision")},
                       {"files", Json::array()},
                       {"message", ""}});
        if (successful(status))
            result["files"] = files(status, workspace, root);
        else
            result["message"] = status.output;
        return result;
    }
    if (action == "update" || action == "commit") {
        const auto status = readStatus();
        if (!successful(status))
            return process(status);
        for (const auto &file : files(status, workspace, root)) {
            if (file.at("conflict").get<bool>())
                throw std::runtime_error("Resolve SVN conflicts first");
            const std::string code = file.at("status");
            if (action == "update" && code != "?" && code != "I" && code != "X")
                throw std::runtime_error("SVN update requires a clean tracked working copy");
        }
    }
    std::vector<std::string> args;
    int seconds = 15;
    if (action == "diff")
        args = {"diff", "--internal-diff"};
    else if (action == "blame") {
        args = {"blame", "--revision", "BASE"};
        seconds = 180;
    } else if (action == "log" || action == "history") {
        args = {"log", "--xml", "--limit", std::to_string(limit.get<int>())};
        seconds = 180;
    } else if (action == "add")
        args = {"add", "--depth", "empty", "--no-auto-props"};
    else if (action == "revert")
        args = {"revert", "--depth", "empty"};
    else if (action == "commit") {
        args = {"commit", "--depth", "infinity", "--encoding", "UTF-8", "--force-log", "--message", message};
        seconds = 180;
    } else if (action == "update") {
        args = {"update", "--ignore-externals", "--accept", "postpone"};
        seconds = 180;
    }
    args.insert(args.end(), {"--", target(root, path)});
    const auto execution = invoke(std::move(args), seconds);
    auto result = process(execution);
    if (p.contains("path"))
        result["path"] = pathUtf8(path.lexically_relative(workspace));
    if ((action == "log" || action == "history") && successful(execution)) {
        tinyxml2::XMLDocument log;
        const auto entries = xml(log, execution, "log");
        result["commits"] = Json::array();
        std::string output;
        for (auto item = entries->FirstChildElement("logentry"); item;
             item = item->NextSiblingElement("logentry")) {
            if (result["commits"].size() >= size_t(limit.get<int>()))
                throw std::runtime_error("SVN history exceeds its limit");
            const auto revision = attribute(item, "revision"),
                       author = text(item->FirstChildElement("author")),
                       date = text(item->FirstChildElement("date")),
                       messageText = text(item->FirstChildElement("msg"));
            result["commits"].push_back({{"hash", revision},
                                         {"author", author},
                                         {"date", date},
                                         {"subject", messageText.substr(0, messageText.find('\n'))}});
            output += "r" + revision + " | " + author + " | " + date + "\n" + messageText + "\n\n";
        }
        result["output"] = std::move(output);
    }
    return result;
}

Json descriptor() {
    return {
        {"id", "cycode.svn.provider"},
        {"title", "Subversion (SVN)"},
        {"capabilities",
         {"status", "inspect", "diff", "log", "history", "blame", "add", "revert", "commit", "update"}},
        {"actions",
         Json::array({{{"id", "diff"}, {"title", "SVN working-copy diff"}, {"scope", "repository"}},
                      {{"id", "log"}, {"title", "SVN history"}, {"scope", "repository"}},
                      {{"id", "blame"}, {"title", "SVN file annotations"}, {"scope", "file"}},
                      {{"id", "add"}, {"title", "Add file to SVN"}, {"scope", "file"}},
                      {{"id", "revert"},
                       {"title", "Revert SVN file"},
                       {"scope", "file"},
                       {"fields", Json::array({{{"id", "confirmation"},
                                                {"title", "Type REVERT to discard local changes"},
                                                {"required", true}}})}},
                      {{"id", "commit"},
                       {"title", "Commit SVN changes"},
                       {"scope", "repository"},
                       {"fields",
                        Json::array({{{"id", "message"}, {"title", "Commit message"}, {"required", true}}})}},
                      {{"id", "update"}, {"title", "Update SVN working copy"}, {"scope", "repository"}}})}};
}
} // namespace cycode::svn

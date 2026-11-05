#include "cycode/GitSubmodules.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
std::vector<std::string> records(const std::string &text) {
    if (text.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Submodule metadata exceeds 2 MiB");
    std::vector<std::string> result;
    size_t at = 0;
    while (at < text.size()) {
        const auto end = text.find('\0', at);
        if (end == std::string::npos || result.size() >= 20000)
            throw std::runtime_error("Invalid or oversized NUL-delimited submodule metadata");
        result.push_back(text.substr(at, end - at));
        at = end + 1;
    }
    return result;
}
void pathCheck(const std::string &path) {
    if (path.empty() || path.size() > 4096 || path.front() == '/' || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos || path.find('\0') != std::string::npos)
        throw std::runtime_error("Submodule path must be an ordinary repository-relative path");
    size_t at = 0;
    while (at < path.size()) {
        const auto end = path.find('/', at);
        auto part = path.substr(at, end == std::string::npos ? end : end - at);
        std::transform(part.begin(), part.end(), part.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (part.empty() || part == "." || part == ".." || part == ".git" || part == ".cycode" ||
            part.back() == '.' || part.back() == ' ')
            throw std::runtime_error("Submodule path crosses metadata or a nonportable path component");
        if (end == std::string::npos)
            break;
        at = end + 1;
        if (at == path.size())
            throw std::runtime_error("Submodule paths cannot end in a separator");
    }
}
bool control(const std::string &value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
void addressCheck(const std::string &url) {
    if (url.empty() || url.size() > 4096 || control(url) ||
        std::isspace(static_cast<unsigned char>(url.front())) ||
        std::isspace(static_cast<unsigned char>(url.back())) || url.front() == '-' ||
        url.find_first_of("?#") != std::string::npos)
        throw std::runtime_error(
            "Use a repository address without helper commands, query tokens or fragments");
    const auto schemeEnd = url.find("://"), firstColon = url.find(':');
    if (schemeEnd == std::string::npos && firstColon != std::string::npos &&
        url.substr(firstColon, 2) == "::" && url.substr(0, firstColon).find('@') == std::string::npos)
        throw std::runtime_error("Custom Git helper command addresses are unsupported");
    if (schemeEnd != std::string::npos) {
        auto scheme = url.substr(0, schemeEnd);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (scheme != "https" && scheme != "http" && scheme != "ssh" && scheme != "git" && scheme != "file")
            throw std::runtime_error("Unsupported repository URL scheme");
        const auto authority = url.substr(schemeEnd + 3, url.find('/', schemeEnd + 3) - (schemeEnd + 3));
        if (authority.empty() && scheme != "file")
            throw std::runtime_error("Repository URL has no host");
        if (const auto userEnd = authority.rfind('@'); userEnd != std::string::npos) {
            const auto user = authority.substr(0, userEnd);
            if (scheme != "ssh" || user.empty() || userEnd + 1 == authority.size() ||
                user.find_first_of(":%@") != std::string::npos)
                throw std::runtime_error(
                    "Use Git credentials/SSH authentication instead of credentials embedded in URLs");
        }
    } else if (const auto userEnd = url.rfind('@');
               userEnd != std::string::npos && url.find(':', userEnd) != std::string::npos) {
        const auto user = url.substr(0, userEnd);
        if (user.empty() || user.find_first_of(":/%@\\") != std::string::npos)
            throw std::runtime_error("Invalid SSH repository address");
    }
}
bool objectId(const std::string &value) {
    return (value.size() == 40 || value.size() == 64) &&
           value.find_first_not_of("0123456789abcdef") == std::string::npos &&
           value.find_first_not_of('0') != std::string::npos;
}
} // namespace
nlohmann::json gitSubmoduleAddModel(const nlohmann::json &parameters) {
    if (!parameters.is_object() || parameters.dump().size() > 16384)
        throw std::runtime_error("Submodule add request exceeds 16 KiB");
    auto string = [&](const char *key, size_t limit, bool required = false) {
        const auto value = parameters.value(key, Json(""));
        if (!value.is_string() || value.get_ref<const std::string &>().size() > limit ||
            (required && value.get_ref<const std::string &>().empty()) ||
            value.get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error(std::string("Invalid submodule add field: ") + key);
        return value.get<std::string>();
    };
    const auto url = string("url", 4096, true), path = string("path", 4096, true);
    auto name = string("name", 512);
    const auto branch = string("branch", 256);
    pathCheck(path);
    if (name.empty())
        name = path;
    pathCheck(name);
    if (name.size() > 512 || control(name) || control(branch) || (!branch.empty() && branch.front() == '-') ||
        branch.find("@{") != std::string::npos)
        throw std::runtime_error("Choose an ordinary module name and a literal branch name");
    addressCheck(url);
    Json arguments = Json::array({"submodule", "add", "--name", name});
    if (!branch.empty()) {
        arguments.push_back("--branch");
        arguments.push_back(branch);
    }
    arguments.push_back("--");
    arguments.push_back(url);
    arguments.push_back(path);
    return {{"request", {{"url", url}, {"path", path}, {"name", name}, {"branch", branch}}},
            {"arguments", arguments}};
}
nlohmann::json gitSubmoduleSettingsModel(const nlohmann::json &parameters) {
    if (!parameters.is_object() || parameters.dump().size() > 16384)
        throw std::runtime_error("Submodule settings request exceeds 16 KiB");
    const auto path = parameters.at("path").get<std::string>();
    const auto setting = parameters.at("setting").get<std::string>();
    const auto value = parameters.at("value").get<std::string>();
    pathCheck(path);
    Json arguments;
    if (setting == "url") {
        addressCheck(value);
        arguments = Json::array({"submodule", "set-url", "--quiet", "--", path, value});
    } else if (setting == "branch") {
        if (value.size() > 256 || control(value) || (!value.empty() && value.front() == '-') ||
            value.find("@{") != std::string::npos)
            throw std::runtime_error("Use a literal tracking branch, '.' or an empty remote-default value");
        arguments = Json::array({"submodule", "set-branch"});
        if (value.empty())
            arguments.push_back("--default");
        else {
            arguments.push_back("--branch");
            arguments.push_back(value);
        }
        arguments.push_back("--");
        arguments.push_back(path);
    } else
        throw std::runtime_error("Unsupported submodule setting");
    return {{"request", {{"path", path}, {"setting", setting}, {"value", value}}}, {"arguments", arguments}};
}
std::string gitSubmoduleSettingsRemainder(const std::string &configuration, const std::string &key) {
    std::string result;
    for (const auto &record : records(configuration)) {
        if (record.empty())
            throw std::runtime_error("Empty Git configuration key");
        if (record.substr(0, record.find('\n')) != key) {
            result += record;
            result += '\0';
        }
    }
    return result;
}
nlohmann::json parseGitSubmodules(const std::string &configuration, const std::string &index) {
    Json result = Json::array();
    std::set<std::string> names, paths;
    for (const auto &record : records(configuration)) {
        const auto separator = record.find('\n');
        const auto key = record.substr(0, separator);
        if (separator == std::string::npos || !key.starts_with("submodule.") || !key.ends_with(".path") ||
            key.size() <= 15)
            throw std::runtime_error("Invalid Git submodule path configuration");
        const auto name = key.substr(10, key.size() - 15), path = record.substr(separator + 1);
        pathCheck(path);
        auto folded = path;
        std::transform(folded.begin(), folded.end(), folded.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (name.size() > 512 || !names.insert(name).second || !paths.insert(folded).second ||
            result.size() >= 64)
            throw std::runtime_error("Duplicate or oversized submodule configuration");
        result.push_back({{"name", name}, {"path", path}, {"expectedCommit", ""}, {"state", "notIndexed"}});
    }
    std::map<std::string, std::set<char>> stages;
    for (const auto &record : records(index)) {
        const auto tab = record.find('\t');
        const auto header = record.substr(0, tab);
        const auto space = header.find(' ', 7);
        if (tab == std::string::npos || header.size() < 10 || header[6] != ' ' ||
            space == std::string::npos || space + 2 != header.size() || header.back() < '0' ||
            header.back() > '3' || header.substr(0, 6).find_first_not_of("01234567") != std::string::npos ||
            !objectId(header.substr(7, space - 7)))
            throw std::runtime_error("Invalid Git submodule index record");
        if (!header.starts_with("160000 "))
            continue;
        const auto path = record.substr(tab + 1);
        for (auto &entry : result) {
            if (entry.at("path") != path)
                continue;
            const char stage = header.back();
            if (!stages[path].insert(stage).second || (stage == '0' && stages[path].size() > 1) ||
                (stage != '0' && stages[path].contains('0')))
                throw std::runtime_error("Duplicate or mixed Git submodule index stages");
            entry["state"] = stage == '0' ? "uninitialized" : "conflicted";
            entry["expectedCommit"] = stage == '0' ? header.substr(7, space - 7) : "";
        }
    }
    // A nested configured path cannot be treated as an independent top-level checkout.
    for (const auto &a : paths)
        for (const auto &b : paths)
            if (a != b && b.starts_with(a + "/"))
                throw std::runtime_error("Overlapping top-level submodule paths");
    return result;
}
std::vector<std::string> gitSubmoduleArguments(const std::string &action, const std::string &path,
                                               bool fetch) {
    pathCheck(path);
    std::vector<std::string> args = {"submodule"};
    if (action == "update") {
        args.insert(args.end(), {"update", "--init", "--checkout", "--jobs=1"});
        if (!fetch)
            args.push_back("--no-fetch");
    } else if (action == "deinit" && !fetch)
        args.insert(args.end(), {"deinit", "--quiet"});
    else if ((action == "init" || action == "sync") && !fetch)
        args.push_back(action);
    else
        throw std::runtime_error("Unsupported submodule action or fetch option");
    args.insert(args.end(), {"--", path});
    return args;
}
nlohmann::json gitSubmoduleDeinitModel(const nlohmann::json &entry, const std::string &index,
                                       const std::string &flags) {
    const auto name = entry.at("name").get<std::string>();
    pathCheck(name);
    if (name.size() > 512 || entry.at("state") != "matching" || entry.at("registered") != true ||
        entry.at("checkedOut") != true || entry.at("dirty") != false ||
        !objectId(entry.at("expectedCommit")) || entry.at("head") != entry.at("expectedCommit"))
        throw std::runtime_error(
            "Deinitialization requires a registered clean checkout at its recorded commit");
    std::set<std::string> paths;
    for (const auto &record : records(index)) {
        const auto tab = record.find('\t');
        if (tab == std::string::npos)
            throw std::runtime_error("Invalid submodule file index");
        const auto header = record.substr(0, tab), path = record.substr(tab + 1);
        const auto space = header.find(' ', 7);
        if (space == std::string::npos || header[6] != ' ' || header.substr(space) != " 0" ||
            (header.substr(0, 6) != "100644" && header.substr(0, 6) != "100755" &&
             header.substr(0, 6) != "120000") ||
            !objectId(header.substr(7, space - 7)) || path.empty() || !paths.insert(path).second)
            throw std::runtime_error("Resolve conflicts and nested submodules before deinitializing");
    }
    auto remaining = paths;
    for (const auto &record : records(flags))
        if (!record.starts_with("H ") || remaining.erase(record.substr(2)) != 1)
            throw std::runtime_error(
                "Hidden index flags or incomplete file inspection block deinitialization");
    if (!remaining.empty())
        throw std::runtime_error("Incomplete submodule file inspection");
    return Json{{"arguments", gitSubmoduleArguments("deinit", entry.at("path"))},
                {"trackedFiles", paths.size()}};
}
} // namespace cycode

#include "cycode/GitConflicts.h"
#include "cycode/GitMerge.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
bool oid(const std::string &text) {
    return (text.size() == 40 || text.size() == 64) &&
           text.find_first_not_of("0123456789abcdef") == std::string::npos;
}
} // namespace
nlohmann::json parseGitMergeHeads(const std::string &text) {
    if (text.size() > 4096)
        throw std::runtime_error("Merge parents exceed their metadata limit");
    Json result = Json::array();
    std::set<std::string> seen;
    size_t at = 0;
    while (at < text.size()) {
        auto end = text.find('\n', at);
        auto value = text.substr(at, end == std::string::npos ? end : end - at);
        if (!value.empty() && value.back() == '\r')
            value.pop_back();
        if (!oid(value) || !seen.insert(value).second || result.size() >= 16)
            throw std::runtime_error("Invalid or duplicate Git merge parent");
        result.push_back(value);
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
    return result;
}
nlohmann::json parseGitMergeCommitParents(const std::string &raw) {
    const auto headerEnd = raw.find("\n\n");
    if (raw.size() > 8 * 1024 * 1024 || headerEnd == std::string::npos || !raw.starts_with("tree "))
        throw std::runtime_error("Invalid raw Git commit header");
    const auto treeEnd = raw.find('\n');
    if (!oid(raw.substr(5, treeEnd - 5)))
        throw std::runtime_error("Invalid raw Git commit tree");
    std::string parents;
    size_t at = treeEnd + 1;
    while (at < headerEnd) {
        const auto end = raw.find('\n', at);
        const auto line = raw.substr(at, end - at);
        if (line.starts_with("parent "))
            parents += line.substr(7) + "\n";
        at = end + 1;
    }
    return parseGitMergeHeads(parents);
}
nlohmann::json parseGitMergeChanges(const std::string &raw) {
    if (raw.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Staged change listing exceeds 2 MiB");
    Json result = Json::array();
    std::set<std::string> seen;
    size_t at = 0;
    while (at < raw.size()) {
        const auto split = raw.find('\0', at);
        if (split == std::string::npos || split != at + 1 ||
            std::string("ADMTU").find(raw[at]) == std::string::npos)
            throw std::runtime_error("Invalid non-renaming staged change record");
        const auto end = raw.find('\0', split + 1);
        if (end == std::string::npos)
            throw std::runtime_error("Incomplete staged change path");
        const auto path = raw.substr(split + 1, end - split - 1);
        if (path.empty() || path.size() > 4096 || !seen.insert(path).second || result.size() >= 2048)
            throw std::runtime_error("Invalid, duplicate or excessive staged paths");
        (void)Json(path).dump();
        result.push_back({{"status", raw.substr(at, 1)}, {"path", path}});
        at = end + 1;
    }
    return result;
}
nlohmann::json parseGitMergeTree(const std::string &raw) {
    if (raw.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Committed tree listing exceeds 8 MiB");
    std::string index;
    size_t at = 0;
    while (at < raw.size()) {
        const auto end = raw.find('\0', at), tab = raw.find('\t', at);
        if (end == std::string::npos || tab == std::string::npos || tab >= end)
            throw std::runtime_error("Incomplete committed tree record");
        const auto header = raw.substr(at, tab - at);
        const auto space = header.find(' ', 7);
        if (header.size() < 9 || header[6] != ' ' || space == std::string::npos)
            throw std::runtime_error("Invalid committed tree record");
        const auto type = header.substr(7, space - 7);
        if (type != "blob" && type != "commit")
            throw std::runtime_error("Unsupported recursive committed tree entry");
        index += header.substr(0, 6) + " " + header.substr(space + 1) + " 0\t" +
                 raw.substr(tab + 1, end - tab - 1) + '\0';
        at = end + 1;
    }
    return parseGitConflictIndex(index);
}
nlohmann::json gitMergeRequest(const Json &parameters) {
    const auto action = parameters.at("action").get<std::string>();
    if (action != "commit" && action != "quit")
        throw std::runtime_error("Choose commit or quit explicitly");
    const auto message = parameters.value("message", std::string{});
    if (message.size() > 16384 || message.find('\0') != std::string::npos)
        throw std::runtime_error("Merge message must be UTF-8 text up to 16 KiB");
    (void)Json(message).dump();
    if (parameters.contains("runHooks") && !parameters.at("runHooks").is_boolean())
        throw std::runtime_error("runHooks must be boolean");
    const bool hooks = parameters.value("runHooks", false);
    if (action == "quit" && (!message.empty() || hooks))
        throw std::runtime_error("Quit does not accept a message or hooks");
    if (action == "commit" && std::none_of(message.begin(), message.end(), [](unsigned char c) {
            return c != ' ' && c != '\t' && c != '\r' && c != '\n';
        }))
        throw std::runtime_error("Enter a nonempty merge commit message");
    return {{"action", action}, {"message", message}, {"runHooks", hooks}};
}
} // namespace cycode

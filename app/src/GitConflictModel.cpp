#include "cycode/GitConflicts.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
bool objectId(const std::string &id) {
    return (id.size() == 40 || id.size() == 64) &&
           id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::string portablePath(const Json &value) {
    if (!value.is_string())
        throw std::runtime_error("Conflict path must be text");
    auto path = value.get<std::string>();
    if (path.empty() || path.size() > 4096 || path.front() == '/' ||
        path.find_first_of("\\:") != std::string::npos ||
        std::any_of(path.begin(), path.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error("Conflict resolution needs a portable repository-relative path");
    (void)Json(path).dump();
    size_t at = 0;
    while (at < path.size()) {
        const auto end = path.find('/', at);
        auto part = path.substr(at, end == std::string::npos ? end : end - at);
        std::transform(part.begin(), part.end(), part.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (part.empty() || part == "." || part == ".." || part == ".git" || part.back() == '.' ||
            part.back() == ' ')
            throw std::runtime_error("Ambiguous or administrative conflict path");
        if (end == std::string::npos)
            break;
        at = end + 1;
        if (at == path.size())
            throw std::runtime_error("Conflict path must name a file");
    }
    return path;
}
std::string folded(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
} // namespace
nlohmann::json parseGitConflictIndex(const std::string &raw) {
    if (raw.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Git index listing exceeds 8 MiB");
    Json records = Json::object();
    size_t at = 0, count = 0;
    while (at < raw.size()) {
        const auto end = raw.find('\0', at), tab = raw.find('\t', at);
        if (end == std::string::npos || tab == std::string::npos || tab >= end || ++count > 100000)
            throw std::runtime_error("Incomplete or oversized Git index listing");
        const auto header = raw.substr(at, tab - at);
        const auto first = header.find(' '), last = header.rfind(' ');
        if (first != 6 || first == last || last + 2 != header.size() || header.back() < '0' ||
            header.back() > '3')
            throw std::runtime_error("Malformed Git index record");
        const auto mode = header.substr(0, first), oid = header.substr(first + 1, last - first - 1);
        if ((mode != "100644" && mode != "100755" && mode != "120000" && mode != "160000" &&
             mode != "040000") ||
            !objectId(oid))
            throw std::runtime_error("Unsupported Git index mode or object ID");
        const auto path = raw.substr(tab + 1, end - tab - 1), stage = header.substr(last + 1);
        if (path.empty() || path.size() > 4096)
            throw std::runtime_error("Invalid Git index path");
        (void)Json(path).dump();
        if (!records.contains(path))
            records[path] = Json::object();
        if (records[path].contains(stage))
            throw std::runtime_error("Duplicate Git index stage");
        records[path][stage] = {{"mode", mode}, {"oid", oid}};
        at = end + 1;
    }
    for (const auto &[path, stages] : records.items())
        if (stages.contains("0") && stages.size() != 1)
            throw std::runtime_error("Mixed resolved and unresolved Git index stages");
    return records;
}
nlohmann::json gitConflictChoices(const Json &choices) {
    if (!choices.is_array() || choices.empty() || choices.size() > 32)
        throw std::runtime_error("Review between one and 32 conflict paths");
    std::set<std::string> seen;
    Json result = Json::array();
    size_t bytes = 0;
    for (const auto &item : choices) {
        if (!item.is_object() || item.size() != 2 || !item.contains("path") || !item.contains("choice") ||
            !item.at("choice").is_string())
            throw std::runtime_error("Each conflict choice needs only path and choice");
        const auto path = portablePath(item.at("path")), key = folded(path);
        const auto choice = item.at("choice").get<std::string>();
        if (choice != "ours" && choice != "theirs" && choice != "base" && choice != "delete")
            throw std::runtime_error("Choose ours, theirs, base or delete");
        for (const auto &old : seen)
            if (key == old || key.starts_with(old + "/") || old.starts_with(key + "/"))
                throw std::runtime_error("Duplicate, case-aliased or overlapping conflict paths");
        seen.insert(key);
        bytes += path.size();
        if (bytes > 12000)
            throw std::runtime_error("Conflict batch exceeds the portable command-line budget");
        result.push_back({{"path", path}, {"choice", choice}});
    }
    return result;
}
nlohmann::json gitConflictIndexPlan(const Json &entries, const Json &choices) {
    const auto request = gitConflictChoices(choices);
    Json changes = Json::array(), arguments = Json::array({"update-index", "--add"}),
         removals = Json::array();
    for (const auto &item : request) {
        const auto path = item.at("path").get<std::string>(), choice = item.at("choice").get<std::string>();
        const Json *entry = nullptr;
        for (const auto &candidate : entries)
            if (candidate.at("path") == path) {
                entry = &candidate;
                break;
            }
        if (!entry || !entry->at("supported").get<bool>())
            throw std::runtime_error("Conflict path is unavailable or unsupported");
        Json version = choice == "delete" ? Json(nullptr) : entry->at(choice);
        if (version.is_null())
            removals.push_back(path);
        else {
            arguments.push_back("--cacheinfo");
            arguments.push_back(version.at("mode"));
            arguments.push_back(version.at("oid"));
            arguments.push_back(path);
        }
        changes.push_back(
            {{"path", path}, {"choice", choice}, {"version", version}, {"deleted", version.is_null()}});
    }
    if (!removals.empty()) {
        arguments.push_back("--force-remove");
        arguments.push_back("--");
        for (const auto &path : removals)
            arguments.push_back(path);
    }
    return {{"choices", request},       {"changes", changes},
            {"arguments", arguments},   {"preservesWorkingFiles", true},
            {"preservesBuffers", true}, {"mayContactRemote", false}};
}
} // namespace cycode

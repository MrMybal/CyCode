#include "cycode/Solution.h"
#include "cycode/Process.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
namespace cycode {
using Json = nlohmann::json;
static std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
static std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
bool isSolutionPath(const std::filesystem::path &path) {
    const auto extension = lower(pathUtf8(path.extension()));
    return extension == ".sln" || extension == ".slnx";
}
static std::filesystem::path solutionFile(const std::filesystem::path &entry) {
    if (!std::filesystem::is_directory(entry)) {
        const auto extension = lower(pathUtf8(entry.extension()));
        if (std::filesystem::is_regular_file(entry) && (extension == ".vcxproj" || extension == ".csproj" || extension == ".fsproj" || extension == ".vbproj")) return {};
        if (std::filesystem::is_regular_file(entry) && lower(pathUtf8(entry.extension())) == ".uproject") {
            auto solution = entry;
            solution.replace_extension(".sln");
            if (!std::filesystem::is_regular_file(solution)) solution.replace_extension(".slnx");
            return std::filesystem::is_regular_file(solution) ? std::filesystem::canonical(solution)
                                                              : std::filesystem::path{};
        }
        if (!std::filesystem::is_regular_file(entry) || !isSolutionPath(entry))
            throw std::runtime_error("Open a workspace directory, .sln/.slnx solution or Unreal .uproject");
        return std::filesystem::canonical(entry);
    }
    std::filesystem::path found;
    for (const auto &item : std::filesystem::directory_iterator(entry))
        if (item.is_regular_file() && isSolutionPath(item.path())) {
            if (!found.empty())
                return {}; // Several solutions: let the user select one explicitly.
            found = item.path();
        }
    return found.empty() ? found : std::filesystem::canonical(found);
}
std::filesystem::path workspaceRoot(const std::filesystem::path &entry) {
    if (std::filesystem::is_directory(entry))
        return std::filesystem::canonical(entry);
    if (std::filesystem::is_regular_file(entry) && lower(pathUtf8(entry.extension())) == ".uproject")
        return std::filesystem::canonical(entry).parent_path();
    const auto extension = lower(pathUtf8(entry.extension()));
    if (std::filesystem::is_regular_file(entry) && (extension == ".vcxproj" || extension == ".csproj" || extension == ".fsproj" || extension == ".vbproj")) return std::filesystem::canonical(entry).parent_path();
    return solutionFile(entry).parent_path();
}
static std::string readText(const std::filesystem::path &path) {
    const auto size = std::filesystem::file_size(path);
    if (size > 16 * 1024 * 1024)
        throw std::runtime_error("Solution exceeds 16 MiB");
    std::ifstream in(path, std::ios::binary);
    std::string text(static_cast<size_t>(size), '\0');
    if (!in.read(text.data(), static_cast<std::streamsize>(size)))
        throw std::runtime_error("Cannot read solution");
    if (text.starts_with("\xef\xbb\xbf"))
        text.erase(0, 3);
    if (text.starts_with("\xff\xfe") || text.starts_with("\xfe\xff")) {
        const bool le = text[0] == char(0xff);
        if (text.size() % 2)
            throw std::runtime_error("Invalid UTF-16 solution");
        std::string utf8;
        auto unit = [&](size_t i) {
            auto a = static_cast<unsigned char>(text[i]), b = static_cast<unsigned char>(text[i + 1]);
            return unsigned(le ? a | (b << 8) : b | (a << 8));
        };
        for (size_t i = 2; i < text.size(); i += 2) {
            unsigned cp = unit(i);
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 3 >= text.size())
                    throw std::runtime_error("Invalid UTF-16 solution");
                auto tail = unit(i + 2);
                if (tail < 0xdc00 || tail > 0xdfff)
                    throw std::runtime_error("Invalid UTF-16 solution");
                cp = 0x10000 + ((cp - 0xd800) << 10) + tail - 0xdc00;
                i += 2;
            } else if (cp >= 0xdc00 && cp <= 0xdfff)
                throw std::runtime_error("Invalid UTF-16 solution");
            if (cp < 0x80)
                utf8 += char(cp);
            else if (cp < 0x800) {
                utf8 += char(0xc0 | (cp >> 6));
                utf8 += char(0x80 | (cp & 63));
            } else if (cp < 0x10000) {
                utf8 += char(0xe0 | (cp >> 12));
                utf8 += char(0x80 | ((cp >> 6) & 63));
                utf8 += char(0x80 | (cp & 63));
            } else {
                utf8 += char(0xf0 | (cp >> 18));
                utf8 += char(0x80 | ((cp >> 12) & 63));
                utf8 += char(0x80 | ((cp >> 6) & 63));
                utf8 += char(0x80 | (cp & 63));
            }
        }
        text = std::move(utf8);
    }
    (void)Json(text).dump(); // Validate UTF-8 before it reaches the GUI or MCP.
    return text;
}
Json solutionReference(const std::filesystem::path &root, std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    auto path = utf8Path(value);
    // Keep Windows absolute references external when inspecting the same solution on Unix.
    const bool foreignDrive = !path.is_absolute() && value.size() >= 3 && value[1] == ':' && value[2] == '/';
    auto full =
        foreignDrive ? path : std::filesystem::weakly_canonical(path.is_absolute() ? path : root / path);
    auto relative = full.lexically_relative(root);
    bool local = !foreignDrive && !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
    return {{"path", pathUtf8(full)},
            {"relativePath", local ? pathUtf8(relative) : ""},
            {"exists", !foreignDrive && std::filesystem::is_regular_file(full)},
            {"external", !local}};
}
Json inspectSolution(const std::filesystem::path &entry) {
    const auto file = solutionFile(entry);
    if (file.empty())
        return nullptr;
    if (lower(pathUtf8(file.extension())) == ".slnx")
        return inspectXmlSolution(file, readText(file));
    std::istringstream input(readText(file));
    std::string line, section;
    bool header = false;
    Json projects = Json::array(), configurations = Json::array();
    std::map<std::string, std::string> parents;
    std::map<std::string, Json> mappings;
    std::set<std::string> ids;
    const std::regex project(
        R"re(^Project\("(\{[^}]+\})"\)\s*=\s*"([^"]*)",\s*"([^"]*)",\s*"(\{[^}]+\})")re");
    const std::regex nesting(R"re(^(\{[^}]+\})\s*=\s*(\{[^}]+\}))re");
    const std::regex mapping(R"re(^(\{[^}]+\})\.(.+)\.(ActiveCfg|Build\.0|Deploy\.0)\s*=\s*(.+)$)re");
    int current = -1;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.starts_with("Microsoft Visual Studio Solution File, Format Version"))
            header = true;
        std::smatch match;
        if (std::regex_search(line, match, project)) {
            const auto id = lower(match[4]);
            if (!ids.insert(id).second)
                throw std::runtime_error("Duplicate solution project identifier");
            bool folder = lower(match[1]) == "{2150e333-8fdc-42a3-9474-1a3956d46de8}";
            auto item = folder
                            ? Json{{"path", ""}, {"relativePath", ""}, {"exists", true}, {"external", false}}
                            : solutionReference(file.parent_path(), match[3]);
            item.update({{"id", id},
                         {"name", match[2].str()},
                         {"type", match[1].str()},
                         {"folder", folder},
                         {"parent", ""},
                         {"items", Json::array()}});
            projects.push_back(std::move(item));
            current = int(projects.size()) - 1;
            if (projects.size() > 10000)
                throw std::runtime_error("Solution exceeds 10,000 projects");
        } else if (line == "EndProject") {
            current = -1;
            section.clear();
        } else if (line.starts_with("GlobalSection(NestedProjects)"))
            section = "nested";
        else if (line.starts_with("GlobalSection(SolutionConfigurationPlatforms)"))
            section = "configurations";
        else if (line.starts_with("GlobalSection(ProjectConfigurationPlatforms)"))
            section = "project-configurations";
        else if (line.starts_with("ProjectSection(SolutionItems)"))
            section = "items";
        else if (line.starts_with("EndGlobalSection") || line.starts_with("EndProjectSection"))
            section.clear();
        else if (section == "nested" && std::regex_search(line, match, nesting))
            parents[lower(match[1])] = lower(match[2]);
        else if (section == "configurations" && line.find('=') != std::string::npos)
            configurations.push_back(trim(line.substr(0, line.find('='))));
        else if (section == "project-configurations" && std::regex_match(line,match,mapping)) {
            auto &projectMap = mappings[lower(match[1])]; if (projectMap.is_null()) projectMap = Json::object();
            const auto key = trim(match[2]); auto &value = projectMap[key];
            if (value.is_null()) value = {{"configuration",""},{"platform",""},{"build",false},{"deploy",false}};
            if (match[3] == "ActiveCfg") { auto active = trim(match[4]); auto split = active.find('|');
                if (split == std::string::npos) throw std::runtime_error("Invalid solution project configuration");
                value["configuration"] = trim(active.substr(0,split)); value["platform"] = trim(active.substr(split+1));
            } else value[match[3] == "Build.0" ? "build" : "deploy"] = true;
        }
        else if (section == "items" && current >= 0 && line.find('=') != std::string::npos)
            projects[size_t(current)]["items"].push_back(
                solutionReference(file.parent_path(), trim(line.substr(0, line.find('=')))));
    }
    if (!header)
        throw std::runtime_error("Invalid Visual Studio solution header");
    std::set<std::string> folders;
    for (const auto &p : projects)
        if (p.at("folder"))
            folders.insert(p.at("id"));
    size_t missing = 0, external = 0;
    for (auto &p : projects) {
        const std::string id = p.at("id");
        p["configurations"] = mappings.contains(id) ? mappings.at(id) : Json::object();
        auto parent = parents.find(id);
        if (parent != parents.end() && folders.contains(parent->second)) {
            std::set<std::string> visited{id};
            auto ancestor = parent->second;
            for (;;) {
                if (!visited.insert(ancestor).second)
                    throw std::runtime_error("Cycle in solution folders");
                auto next = parents.find(ancestor);
                if (next == parents.end())
                    break;
                ancestor = next->second;
            }
            p["parent"] = parent->second;
        }
        if (!p.at("folder").get<bool>()) {
            missing += !p.at("exists").get<bool>();
            external += p.at("external").get<bool>();
        }
    }
    return {{"path", pathUtf8(file)},     {"name", pathUtf8(file.stem())},
            {"projects", projects},       {"configurations", configurations},
            {"missingProjects", missing}, {"externalProjects", external}};
}
} // namespace cycode

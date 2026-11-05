#include "cycode/Paths.h"
#include "cycode/Solution.h"
#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <tinyxml2.h>

namespace cycode {
namespace {
using Json = nlohmann::json;
using Element = tinyxml2::XMLElement;
std::string folded(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}
std::string attribute(const Element &element, const char *name, const std::string &fallback = "") {
    const auto *value = element.Attribute(name);
    return value ? value : fallback;
}
std::string required(const Element &element, const char *name) {
    auto value = attribute(element, name);
    if (value.empty() || value.size() > 8192)
        throw std::runtime_error(std::string("Invalid .slnx ") + name + " attribute");
    return value;
}
std::string slashes(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}
std::string typeKey(std::string value) {
    value = folded(std::move(value));
    if (value.size() > 2 && value.front() == '{' && value.back() == '}')
        value = value.substr(1, value.size() - 2);
    return value;
}
std::string trimmed(const std::string &value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? ""
                                      : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
enum class Kind { identity, managed, native, nonbuild, noplatform };
std::string platformKey(std::string value);
Kind builtin(const std::string &value) {
    const auto key = typeKey(value);
    static const std::set<std::string> managed = {".csproj",
                                                  ".fsproj",
                                                  ".vbproj",
                                                  ".webproj",
                                                  "c#",
                                                  "common c#",
                                                  "f#",
                                                  "common f#",
                                                  "vb",
                                                  "common vb",
                                                  "website",
                                                  "fae04ec0-301f-11d3-bf4b-00c04f79efbc",
                                                  "9a19103f-16f7-4668-be54-9a1e7a4f7556",
                                                  "f184b08f-c81c-45f6-a57f-5abd9991f28f",
                                                  "778dae3c-4631-46ea-aa77-85c1314464d9",
                                                  "f2a71f9b-5d33-465a-a702-920d77279786",
                                                  "6ec3ee1d-3c4e-46dd-8f32-0cc8e7565705"};
    static const std::set<std::string> nonbuild = {
        ".shproj",  ".vcxitems", ".exe",        ".esproj",    ".njsproj", ".deployproj",     ".vsproj",
        ".wixproj", ".sqlproj",  ".usqldbproj", ".usqlproj",  ".sfproj",  ".ccproj",         ".dcproj",
        "shared",   "exe",       "folder",      "javascript", "node.js",  "deploy",          "installer",
        "wix",      "sql",       "u-sql-db",    "u-sql",      "fabric",   "cloud computing", "docker"};
    if (managed.contains(key))
        return Kind::managed;
    if (key == ".vcxproj" || key == "vc" || key == "8bc9ceb8-8b4a-11d0-8d11-00a0c91bc942")
        return Kind::native;
    if (nonbuild.contains(key))
        return Kind::nonbuild;
    if (key == ".rptproj" || key == "ssrs")
        return Kind::noplatform;
    return Kind::identity;
}
void applyBuiltin(Json &mapping, Kind kind, const std::string &platform) {
    if (kind == Kind::managed)
        mapping["platform"] = "Any CPU";
    if (kind == Kind::native)
        mapping["platform"] = platformKey(platform) == "any cpu" ? "x64"
                              : folded(platform) == "x86"        ? "Win32"
                                                                 : platform;
    if (kind == Kind::nonbuild)
        mapping["build"] = false;
    if (kind == Kind::noplatform)
        mapping["platform"] = "";
}
std::string platformKey(std::string value) {
    value = folded(std::move(value));
    return value == "anycpu" ? "any cpu" : value;
}
bool boolean(const std::string &value) {
    const auto key = folded(value);
    if (key != "true" && key != "false")
        throw std::runtime_error("Invalid .slnx boolean rule");
    return key == "true";
}
std::string identifier(const Element &element, const std::string &fallback) {
    auto value = attribute(element, "Id");
    if (value.empty())
        return fallback;
    static const std::regex guid(
        R"(\{?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}?)");
    if (!std::regex_match(value, guid) || (value.front() == '{') != (value.back() == '}'))
        throw std::runtime_error("Invalid .slnx item identifier");
    if (value.front() != '{')
        value = "{" + value + "}";
    return folded(value);
}
struct Rule {
    std::string dimension, configuration, platform, value;
};
std::vector<Rule> rules(const Element &element) {
    std::vector<Rule> result;
    std::set<std::string> keys;
    for (auto *child = element.FirstChildElement(); child; child = child->NextSiblingElement()) {
        const std::string kind = child->Name();
        if (kind != "BuildType" && kind != "Platform" && kind != "Build" && kind != "Deploy")
            continue;
        const auto selector = attribute(*child, "Solution");
        auto configuration = std::string("*"), platform = configuration;
        if (!selector.empty()) {
            auto split = selector.find('|');
            if (split == std::string::npos || split == 0 || split == selector.size() - 1 ||
                selector.find('|', split + 1) != std::string::npos)
                throw std::runtime_error("Invalid .slnx solution rule selector");
            configuration = trimmed(selector.substr(0, split));
            platform = trimmed(selector.substr(split + 1));
            if (configuration.empty() || platform.empty())
                throw std::runtime_error("Empty .slnx solution rule dimension");
        }
        const auto key = kind + ":" + folded(configuration) + "|" + platformKey(platform);
        if (!keys.insert(key).second)
            throw std::runtime_error("Duplicate .slnx configuration rule");
        auto value = attribute(*child, "Project", kind == "Build" || kind == "Deploy" ? "true" : "");
        if (value.empty())
            throw std::runtime_error("Missing .slnx project configuration value");
        if (kind == "Build" || kind == "Deploy")
            boolean(value);
        else if (value.find('|') != std::string::npos)
            throw std::runtime_error("Invalid .slnx project dimension");
        result.push_back({kind, configuration, platform, value});
        if (result.size() > 1024)
            throw std::runtime_error("Too many .slnx configuration rules");
    }
    return result;
}
void applyRules(Json &mapping, const std::vector<Rule> &list, const std::string &configuration,
                const std::string &platform) {
    for (const auto &rule : list) {
        if ((rule.configuration != "*" && folded(rule.configuration) != folded(configuration)) ||
            (rule.platform != "*" && platformKey(rule.platform) != platformKey(platform)))
            continue;
        if (rule.dimension == "Build")
            mapping["build"] = boolean(rule.value);
        else if (rule.dimension == "Deploy")
            mapping["deploy"] = boolean(rule.value);
        else if (rule.dimension == "BuildType")
            mapping["configuration"] = rule.value == "*" ? configuration : rule.value;
        else
            mapping["platform"] = rule.value == "*" ? platform : rule.value;
    }
}
struct ProjectType {
    std::string name, extension, id, base;
    std::vector<Rule> rules;
    std::optional<bool> buildable, platforms;
};
} // namespace
Json inspectXmlSolution(const std::filesystem::path &file, const std::string &text) {
    if (text.find('\0') != std::string::npos)
        throw std::runtime_error("NUL in .slnx document");
    tinyxml2::XMLDocument document;
    if (document.Parse(text.data(), text.size()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error(std::string("Invalid .slnx XML: ") + document.ErrorStr());
    const auto *root = document.RootElement();
    if (!root || std::string(root->Name()) != "Solution" || root->NextSiblingElement())
        throw std::runtime_error("Expected one .slnx Solution root");
    size_t nodes = 0;
    std::function<void(const tinyxml2::XMLNode &, size_t)> validate = [&](const auto &node, size_t depth) {
        if (++nodes > 100000 || depth > 32)
            throw std::runtime_error(".slnx XML exceeds node/depth limits");
        if (node.ToUnknown())
            throw std::runtime_error(".slnx DTD/declarations are not supported");
        if (const auto *value = node.ToText(); value && !trimmed(value->Value()).empty())
            throw std::runtime_error("Unexpected text in .slnx document");
        for (auto *child = node.FirstChild(); child; child = child->NextSibling())
            validate(*child, depth + 1);
    };
    validate(document, 0);
    Json projects = Json::array(), configurations = Json::array();
    std::vector<std::string> buildTypes, platforms;
    std::set<std::string> buildNames, platformNames, ids, paths;
    std::vector<ProjectType> types;
    std::vector<Rule> globalRules;
    bool hasDefaultType = false;
    std::set<std::string> typeNames, typeIds, typeExtensions;
    bool hasConfigurations = false;
    for (auto *element = root->FirstChildElement("Configurations"); element;
         element = element->NextSiblingElement("Configurations")) {
        if (hasConfigurations)
            throw std::runtime_error("Duplicate .slnx Configurations");
        hasConfigurations = true;
        for (auto *child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
            const std::string kind = child->Name();
            if (kind == "BuildType" || kind == "Platform") {
                auto name = required(*child, "Name");
                if (name.find_first_of("|*\r\n") != std::string::npos)
                    throw std::runtime_error("Invalid .slnx configuration name");
                auto &names = kind == "BuildType" ? buildNames : platformNames;
                if (!names.insert(kind == "Platform" ? platformKey(name) : folded(name)).second)
                    throw std::runtime_error("Duplicate .slnx configuration name");
                (kind == "BuildType" ? buildTypes : platforms).push_back(name);
            } else if (kind == "ProjectType") {
                ProjectType type{attribute(*child, "Name"), attribute(*child, "Extension"),
                                 typeKey(attribute(*child, "TypeId")), attribute(*child, "BasedOn"),
                                 rules(*child)};
                if (!type.extension.empty() && type.extension.front() != '.')
                    type.extension = "." + type.extension;
                if (child->Attribute("IsBuildable"))
                    type.buildable = boolean(attribute(*child, "IsBuildable"));
                if (child->Attribute("SupportsPlatform"))
                    type.platforms = boolean(attribute(*child, "SupportsPlatform"));
                if (type.name.empty() && type.extension.empty() && type.id.empty()) {
                    if (hasDefaultType)
                        throw std::runtime_error("Duplicate .slnx default project rules");
                    hasDefaultType = true;
                    globalRules = type.rules;
                    if (type.buildable)
                        globalRules.insert(globalRules.begin(),
                                           {"Build", "*", "*", *type.buildable ? "true" : "false"});
                    if (type.platforms && !*type.platforms)
                        globalRules.insert(globalRules.begin(), {"Platform", "*", "*", ""});
                } else {
                    if ((!type.name.empty() && !typeNames.insert(folded(type.name)).second) ||
                        (!type.id.empty() && !typeIds.insert(type.id).second) ||
                        (!type.extension.empty() && !typeExtensions.insert(folded(type.extension)).second))
                        throw std::runtime_error("Duplicate .slnx project type");
                    types.push_back(std::move(type));
                    if (types.size() > 64)
                        throw std::runtime_error("Too many .slnx project types");
                }
            } else
                throw std::runtime_error("Unknown .slnx configuration element: " + kind);
        }
    }
    if (buildTypes.empty())
        buildTypes = {"Debug", "Release"};
    if (platforms.empty())
        platforms = {"Any CPU"};
    if (buildTypes.size() * platforms.size() > 256)
        throw std::runtime_error(".slnx exceeds 256 configurations");
    for (const auto &configuration : buildTypes)
        for (const auto &platform : platforms)
            configurations.push_back(configuration + "|" + platform);
    std::map<std::string, size_t> folders;
    auto append = [&](Json item) {
        if (!ids.insert(item.at("id")).second)
            throw std::runtime_error("Duplicate .slnx item identifier");
        if (projects.size() >= 10000)
            throw std::runtime_error(".slnx exceeds 10,000 items");
        projects.push_back(std::move(item));
        return projects.size() - 1;
    };
    std::function<std::string(std::string)> folder = [&](std::string path) -> std::string {
        path = slashes(std::move(path));
        if (std::count(path.begin(), path.end(), '/') > 65)
            throw std::runtime_error(".slnx logical folders exceed 64 levels");
        if (path.size() < 3 || path.front() != '/' || path.back() != '/')
            throw std::runtime_error(".slnx folder names must use /Folder/ paths");
        const auto key = folded(path);
        if (folders.contains(key))
            return projects[folders.at(key)].at("id");
        auto end = path.size() - 1, split = path.rfind('/', end - 1);
        const auto name = path.substr(split + 1, end - split - 1);
        if (name.empty() || name == "." || name == ".." || path.find("//") != std::string::npos)
            throw std::runtime_error("Invalid .slnx folder path");
        const auto parent = split ? folder(path.substr(0, split + 1)) : "";
        const auto id = "slnx:folder:" + key;
        folders[key] = append({{"id", id},
                               {"name", name},
                               {"path", ""},
                               {"relativePath", ""},
                               {"exists", true},
                               {"external", false},
                               {"type", "folder"},
                               {"folder", true},
                               {"parent", parent},
                               {"items", Json::array()},
                               {"configurations", Json::object()}});
        return id;
    };
    // Resolve all logical folders first, independent of the document order.
    std::set<std::string> explicitFolders;
    for (auto *element = root->FirstChildElement("Folder"); element;
         element = element->NextSiblingElement("Folder")) {
        const auto name = slashes(required(*element, "Name"));
        if (!explicitFolders.insert(folded(name)).second)
            throw std::runtime_error("Duplicate .slnx folder");
        folder(name);
    }
    // Explicit GUIDs replace host-derived folder IDs after every parent exists.
    std::map<std::string, std::string> renamed;
    for (auto *element = root->FirstChildElement("Folder"); element;
         element = element->NextSiblingElement("Folder")) {
        auto &item = projects[folders.at(folded(slashes(required(*element, "Name"))))];
        auto old = item.at("id").get<std::string>(), id = identifier(*element, old);
        if (id != old) {
            if (!ids.insert(id).second)
                throw std::runtime_error("Duplicate .slnx folder identifier");
            ids.erase(old);
            item["id"] = id;
            renamed[old] = id;
        }
    }
    for (auto &item : projects)
        if (renamed.contains(item.at("parent")))
            item["parent"] = renamed.at(item.at("parent"));
    size_t mappings = 0, missing = 0, external = 0;
    auto addProject = [&](const Element &element, const std::string &parent) {
        const auto path = slashes(required(element, "Path"));
        if (!paths.insert(folded(pathUtf8(utf8Path(path).lexically_normal()))).second)
            throw std::runtime_error("Duplicate .slnx project path");
        auto item = solutionReference(file.parent_path(), path);
        const auto extension = folded(pathUtf8(utf8Path(path).extension()));
        const auto typeName = attribute(element, "Type", extension);
        const auto id =
            identifier(element, "slnx:project:" + folded(pathUtf8(utf8Path(path).lexically_normal())));
        item.update({{"id", id},
                     {"name", attribute(element, "DisplayName", pathUtf8(utf8Path(path).stem()))},
                     {"type", typeName},
                     {"folder", false},
                     {"parent", parent},
                     {"items", Json::array()},
                     {"dependencies", Json::array()},
                     {"configurations", Json::object()}});
        const auto projectRules = rules(element);
        std::optional<size_t> selectedType;
        for (size_t i = 0; i < types.size(); ++i)
            if ((!types[i].extension.empty() && folded(types[i].extension) == extension &&
                 (element.Attribute("Type") == nullptr || folded(types[i].name) == folded(typeName) ||
                  types[i].id == typeKey(typeName))) ||
                (!types[i].name.empty() && folded(types[i].name) == folded(typeName)) ||
                (!types[i].id.empty() && types[i].id == typeKey(typeName))) {
                selectedType = i;
                break;
            }
        for (const auto &configuration : buildTypes)
            for (const auto &platform : platforms) {
                if (++mappings > 200000)
                    throw std::runtime_error(".slnx exceeds 200,000 project configuration mappings");
                Json mapping = {{"configuration", configuration},
                                {"platform", platform},
                                {"build", true},
                                {"deploy", false}};
                if (!selectedType)
                    applyBuiltin(mapping, builtin(typeName), platform);
                std::set<size_t> chain;
                std::function<void(size_t)> custom = [&](size_t index) {
                    if (!chain.insert(index).second)
                        throw std::runtime_error("Cycle in .slnx project types");
                    const auto &type = types[index];
                    if (!type.base.empty()) {
                        bool found = false;
                        for (size_t i = 0; i < types.size(); ++i)
                            if ((!types[i].name.empty() && folded(type.base) == folded(types[i].name)) ||
                                (!types[i].id.empty() && typeKey(type.base) == types[i].id)) {
                                custom(i);
                                found = true;
                                break;
                            }
                        if (!found) {
                            const auto kind = builtin(type.base);
                            if (kind == Kind::identity)
                                throw std::runtime_error("Unknown .slnx BasedOn project type: " + type.base);
                            applyBuiltin(mapping, kind, platform);
                        }
                    }
                    if (type.buildable)
                        mapping["build"] = *type.buildable;
                    if (type.platforms && !*type.platforms)
                        mapping["platform"] = "";
                    applyRules(mapping, type.rules, configuration, platform);
                    chain.erase(index);
                };
                if (selectedType)
                    custom(*selectedType);
                applyRules(mapping, globalRules, configuration, platform);
                applyRules(mapping, projectRules, configuration, platform);
                item["configurations"][configuration + "|" + platform] = std::move(mapping);
            }
        for (auto *child = element.FirstChildElement(); child; child = child->NextSiblingElement()) {
            const std::string kind = child->Name();
            if (kind == "BuildDependency")
                item["dependencies"].push_back(
                    solutionReference(file.parent_path(), required(*child, "Project")));
            else if (kind != "Properties" && kind != "BuildType" && kind != "Platform" && kind != "Build" &&
                     kind != "Deploy")
                throw std::runtime_error("Unknown .slnx project element: " + kind);
        }
        missing += !item.at("exists").get<bool>();
        external += item.at("external").get<bool>();
        append(std::move(item));
    };
    for (auto *element = root->FirstChildElement(); element; element = element->NextSiblingElement()) {
        const std::string kind = element->Name();
        if (kind == "Project")
            addProject(*element, "");
        else if (kind == "Folder") {
            auto &item = projects[folders.at(folded(slashes(required(*element, "Name"))))];
            const auto parent = item.at("id").get<std::string>();
            for (auto *child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
                const std::string childKind = child->Name();
                if (childKind == "Project")
                    addProject(*child, parent);
                else if (childKind == "File")
                    projects[folders.at(folded(slashes(required(*element, "Name"))))]["items"].push_back(
                        solutionReference(file.parent_path(), required(*child, "Path")));
                else if (childKind != "Properties")
                    throw std::runtime_error("Unknown .slnx folder element: " + childKind);
            }
        } else if (kind != "Configurations" && kind != "Properties")
            throw std::runtime_error("Unknown .slnx solution element: " + kind);
    }
    return {{"path", pathUtf8(file)},
            {"name", pathUtf8(file.stem())},
            {"description", attribute(*root, "Description")},
            {"format", "slnx"},
            {"projects", projects},
            {"configurations", configurations},
            {"missingProjects", missing},
            {"externalProjects", external}};
}
} // namespace cycode

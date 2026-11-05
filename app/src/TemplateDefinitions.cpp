#include "cycode/TemplateDefinitions.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
std::string lower(std::string value) {
    for (auto &c : value)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return value;
}
bool letter(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool word(unsigned char c) {
    return letter(c) || (c >= '0' && c <= '9') || c == '_';
}
bool identifier(const std::string &text) {
    return !text.empty() && text.size() <= 64 && letter(text[0]) &&
           std::all_of(text.begin(), text.end(), word);
}
bool reserved(std::string value) {
    value = lower(value.substr(0, value.find('.')));
    return value == "con" || value == "prn" || value == "aux" || value == "nul" ||
           (value.size() == 4 && (value.starts_with("com") || value.starts_with("lpt")) && value[3] >= '1' &&
            value[3] <= '9');
}
void text(const Json &value, size_t maximum, bool flat = false) {
    if (!value.is_string() || value.get_ref<const std::string &>().size() > maximum)
        throw std::runtime_error("Invalid project template text");
    const auto &bytes = value.get_ref<const std::string &>();
    validateEditorText(bytes);
    if (flat && bytes.find_first_of("\r\n\t") != std::string::npos)
        throw std::runtime_error("Project template label must be a single line");
}
void value(const Json &input) {
    text(input, 128, true);
    const auto &bytes = input.get_ref<const std::string &>();
    if (bytes.empty() || !std::all_of(bytes.begin(), bytes.end(), [](unsigned char c) {
            return word(c) || c == '.' || c == '-' || c == '{' || c == '}';
        }))
        throw std::runtime_error(
            "Template value must use ASCII letters, digits, underscores, dots, hyphens or braces");
}
std::string relative(const std::string &path, bool root = false) {
    text(path, 1024);
    if (root && path == ".")
        return path;
    const auto parsed = utf8Path(path);
    if (path.empty() || parsed.has_root_path() || path.find_first_of("<>:\\\"|?*") != std::string::npos ||
        std::any_of(path.begin(), path.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error("Template file paths must be portable relative paths");
    size_t depth = 0;
    for (const auto &part : parsed) {
        const auto key = pathUtf8(part);
        if (++depth > 16 || key.empty() || key == "." || key == ".." || key.size() > 255 ||
            key.back() == '.' || key.back() == ' ' || reserved(key) || lower(key) == ".git" ||
            lower(key) == ".cycode")
            throw std::runtime_error("Invalid or protected template file path");
    }
    if (pathUtf8(parsed) != path || path.back() == '/' || path.find("//") != std::string::npos)
        throw std::runtime_error("Template file paths must be normalized");
    return path;
}
Json optionsFor(const Json &definition, const Json &provided) {
    if (!provided.is_object() || provided.size() > 16)
        throw std::runtime_error("Invalid template options");
    const auto fields = definition.value("fields", Json::array());
    if (!fields.is_array() || fields.size() > 16)
        throw std::runtime_error("A template supports at most 16 options");
    Json options = Json::object();
    std::set<std::string> ids;
    for (const auto &field : fields) {
        if (!field.is_object())
            throw std::runtime_error("Invalid template field");
        for (const auto &[key, v] : field.items())
            if (key != "id" && key != "title" && key != "type" && key != "default" && key != "choices" &&
                key != "help")
                throw std::runtime_error("Unknown template field property");
        const std::string id = field.at("id"), kind = field.at("type");
        if (!identifier(id) || !ids.insert(id).second)
            throw std::runtime_error("Invalid or duplicate template field ID");
        text(field.at("title"), 256, true);
        if (field.contains("help"))
            text(field.at("help"), 512, true);
        auto check = [&](const Json &v) {
            if (kind == "boolean") {
                if (!v.is_boolean())
                    throw std::runtime_error("Template option must be boolean");
            } else if (kind == "value")
                value(v);
            else if (kind == "identifier") {
                text(v, 64, true);
                if (!identifier(v.get<std::string>()))
                    throw std::runtime_error("Template option must be an identifier");
            } else if (kind == "choice") {
                value(v);
                const auto &choices = field.at("choices");
                if (!choices.is_array() || choices.empty() || choices.size() > 32)
                    throw std::runtime_error("Invalid template choices");
                std::set<std::string> known;
                for (const auto &choice : choices) {
                    value(choice);
                    if (!known.insert(choice.get<std::string>()).second)
                        throw std::runtime_error("Duplicate template choice");
                }
                if (std::find(choices.begin(), choices.end(), v) == choices.end())
                    throw std::runtime_error("Select an advertised template choice");
            } else
                throw std::runtime_error("Unsupported template field type");
        };
        check(field.at("default"));
        const auto v = provided.contains(id) ? provided.at(id) : field.at("default");
        check(v);
        options[id] = v;
    }
    for (const auto &[id, v] : provided.items())
        if (!ids.contains(id))
            throw std::runtime_error("Unknown template option: " + id);
    return options;
}
std::string render(const std::string &source, const std::string &name, const Json &options) {
    std::string result;
    auto symbol = name;
    std::replace(symbol.begin(), symbol.end(), '-', '_');
    auto upper = symbol;
    for (auto &c : upper)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    size_t at = 0;
    while (at < source.size()) {
        const auto begin = source.find("${", at);
        if (begin == std::string::npos) {
            result.append(source, at);
            break;
        }
        result.append(source, at, begin - at);
        if (source.compare(begin, 10, "${project.") != 0 && source.compare(begin, 9, "${option.") != 0) {
            result += "${";
            at = begin + 2;
            continue;
        }
        const auto end = source.find('}', begin + 2);
        if (end == std::string::npos)
            throw std::runtime_error("Unterminated template placeholder");
        const auto key = source.substr(begin + 2, end - begin - 2);
        std::string replacement;
        if (key == "project.name")
            replacement = name;
        else if (key == "project.identifier")
            replacement = symbol;
        else if (key == "project.slug")
            replacement = lower(name);
        else if (key == "project.upper")
            replacement = upper;
        else if (key.starts_with("option.") && options.contains(key.substr(7))) {
            const auto &v = options.at(key.substr(7));
            replacement = v.is_boolean() ? (v.get<bool>() ? "true" : "false") : v.get<std::string>();
        } else
            throw std::runtime_error("Unknown template placeholder: " + key);
        result += replacement;
        at = end + 1;
        if (result.size() > 256 * 1024)
            throw std::runtime_error("Expanded template text exceeds 256 KiB");
    }
    if (result.size() > 256 * 1024)
        throw std::runtime_error("Expanded template text exceeds 256 KiB");
    return result;
}
} // namespace
Json expandProjectTemplate(const Json &definition, const std::string &name, const Json &provided) {
    const auto options = optionsFor(definition, provided);
    if (definition.value("nameStyle", std::string("portable")) == "identifier" && !identifier(name))
        throw std::runtime_error("This template needs a project name containing only letters, digits and "
                                 "underscores, starting with a letter");
    const auto &source = definition.at("files");
    if (!source.is_array() || source.empty() || source.size() > 64)
        throw std::runtime_error("A template supports 1 to 64 files");
    Json files = Json::array();
    std::set<std::string> paths;
    std::map<std::string, std::string> spelling;
    size_t bytes = 0;
    for (const auto &file : source) {
        if (!file.is_object() || file.size() != 2 || !file.contains("path") || !file.contains("text"))
            throw std::runtime_error("Template files contain only path and text");
        text(file.at("path"), 1024);
        text(file.at("text"), 64 * 1024);
        const auto path = relative(render(file.at("path"), name, options));
        const auto content = render(file.at("text"), name, options);
        validateEditorText(content);
        if (!paths.insert(lower(path)).second)
            throw std::runtime_error("Duplicate template file path");
        for (auto part = utf8Path(path); !part.empty(); part = part.parent_path()) {
            const auto actual = pathUtf8(part), key = lower(actual);
            const auto found = spelling.find(key);
            if (found != spelling.end() && found->second != actual)
                throw std::runtime_error("Template paths differ only by case");
            spelling[key] = actual;
        }
        bytes += content.size();
        if (bytes > 256 * 1024 || spelling.size() > 128)
            throw std::runtime_error("Template exceeds 256 KiB / 128 entries");
        files.push_back({{"path", path}, {"text", content}, {"bytes", content.size()}});
    }
    for (const auto &file : files)
        for (auto part = utf8Path(file.at("path")).parent_path(); !part.empty(); part = part.parent_path())
            if (paths.contains(lower(pathUtf8(part))))
                throw std::runtime_error("Template file conflicts with a directory");
    const auto entry = relative(render(definition.at("entry"), name, options), true),
               openFile = relative(render(definition.at("openFile"), name, options));
    if ((entry != "." && !spelling.contains(lower(entry))) || !paths.contains(lower(openFile)))
        throw std::runtime_error("Template entry/source is not part of its files");
    // Require exact spelling, including on case-sensitive hosts.
    if ((entry != "." && spelling.at(lower(entry)) != entry) || spelling.at(lower(openFile)) != openFile)
        throw std::runtime_error("Template entry/source casing differs from its files");
    return {{"files", files},
            {"entry", entry},
            {"openFile", openFile},
            {"options", options},
            {"instructions", render(definition.value("instructions", std::string{}), name, options)}};
}
void validateProjectTemplate(const Json &definition) {
    if (!definition.is_object() || definition.dump().size() > 256 * 1024)
        throw std::runtime_error("Template descriptor exceeds 256 KiB");
    for (const auto &[key, v] : definition.items())
        if (key != "id" && key != "title" && key != "language" && key != "buildSystem" &&
            key != "requirements" && key != "instructions" && key != "entry" && key != "openFile" &&
            key != "files" && key != "fields" && key != "nameStyle" && key != "order")
            throw std::runtime_error("Unknown template descriptor property: " + key);
    for (const auto *key : {"id", "title", "language", "buildSystem"}) {
        text(definition.at(key), 256, true);
        if (definition.at(key).get_ref<const std::string &>().empty())
            throw std::runtime_error("Template identity/title/language/build system cannot be empty");
    }
    const std::set<std::string> builtins = {"cpp.console",    "c.console",    "rust.console",
                                            "csharp.console", "java.console", "web.javascript"};
    if (builtins.contains(definition.at("id")))
        throw std::runtime_error("Cannot replace a built-in project template");
    const auto nameStyle = definition.value("nameStyle", std::string("portable"));
    if (nameStyle != "portable" && nameStyle != "identifier")
        throw std::runtime_error("Invalid template name style");
    const auto &requirements = definition.at("requirements");
    if (!requirements.is_array() || requirements.size() > 32)
        throw std::runtime_error("Invalid template requirements");
    for (const auto &requirement : requirements)
        text(requirement, 256, true);
    if (definition.contains("instructions"))
        text(definition.at("instructions"), 8192);
    (void)expandProjectTemplate(definition, "SampleProject", Json::object());
}
} // namespace cycode

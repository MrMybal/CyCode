#include "cycode/CMakePresets.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
using Json = nlohmann::json;
std::string readPreset(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("Cannot read CMake presets: " + pathUtf8(path));
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 0 || size > 2 * 1024 * 1024)
        throw std::runtime_error("CMake preset file exceeds 2 MiB");
    stream.seekg(0);
    std::string text(size_t(size), '\0');
    if (!stream.read(text.data(), size))
        throw std::runtime_error("Cannot read CMake preset file");
    return text;
}
std::string environment(const std::string &key) {
#ifdef _WIN32
    const auto wide = utf8Path(key).wstring();
    const auto count = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (!count)
        return "";
    std::wstring text(count, L'\0');
    const auto length = GetEnvironmentVariableW(wide.c_str(), text.data(), count);
    if (length >= count)
        throw std::runtime_error("Environment changed during preset expansion");
    text.resize(length);
    return pathUtf8(std::filesystem::path(text));
#else
    const auto *value = std::getenv(key.c_str());
    return value ? value : "";
#endif
}
std::string hostName() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "Darwin";
#else
    return "Linux";
#endif
}
void name(const std::string &value) {
    if (value.empty() || value.size() > 160 || value.find_first_of("\r\n") != std::string::npos ||
        value.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid CMake preset name");
}
} // namespace
CMakePresets::CMakePresets(std::filesystem::path source, Resolver resolver)
    : source_(std::move(source)), resolver_(std::move(resolver)) {
    const auto project = source_ / "CMakePresets.json", user = source_ / "CMakeUserPresets.json";
    rootPresence_[pathUtf8(project)] = std::filesystem::exists(project);
    rootPresence_[pathUtf8(user)] = std::filesystem::exists(user);
    if (std::filesystem::exists(project))
        load(project);
    if (std::filesystem::exists(user)) {
        load(user);
        if (std::filesystem::exists(project)) {
            auto &includes = reachable_[pathUtf8(resolver_(pathUtf8(user)))];
            const auto projectKey = pathUtf8(resolver_(pathUtf8(project)));
            includes.insert(projectKey);
            includes.insert(reachable_[projectKey].begin(), reachable_[projectKey].end());
        }
    }
}
void CMakePresets::load(const std::filesystem::path &input) {
    const auto file = resolver_(pathUtf8(input));
    const auto key = pathUtf8(file);
    if (loading_.contains(key))
        throw std::runtime_error("Cycle in CMake preset includes");
    if (files_.contains(key))
        return;
    if (files_.size() >= 64)
        throw std::runtime_error("CMake presets exceed 64 files");
    auto text = readPreset(file);
    bytes_ += text.size();
    if (bytes_ > 8 * 1024 * 1024)
        throw std::runtime_error("CMake preset catalogue exceeds 8 MiB");
    auto document = Json::parse(text, [](int depth, auto, auto &) {
        if (depth > 64)
            throw std::runtime_error("CMake preset JSON exceeds depth limit");
        return true;
    });
    if (!document.is_object() || !document.contains("version") ||
        !document.at("version").is_number_integer() || document.at("version") < 1 ||
        document.at("version") > 10)
        throw std::runtime_error("CMake preset schema versions 1 through 10 are supported");
    loading_.insert(key);
    files_[key] = std::move(text);
    reachable_[key].insert(key);
    if (document.contains("include")) {
        if (!document.at("include").is_array())
            throw std::runtime_error("CMake preset include must be an array");
        Resolved context{{{"name", ""}, {"environment", Json::object()}}, {}, file};
        for (const auto &entry : document.at("include")) {
            if (!entry.is_string())
                throw std::runtime_error("Invalid CMake preset include");
            const auto raw = entry.get<std::string>();
            if (raw.find("$env{") != std::string::npos || raw.find("${presetName}") != std::string::npos ||
                raw.find("${generator}") != std::string::npos)
                throw std::runtime_error("Preset-specific macros are invalid in include paths");
            std::set<std::string> expansion;
            const auto included =
                resolver_(pathUtf8(file.parent_path() / utf8Path(expand(raw, context, file, expansion))));
            load(included);
            const auto includedKey = pathUtf8(included);
            reachable_[key].insert(reachable_[includedKey].begin(), reachable_[includedKey].end());
        }
    }
    for (const auto *kind :
         {"configurePresets", "buildPresets", "testPresets", "packagePresets", "workflowPresets"})
        if (document.contains(kind)) {
            if ((std::string(kind) == "packagePresets" || std::string(kind) == "workflowPresets") &&
                document.at("version") < 6)
                throw std::runtime_error("Package/workflow presets require schema version 6 or later");
            if (!document.at(kind).is_array() || document.at(kind).size() > 512)
                throw std::runtime_error("Invalid CMake preset array");
            for (const auto &entry : document.at(kind)) {
                if (!entry.is_object())
                    throw std::runtime_error("Invalid CMake preset");
                const auto id = entry.at("name").get<std::string>();
                name(id);
                if (!presets_[kind].emplace(id, Entry{entry, file}).second)
                    throw std::runtime_error("Duplicate CMake preset: " + id);
                if (presets_[kind].size() > 512)
                    throw std::runtime_error("Too many CMake presets");
            }
        }
    loading_.erase(key);
}
CMakePresets::Resolved CMakePresets::resolve(const std::string &kind, const std::string &id,
                                             std::set<std::string> &chain) {
    const auto cacheKey = kind + ":" + id;
    if (resolved_.contains(cacheKey))
        return resolved_.at(cacheKey);
    if (!presets_[kind].contains(id))
        throw std::runtime_error("Unknown CMake preset: " + id);
    if (!chain.insert(id).second || chain.size() > 64)
        throw std::runtime_error("Cycle/depth limit in CMake preset inheritance");
    const auto &entry = presets_[kind].at(id);
    Resolved result{Json::object(), {}, entry.file};
    auto inherit = entry.value.value("inherits", Json::array());
    if (inherit.is_string())
        inherit = Json::array({inherit});
    if (!inherit.is_array())
        throw std::runtime_error("Invalid CMake preset inheritance");
    auto merge = [&](const Resolved &parent, bool replace) {
        for (const auto &[field, value] : parent.value.items()) {
            if (field == "name" || field == "hidden" || field == "inherits" || field == "displayName" ||
                field == "description")
                continue;
            if ((field == "environment" || field == "cacheVariables" || field == "variables" ||
                 field == "output") &&
                value.is_object()) {
                if (!result.value.contains(field))
                    result.value[field] = Json::object();
                if (!result.value[field].is_object())
                    throw std::runtime_error("Invalid CMake preset map");
                for (const auto &[key, item] : value.items())
                    if (replace || !result.value[field].contains(key)) {
                        result.value[field][key] = item;
                        const auto originKey = field + ":" + key;
                        result.origins[originKey] =
                            parent.origins.contains(originKey) ? parent.origins.at(originKey) : parent.file;
                    }
            } else if (replace || !result.value.contains(field)) {
                result.value[field] = value;
                result.origins[field] =
                    parent.origins.contains(field) ? parent.origins.at(field) : parent.file;
            }
        }
    };
    for (const auto &parentName : inherit) {
        const auto parent = parentName.get<std::string>();
        name(parent);
        if (!presets_[kind].contains(parent) ||
            !reachable_.at(pathUtf8(entry.file)).contains(pathUtf8(presets_[kind].at(parent).file)))
            throw std::runtime_error("CMake preset inherits from a file it does not include");
        merge(resolve(kind, parent, chain), false);
    }
    merge(Resolved{entry.value, {}, entry.file}, true);
    for (const auto *field : {"name", "hidden", "displayName", "description"})
        if (entry.value.contains(field))
            result.value[field] = entry.value.at(field);
    const auto size = result.value.dump().size();
    resolvedBytes_ += size;
    if (size > 256 * 1024 || resolvedBytes_ > 16 * 1024 * 1024)
        throw std::runtime_error("Resolved CMake presets exceed metadata limits");
    chain.erase(id);
    resolved_[cacheKey] = result;
    return result;
}
std::string CMakePresets::expand(const std::string &input, const Resolved &preset,
                                 const std::filesystem::path &origin, std::set<std::string> &chain) {
    if (chain.size() > 64 || input.size() > 32768 || input.find('\0') != std::string::npos)
        throw std::runtime_error("CMake macro expansion limit exceeded");
    std::string result;
    for (size_t i = 0; i < input.size();) {
        if (input[i] != '$') {
            result += input[i++];
            continue;
        }
        size_t start = 0;
        std::string family;
        if (input.compare(i, 2, "${") == 0) {
            family = "macro";
            start = i + 2;
        } else if (input.compare(i, 5, "$env{") == 0) {
            family = "env";
            start = i + 5;
        } else if (input.compare(i, 6, "$penv{") == 0) {
            family = "penv";
            start = i + 6;
        } else if (input.compare(i, 8, "$vendor{") == 0)
            throw std::runtime_error("Vendor macros cannot determine CyCode build paths");
        else {
            result += input[i++];
            continue;
        }
        const auto end = input.find('}', start);
        if (end == std::string::npos)
            throw std::runtime_error("Unclosed CMake preset macro");
        const auto key = input.substr(start, end - start);
        std::string value;
        if (family == "macro") {
            if (key == "sourceDir")
                value = pathUtf8(source_);
            else if (key == "sourceParentDir")
                value = pathUtf8(source_.parent_path());
            else if (key == "sourceDirName")
                value = pathUtf8(source_.filename());
            else if (key == "presetName")
                value = preset.value.at("name");
            else if (key == "hostSystemName")
                value = hostName();
            else if (key == "fileDir")
                value = pathUtf8(origin.parent_path());
            else if (key == "dollar")
                value = "$";
            else if (key == "pathListSep") {
#ifdef _WIN32
                value = ";";
#else
                value = ":";
#endif
            } else if (key == "generator") {
                if (!chain.insert("generator").second)
                    throw std::runtime_error("Cyclic CMake generator macro");
                value = expand(preset.value.value("generator", ""), preset,
                               preset.origins.contains("generator") ? preset.origins.at("generator")
                                                                    : preset.file,
                               chain);
                chain.erase("generator");
            } else
                throw std::runtime_error("Unknown CMake preset path macro: " + key);
        } else {
            const auto variables = preset.value.value("environment", Json::object());
            if (!variables.is_object())
                throw std::runtime_error("Invalid CMake preset environment");
            if (family == "env" && variables.contains(key)) {
                if (!chain.insert("env:" + key).second)
                    throw std::runtime_error("Cycle in CMake preset environment");
                if (!variables.at(key).is_null())
                    value = expand(variables.at(key).get<std::string>(), preset,
                                   preset.origins.contains("environment:" + key)
                                       ? preset.origins.at("environment:" + key)
                                       : preset.file,
                                   chain);
                chain.erase("env:" + key);
            } else
                value = environment(key);
        }
        result += value;
        if (result.size() > 32768 || result.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid expanded CMake path");
        i = end + 1;
    }
    return result;
}
CMakePresets::Json CMakePresets::selection(std::string configure, const std::string &build) {
    std::set<std::string> chain;
    Resolved buildPreset;
    if (!build.empty()) {
        name(build);
        buildPreset = resolve("buildPresets", build, chain);
        if (buildPreset.value.value("hidden", false))
            throw std::runtime_error("Hidden build preset cannot be selected");
        const auto linked = buildPreset.value.at("configurePreset").get<std::string>();
        if (!configure.empty() && configure != linked)
            throw std::runtime_error("Build preset refers to another configure preset");
        configure = linked;
    }
    name(configure);
    auto preset = resolve("configurePresets", configure, chain);
    if (preset.value.value("hidden", false))
        throw std::runtime_error("Hidden configure preset cannot be selected");
    if (buildPreset.value.is_object() && buildPreset.value.value("inheritConfigureEnvironment", true)) {
        auto env = preset.value.value("environment", Json::object());
        const auto own = buildPreset.value.value("environment", Json::object());
        for (const auto &[key, value] : env.items())
            if (!own.contains(key))
                buildPreset.origins["environment:" + key] = preset.origins.contains("environment:" + key)
                                                                ? preset.origins.at("environment:" + key)
                                                                : preset.file;
        env.update(buildPreset.value.value("environment", Json::object()));
        buildPreset.value["environment"] = std::move(env);
    }
    const auto binary =
        expand(preset.value.value("binaryDir", ""), preset,
               preset.origins.contains("binaryDir") ? preset.origins.at("binaryDir") : preset.file, chain);
    if (binary.empty())
        throw std::runtime_error("Selected CMake configure preset needs binaryDir");
    const auto directory = resolver_(pathUtf8(source_ / utf8Path(binary)));
    std::string configuration;
    if (!build.empty() && buildPreset.value.contains("configuration"))
        configuration =
            expand(buildPreset.value.at("configuration"), buildPreset,
                   buildPreset.origins.contains("configuration") ? buildPreset.origins.at("configuration")
                                                                 : buildPreset.file,
                   chain);
    else if (preset.value.value("cacheVariables", Json::object()).contains("CMAKE_BUILD_TYPE")) {
        auto value = preset.value.at("cacheVariables").at("CMAKE_BUILD_TYPE");
        if (value.is_object())
            value = value.at("value");
        if (!value.is_null())
            configuration = expand(value.get<std::string>(), preset,
                                   preset.origins.contains("cacheVariables:CMAKE_BUILD_TYPE")
                                       ? preset.origins.at("cacheVariables:CMAKE_BUILD_TYPE")
                                       : preset.file,
                                   chain);
    }
    return {{"configurePreset", configure},
            {"buildPreset", build},
            {"buildDirectory", pathUtf8(directory)},
            {"generator", preset.value.value("generator", "")},
            {"configuration", configuration}};
}
CMakePresets::Json CMakePresets::catalogue() {
    size_t metadataBytes = 0;
    Json result = {{"configurePresets", Json::array()},
                   {"buildPresets", Json::array()},
                   {"testPresets", Json::array()},
                   {"packagePresets", Json::array()},
                   {"workflowPresets", Json::array()},
                   {"error", ""},
                   {"files", Json::array()}};
    for (const auto &[file, text] : files_)
        result["files"].push_back(file);
    for (const auto *kind :
         {"configurePresets", "buildPresets", "testPresets", "packagePresets", "workflowPresets"})
        for (const auto &[id, entry] : presets_[kind]) {
            if (entry.value.value("hidden", false))
                continue;
            Json item = {{"name", id},
                         {"displayName", entry.value.value("displayName", id)},
                         {"description", entry.value.value("description", "")},
                         {"file", pathUtf8(entry.file)},
                         {"error", ""}};
            try {
                const std::string family = kind;
                auto selected = family == "configurePresets" || family == "buildPresets"
                                    ? selection(family == "configurePresets" ? id : "",
                                                family == "buildPresets" ? id : "")
                                    : action(family == "testPresets"      ? "test"
                                             : family == "packagePresets" ? "package"
                                                                          : "workflow",
                                             id);
                item.update(selected);
            } catch (const std::exception &e) {
                item["error"] = e.what();
            }
            metadataBytes += item.dump().size();
            if (metadataBytes > 16 * 1024 * 1024)
                throw std::runtime_error("CMake preset catalogue exceeds 16 MiB of public metadata");
            result[kind].push_back(std::move(item));
        }
    result["revision"] = revision();
    return result;
}
void CMakePresets::checkUnchanged() const {
    for (const auto &[file, present] : rootPresence_)
        if (std::filesystem::exists(resolver_(file)) != present)
            throw std::runtime_error("CMake root preset files changed; refresh and retry");
    for (const auto &[file, text] : files_)
        if (resolver_(file) != utf8Path(file) || readPreset(resolver_(file)) != text)
            throw std::runtime_error("CMake presets changed during operation; refresh and retry");
}
} // namespace cycode

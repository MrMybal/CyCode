#include "cycode/PluginManifest.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace cycode {
using Json = nlohmann::json;
bool pluginIdentifier(const std::string &id) {
    return !id.empty() && id.size() <= 160 && id != "." && id != ".." &&
           std::all_of(id.begin(), id.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_' || c == '-'; });
}
void validatePluginManifest(const Json &info) {
    if (!info.is_object() || !pluginIdentifier(info.at("id").get<std::string>()) ||
        !pluginIdentifier(info.at("library").get<std::string>()))
        throw std::runtime_error("Invalid plugin ID or library basename");
    if (!info.at("abi").is_number_unsigned() && !info.at("abi").is_number_integer())
        throw std::runtime_error("Invalid plugin ABI");
    if (info.at("abi") != 1 && info.at("abi") != 2)
        throw std::runtime_error("Unsupported plugin ABI");
    pluginVersion(info.value("version", std::string("0.0.0")));
    for (const auto *key : {"name", "description", "homepage", "license"})
        if (info.contains(key) &&
            (!info.at(key).is_string() || info.at(key).get_ref<const std::string &>().size() > 8192 ||
             info.at(key).get_ref<const std::string &>().find('\0') != std::string::npos))
            throw std::runtime_error("Invalid plugin metadata string");
    const auto permissions = info.value("permissions", Json::array()),
               dependencies = info.value("dependencies", Json::array());
    if (!permissions.is_array() || permissions.size() > 64 || !dependencies.is_array() ||
        dependencies.size() > 64)
        throw std::runtime_error("Invalid plugin permissions/dependencies");
    std::set<std::string> seen;
    for (const auto &permission : permissions)
        if (!permission.is_string() || permission.get_ref<const std::string &>().empty() ||
            permission.get_ref<const std::string &>().size() > 128 ||
            permission.get_ref<const std::string &>().find('\0') != std::string::npos ||
            !seen.insert(permission.get<std::string>()).second)
            throw std::runtime_error("Invalid or duplicate plugin permission");
    seen.clear();
    for (const auto &d : dependencies) {
        if (!d.is_object() || !pluginIdentifier(d.at("id").get<std::string>()) ||
            d.at("id") == info.at("id") || !seen.insert(d.at("id").get<std::string>()).second)
            throw std::runtime_error("Invalid or duplicate dependency");
        if (d.contains("minimum"))
            pluginVersion(d.at("minimum"));
        if (d.contains("maximumExclusive"))
            pluginVersion(d.at("maximumExclusive"));
        if (d.contains("minimum") && d.contains("maximumExclusive") &&
            pluginVersion(d.at("minimum")) >= pluginVersion(d.at("maximumExclusive")))
            throw std::runtime_error("Dependency version range is empty");
        if (d.contains("optional") && !d.at("optional").is_boolean())
            throw std::runtime_error("Invalid optional dependency flag");
    }
    (void)pluginCompatibility(info);
}

std::array<unsigned, 3> pluginVersion(const std::string &text) {
    std::array<unsigned, 3> value{};
    size_t part = 0;
    bool digit = false;
    if (text.empty() || text.size() > 24)
        throw std::runtime_error("Expected numeric major.minor.patch version");
    for (const unsigned char c : text) {
        if (c == '.' && part < 2 && digit) {
            ++part;
            digit = false;
        } else {
            if (c < '0' || c > '9' || value[part] > (999999u - unsigned(c - '0')) / 10)
                throw std::runtime_error("Expected numeric major.minor.patch version");
            value[part] = value[part] * 10 + c - '0';
            digit = true;
        }
    }
    if (part != 2 || !digit)
        throw std::runtime_error("Expected numeric major.minor.patch version");
    return value;
}

Json pluginHostInfo() {
#if defined(_WIN32)
    const std::string os = "windows";
#elif defined(__APPLE__)
    const std::string os = "macos";
#elif defined(__linux__)
    const std::string os = "linux";
#else
    const std::string os = "unknown";
#endif
#if defined(_M_ARM64) || defined(__aarch64__)
    const std::string architecture = "arm64";
#elif defined(_M_X64) || defined(__x86_64__)
    const std::string architecture = "x64";
#elif defined(_M_IX86) || defined(__i386__)
    const std::string architecture = "x86";
#else
    const std::string architecture = "unknown";
#endif
    return {
        {"name", "CyCode"},
        {"version", CYCODE_VERSION},
        {"os", os},
        {"architecture", architecture},
        {"platform", os + "-" + architecture},
        {"features", {"activationNotifications", "workspaceEvents", "eventSubscriptions", "debugAdapters"}}};
}

Json pluginCompatibility(const Json &manifest) {
    if (!manifest.is_object())
        throw std::runtime_error("Plugin manifest must be an object");
    const auto host = pluginHostInfo();
    Json reasons = Json::array();
    if (manifest.contains("platforms")) {
        const auto &platforms = manifest.at("platforms");
        const std::set<std::string> known = {"windows-x64", "windows-arm64", "windows-x86", "linux-x64",
                                             "linux-arm64", "linux-x86",     "macos-x64",   "macos-arm64"};
        if (!platforms.is_array() || platforms.empty() || platforms.size() > known.size())
            throw std::runtime_error("Plugin platforms must be a nonempty list of supported platform IDs");
        std::set<std::string> unique;
        for (const auto &platform : platforms)
            if (!platform.is_string() || !known.contains(platform.get<std::string>()) ||
                !unique.insert(platform.get<std::string>()).second)
                throw std::runtime_error("Invalid or duplicate plugin platform");
        if (!unique.contains(host.at("platform").get<std::string>()))
            reasons.push_back("Plugin does not support " + host.at("platform").get<std::string>());
    }
    if (manifest.contains("hostVersion")) {
        const auto &range = manifest.at("hostVersion");
        if (!range.is_object() || range.empty())
            throw std::runtime_error("Plugin hostVersion requires minimum and/or maximumExclusive");
        for (auto it = range.begin(); it != range.end(); ++it) {
            if ((it.key() != "minimum" && it.key() != "maximumExclusive") || !it.value().is_string())
                throw std::runtime_error("Invalid plugin hostVersion constraint");
            pluginVersion(it.value());
        }
        if (range.contains("minimum") && range.contains("maximumExclusive") &&
            pluginVersion(range.at("minimum")) >= pluginVersion(range.at("maximumExclusive")))
            throw std::runtime_error("Plugin hostVersion range is empty");
        const auto current = pluginVersion(host.at("version"));
        if (range.contains("minimum") && current < pluginVersion(range.at("minimum")))
            reasons.push_back("Requires CyCode >= " + range.at("minimum").get<std::string>());
        if (range.contains("maximumExclusive") && current >= pluginVersion(range.at("maximumExclusive")))
            reasons.push_back("Requires CyCode < " + range.at("maximumExclusive").get<std::string>());
    }
    std::string reason;
    for (const auto &message : reasons) {
        if (!reason.empty())
            reason += "; ";
        reason += message.get<std::string>();
    }
    return {{"compatible", reasons.empty()}, {"reason", reason}, {"reasons", reasons}};
}
} // namespace cycode

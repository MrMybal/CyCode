#include "cycode/update/detail/UpdateManifest.h"
#include "cycode/update/detail/UpdateFiles.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
namespace cycode::updates::detail {
using Json = nlohmann::json;
static std::string folded(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return name;
}
std::set<std::string> managedUnion(const ManagedFiles &before, const ManagedFiles &after) {
    std::set<std::string> result{"manifest.json"}, unique;
    for (const auto &[name, file] : before)
        result.insert(name);
    for (const auto &[name, file] : after)
        result.insert(name);
    for (const auto &name : result)
        if (!unique.insert(folded(name)).second)
            throw std::runtime_error("Conflicting managed path case");
    for (const auto &key : unique)
        for (auto slash = key.find('/'); slash != key.npos; slash = key.find('/', slash + 1))
            if (unique.contains(key.substr(0, slash)))
                throw std::runtime_error("Conflicting managed file/directory paths");
    return result;
}
ManagedFiles managedManifest(const Json &json) {
    (void)version(json.at("version").get<std::string>());
    if (!json.contains("updateFormat") || !json.at("updateFormat").is_number_integer() ||
        json.at("updateFormat") != 1 || json.at("platform") != platform() || !json.at("files").is_array() ||
        json.at("files").empty() || json.at("files").size() > 4095)
        throw std::runtime_error("Invalid update manifest");
    ManagedFiles result;
    std::set<std::string> unique;
    uint64_t total = 0;
    for (const auto &file : json.at("files")) {
        const auto name = file.at("path").get<std::string>(), hash = file.at("sha256").get<std::string>();
        const auto &size = file.at("size");
        if (name == "manifest.json" || !packagePath(name) || !unique.insert(folded(name)).second ||
            !detail::digest(hash) || !size.is_number_integer() || size < 0 || size > 64 * 1024 * 1024)
            throw std::runtime_error("Invalid managed update file");
        total += size.get<uint64_t>();
        if (total > maximumPackage)
            throw std::runtime_error("Update manifest exceeds size limit");
        result.emplace(name, ManagedFile{hash, size.get<uint64_t>()});
    }
    for (const auto *required : {"CyCode.exe", "CyCodeUpdater.exe", "cycode_cli.exe", "app/CyTool.json",
                                 "app/locales/ui.json", "LICENSE", "COPYRIGHT"})
        if (!result.contains(required))
            throw std::runtime_error("Incomplete update package");
    return result;
}
} // namespace cycode::updates::detail

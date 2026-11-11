#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/Updates.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>

namespace cycode::updates {
std::array<uint32_t, 3> version(std::string_view text) {
    if (text.starts_with('v'))
        text.remove_prefix(1);
    if (text.empty() || text.size() > 32)
        throw std::runtime_error("Invalid release version");
    std::array<uint32_t, 3> result{};
    for (size_t i = 0; i < 3; ++i) {
        const auto at = text.find('.');
        const auto part = text.substr(0, at);
        if (part.empty() || (part.size() > 1 && part[0] == '0'))
            throw std::runtime_error("Invalid release version");
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), result[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || result[i] > 65535 ||
            (i < 2) == (at == std::string_view::npos))
            throw std::runtime_error("Invalid release version");
        if (i < 2)
            text.remove_prefix(at + 1);
    }
    return result;
}
bool newer(std::string_view candidate, std::string_view current) {
    return version(candidate) > version(current);
}
std::string platform() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    return "windows-x64";
#else
    return "unsupported";
#endif
}
std::optional<Release> parseRelease(const nlohmann::json &json, std::string_view current,
                                    std::string_view target, bool installed) {
    if (json.at("draft").get<bool>() || json.at("prerelease").get<bool>())
        throw std::runtime_error("Only stable published releases are accepted");
    const auto tag = json.at("tag_name").get<std::string>();
    if (!newer(tag, current))
        return {};
    Release result;
    result.version = tag.starts_with('v') ? tag.substr(1) : tag;
    result.page = "https://github.com/" + std::string(repository) + "/releases/tag/" + tag;
    if (json.contains("body") && json.at("body").is_string())
        result.notes = json.at("body").get<std::string>();
    if (result.notes.size() > 32768) {
        size_t cut = 32768;
        while (cut && (static_cast<unsigned char>(result.notes[cut]) & 0xc0) == 0x80)
            --cut;
        result.notes.resize(cut);
    }
    if (!json.at("assets").is_array() || json.at("assets").size() > 1024)
        throw std::runtime_error("Invalid release asset list");
    const auto portable = std::string(productIdentity().product) + "-" + result.version + "-" + std::string(target) + ".zip";
    const auto legacy = std::string(productIdentity().product) + "-" + result.version + "-" + std::string(target) + "-update.zip";
    const auto &assets = json.at("assets");
    const auto name = installed ? std::string(productIdentity().product) + "-" + result.version + "-" + std::string(target) + "-setup.exe" : std::any_of(assets.begin(), assets.end(), [&](const auto &asset) {
        return asset.value("name", std::string{}) == portable;
    }) ? portable : legacy;
    unsigned matches = 0;
    for (const auto &asset : json.at("assets"))
        if (asset.value("name", std::string{}) == name) {
            if (++matches > 1)
                throw std::runtime_error("Duplicate update package");
            result.asset.name = name;
            result.asset.url = asset.at("url");
            const auto &size = asset.at("size");
            if (!size.is_number_integer() || size <= 0 || size > maximumPackage)
                throw std::runtime_error("Invalid update size");
            result.asset.size = size.get<uint64_t>();
            const auto digest = asset.value("digest", nlohmann::json());
            if (digest.is_null()) {
                result.asset = {};
                continue;
            } // Old releases remain manually accessible.
            const auto hash = digest.get<std::string>();
            if (!hash.starts_with("sha256:") || hash.size() != 71 ||
                !std::all_of(hash.begin() + 7, hash.end(),
                             [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
                throw std::runtime_error("Invalid GitHub asset digest");
            result.asset.digest = hash.substr(7);
            const auto prefix =
                "https://api.github.com/repos/" + std::string(repository) + "/releases/assets/";
            const auto id = result.asset.url.substr(std::min(prefix.size(), result.asset.url.size()));
            if (!result.asset.url.starts_with(prefix) || id.empty() ||
                !std::all_of(id.begin(), id.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
                asset.value("state", std::string{}) != "uploaded")
                throw std::runtime_error("Untrusted release asset");
        }
    return result;
}
bool packagePath(std::string_view path) {
    if (path.empty() || path.size() > 512 || path.front() == '/' || path.back() == '/' ||
        path.find('\\') != path.npos)
        return false;
    size_t from = 0;
    while (from < path.size()) {
        const auto end = path.find('/', from);
        const auto part = path.substr(from, end == path.npos ? path.size() - from : end - from);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        std::string key(part);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        const auto stem = key.substr(0, key.find('.'));
        if (key == "data" || key == "runtime" || key == ".git" || key == ".cydocbuild" || stem == "con" || stem == "nul" ||
            stem == "prn" || stem == "aux" ||
            (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' &&
             stem[3] <= '9'))
            return false;
        for (unsigned char c : part)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                  c == '_' || c == '.'))
                return false;
        if (end == path.npos)
            break;
        from = end + 1;
    }
    return path == productIdentity().application || path == "cycode_cli.exe" || path == productIdentity().helper ||
           path == "manifest.json" || path == "PACKAGE.md" || path == "README.md" || path == "LISEZ-MOI.md" ||
           path == "website/assets/workspace.png" || path == "website/assets/git-conflicts.png" ||
           path == "CHANGELOG.md" || path == "README.fr.md" || path == "VERSION.md" || path == "VERSION.fr.md" ||
           path == "msvcp140.dll" || path == "msvcp140_1.dll" || path == "msvcp140_2.dll" ||
           path == "msvcp140_atomic_wait.dll" || path == "msvcp140_codecvt_ids.dll" ||
           path == "vcruntime140.dll" || path == "vcruntime140_1.dll" || path == "concrt140.dll" || path == "LICENSE" || path == "COPYRIGHT" || path.starts_with("app/") ||
           path.starts_with("icons/") || path.starts_with("tools/") || path.starts_with("extensions/");
}
bool installedInstallation(const std::filesystem::path &root) {
    size_t visited = 0;
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        if (++visited > 512)
            throw std::runtime_error("Installation identity scan exceeds its bound");
        auto name = pathUtf8(entry.path().filename());
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        std::string ownMarker = productIdentity().installationMarker;
        std::transform(ownMarker.begin(), ownMarker.end(), ownMarker.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if ((name == "installed.flag" || name.ends_with("-installed.flag")) &&
            name != ownMarker)
            throw std::runtime_error("Installation marker belongs to another product");
    }
    const auto flag = root / productIdentity().installationMarker;
    detail::safe(flag);
    if (!std::filesystem::exists(flag)) return false;
    if (detail::read(flag, 128) != productIdentity().installationMarkerText)
        throw std::runtime_error("Invalid installed distribution marker");
    return true;
}
bool portableInstallation(const std::filesystem::path &root) {
    try {
        if (std::filesystem::exists(root / ".git") ||
            std::filesystem::is_symlink(std::filesystem::symlink_status(root)))
            return false;
        if (std::filesystem::file_size(root / "manifest.json") > 2 * 1024 * 1024)
            return false;
        std::ifstream in(root / "manifest.json");
        const auto m = nlohmann::json::parse(in);
        if (!detail::matchesProduct(m)) return false;
        (void)installedInstallation(root);
        (void)version(m.at("version").get<std::string>());
        return detail::matchesProduct(m) && m.contains("updateFormat") && m.at("updateFormat").is_number_integer() &&
               m.at("updateFormat") == 1 && m.value("platform", std::string{}) == platform() &&
               m.at("files").is_array() && std::filesystem::is_regular_file(root / productIdentity().application) &&
               std::filesystem::is_regular_file(root / productIdentity().helper);
    } catch (...) {
        return false;
    }
}
std::unique_ptr<NativeFileLease> installationUse(const std::filesystem::path &root) {
    if (std::filesystem::exists(root / ".git"))
        return {};
    detail::safe(root / "data/updates/pending.json");
    if (std::filesystem::exists(root / "data/updates/pending.json"))
        throw std::runtime_error(
            std::string("An interrupted update needs recovery. Run the staged ") + productIdentity().helper + " in "
            "data/updates/download-... "
            "to review restoration. Keep pending.json, transaction backups and recovery files.");
    if (!portableInstallation(root))
        return {};
    std::filesystem::create_directories(root / "data/updates");
    auto result =
        std::make_unique<NativeFileLease>(root / "data/updates/instances.lock", NativeFileLeaseMode::shared);
    if (!result->acquired())
        throw std::runtime_error("An application update is in progress; retry when it finishes");
    return result;
}
} // namespace cycode::updates

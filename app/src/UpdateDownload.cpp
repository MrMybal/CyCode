#include "UpdateDownload.h"
#include "UpdateBudget.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <stdexcept>

namespace cycode::updates::detail {
namespace {
namespace fs = std::filesystem;
uint64_t ordinary(const fs::path &file, uint64_t limit) {
    safe(file);
    if (!fs::is_regular_file(file) || fs::hard_link_count(file) != 1 || fs::file_size(file) > limit)
        throw std::runtime_error("Unsafe partial update file");
    return fs::file_size(file);
}
} // namespace
void requireDownloadSpace(const fs::path &directory, uint64_t total, uint64_t received, uint64_t helperSize,
                          const std::shared_ptr<DownloadStatus> &status) {
    const auto required = downloadHeadroom(total, received, helperSize);
    if (!required)
        throw DownloadError(DownloadIssue::installation, "Invalid update download space requirement");
    if (status) {
        status->spaceKnown = false;
        status->requiredBytes = *required;
    }
    const auto available = availableSpace(directory);
    if (status) {
        status->availableBytes = available;
        status->spaceKnown = true;
    }
    if (available < *required)
        throw DownloadError(DownloadIssue::space, "Insufficient space for update download and staging");
}
void downloadBinding(const Release &release) {
    (void)version(release.version);
    const auto prefix = "https://api.github.com/repos/" + std::string(repository) + "/releases/assets/";
    const auto &asset = release.asset;
    if (asset.name != "CyCode-" + release.version + "-" + platform() + "-update.zip" ||
        !digest(asset.digest) || !asset.size || asset.size > maximumPackage ||
        !asset.url.starts_with(prefix) || asset.url.size() <= prefix.size() ||
        asset.url.size() > prefix.size() + 20 ||
        !std::all_of(asset.url.begin() + prefix.size(), asset.url.end(),
                     [](char c) { return c >= '0' && c <= '9'; }))
        throw std::runtime_error("Invalid update package binding");
}
bool sameDownload(const PartialDownload &partial, const Release &release) {
    return partial.version == release.version && partial.asset.name == release.asset.name &&
           partial.asset.url == release.asset.url && partial.asset.digest == release.asset.digest &&
           partial.asset.size == release.asset.size;
}
std::optional<PartialDownload> selectPartial(const fs::path &root, const Release &release, DownloadMode mode,
                                             const std::atomic<bool> &cancel) {
    if (cancel)
        throw std::runtime_error("Update cancelled");
    if (mode == DownloadMode::fresh)
        return std::nullopt; // Do not even inspect old checkpoints for an explicit fresh transfer.
    if (mode != DownloadMode::resume)
        throw std::invalid_argument("Invalid update download mode");
    std::optional<PartialDownload> selected;
    safe(root / "data/updates");
    size_t visited = 0;
    for (const auto &entry : fs::directory_iterator(root / "data/updates")) {
        if (cancel)
            throw std::runtime_error("Update cancelled");
        if (++visited > 128)
            break;
        if (!workName(pathUtf8(entry.path().filename()), "download-"))
            continue;
        try {
            auto partial = partialMetadata(root, entry.path());
            if (sameDownload(partial, release) &&
                (!selected || partial.received > selected->received ||
                 (partial.received == selected->received && partial.directory < selected->directory)))
                selected = std::move(partial);
        } catch (...) {
            // Keep foreign, completed, rejected and modified directories untouched.
        }
    }
    return selected;
}
void createPartialMetadata(const fs::path &root, const fs::path &directory, const Release &release) {
    downloadBinding(release);
    const auto file = directory / "resume.json";
    safe(file);
    const auto bytes = nlohmann::json{{"format", 1},
                                      {"installation", pathUtf8(root)},
                                      {"version", release.version},
                                      {"name", release.asset.name},
                                      {"url", release.asset.url},
                                      {"sha256", release.asset.digest},
                                      {"size", release.asset.size}}
                           .dump(2) +
                       "\n";
    if (bytes.size() > 8192)
        throw std::runtime_error("Partial update metadata exceeds its size limit");
    writeTextFileAtomic(file, bytes, std::nullopt);
}
PartialDownload partialMetadata(const fs::path &root, const fs::path &directory) {
    if (!root.is_absolute() || root != root.lexically_normal() ||
        directory.parent_path() != root / "data/updates" ||
        !workName(pathUtf8(directory.filename()), "download-"))
        throw std::runtime_error("Invalid partial update location");
    ordinary(directory / "resume.json", 8192);
    const auto j = nlohmann::json::parse(read(directory / "resume.json", 8192));
    if (!j.at("format").is_number_integer() || j.at("format") != 1 || j.contains("rejected") ||
        j.at("installation") != pathUtf8(root) || !j.at("size").is_number_unsigned())
        throw std::runtime_error("Invalid partial update metadata");
    Release release{j.at("version").get<std::string>(),
                    {},
                    {},
                    {j.at("name").get<std::string>(), j.at("url").get<std::string>(),
                     j.at("sha256").get<std::string>(), j.at("size").get<uint64_t>()}};
    downloadBinding(release);
    const auto archive = directory / release.asset.name;
    const auto partial = directory / "download.partial";
    size_t visited = 0;
    for (const auto &entry : fs::directory_iterator(directory)) {
        if (++visited > 5)
            throw std::runtime_error("Unknown files in partial update directory");
        const auto name = pathUtf8(entry.path().filename());
        if (name != "resume.json" && name != "download.partial" && name != release.asset.name &&
            name != "CyCodeUpdater.exe" && name != "updater-messages.json")
            throw std::runtime_error("Unknown files in partial update directory");
        ordinary(entry.path(), name == "CyCodeUpdater.exe"       ? 64 * 1024 * 1024
                               : name == "updater-messages.json" ? 4 * 1024 * 1024
                                                                 : maximumPackage);
    }
    const bool complete = fs::exists(archive), incomplete = fs::exists(partial);
    if (complete && incomplete)
        throw std::runtime_error("Ambiguous partial update payload");
    const auto received = complete     ? ordinary(archive, release.asset.size)
                          : incomplete ? ordinary(partial, release.asset.size)
                                       : 0;
    if (complete && received != release.asset.size)
        throw std::runtime_error("Incomplete staged update archive");
    return {directory, release.version, release.asset, received};
}
bool downloadResponse(unsigned status, std::string_view range, std::string_view encoding,
                      std::string_view length, uint64_t offset, uint64_t size) {
    const auto result = downloadResponsePolicy(status, range, encoding, length, offset, size);
    if (result == DownloadResponse::reject)
        throw DownloadError(DownloadIssue::response,
                            "Update server returned an invalid encoding, size or byte range");
    return result == DownloadResponse::receive;
}
} // namespace cycode::updates::detail

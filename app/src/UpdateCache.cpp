#include "UpdateDownload.h"
#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/update/detail/UpdateInstall.h"
#include "cycode/update/detail/UpdateManifest.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace cycode::updates {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
void cancelled(const std::atomic<bool> &cancel) {
    if (cancel)
        throw std::runtime_error("Update cancelled");
}
bool directoryName(const std::string &name) {
    return detail::workName(name, "download-");
}
void binding(const Prepared &p) {
    (void)version(p.version);
    if (!p.installation.is_absolute() || p.installation != p.installation.lexically_normal() ||
        !detail::digest(p.digest) || !p.package.is_absolute() || p.package != p.package.lexically_normal() ||
        p.helper != p.package.parent_path() / "CyCodeUpdater.exe" ||
        p.package.parent_path().parent_path() != p.installation / "data/updates" ||
        !directoryName(pathUtf8(p.package.parent_path().filename())) ||
        !packageArchiveName(pathUtf8(p.package.filename()), p.version, platform(), installedInstallation(p.installation)))
        throw std::runtime_error("Invalid prepared update binding");
    detail::safe(p.package);
    detail::safe(p.helper);
}

} // namespace
bool detail::recordedOutcome(std::string_view state) {
    return state == "waiting" || state == "installed" || state == "failed" || state == "restart-failed" ||
           state == "recovered";
}
namespace {
uint64_t ordinarySize(const fs::path &path, uint64_t maximum) {
    detail::safe(path);
    if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1)
        throw std::runtime_error("Prepared update file is not an ordinary private file");
    const auto size = fs::file_size(path);
    if (!size || size > maximum)
        throw std::runtime_error("Prepared update file exceeds its size limit");
    return size;
}
} // namespace
Prepared detail::stagedMetadata(const fs::path &root, const fs::path &directory) {
    const auto j = Json::parse(detail::read(directory / "prepared.json", 8192));
    return stagedMetadata(root, directory, j);
}
Prepared detail::stagedMetadata(const fs::path &root, const fs::path &directory, const Json &j) {
    if (!j.is_object() || j.value("installation", std::string{}) != pathUtf8(root))
        throw std::runtime_error("Prepared update belongs to another installation");
    Prepared p{directory / utf8Path(j.at("package").get<std::string>()), directory / "CyCodeUpdater.exe",
               root, j.at("version").get<std::string>(), j.at("sha256").get<std::string>()};
    binding(p);
    return p;
}
Attempt detail::outcomeMetadata(const Prepared &p, const Json &j) {
    const auto state = j.at("state").get<std::string>();
    if (!j.at("format").is_number_integer() || j.at("format") != 1 || j.at("version") != p.version ||
        j.at("sha256") != p.digest || !recordedOutcome(state) || !j.at("time").is_number_integer() ||
        j.at("time") < 0 || j.at("time") > 253402300799LL)
        throw std::runtime_error("Invalid update outcome record");
    const auto failure = j.value("failure", std::string{});
    if (!failure.empty() && (state != "failed" || installFailure(failure) == InstallFailure::none))
        throw std::runtime_error("Invalid cached installation failure code");
    return {p.version, state, p.package.parent_path(), j.at("time").get<int64_t>(), failure};
}
static void verify(const Prepared &p, const std::atomic<bool> &cancel, uint64_t archiveLimit) {
    cancelled(cancel);
    binding(p);
    if (!portableInstallation(p.installation))
        throw std::runtime_error("Cannot resume an update in a development or unsupported installation");
    detail::safe(p.installation / "data/updates/pending.json");
    if (fs::exists(p.installation / "data/updates/pending.json"))
        throw std::runtime_error("An interrupted installation requires recovery before another update");
    const auto persisted = detail::stagedMetadata(p.installation, p.package.parent_path());
    if (persisted.version != p.version || persisted.digest != p.digest || persisted.package != p.package)
        throw std::runtime_error("Prepared update metadata changed");
    const auto manifest = Json::parse(detail::read(p.installation / "manifest.json", 2 * 1024 * 1024));
    if (!newer(p.version, CYCODE_VERSION) || !newer(p.version, manifest.at("version").get<std::string>()))
        throw std::runtime_error("Prepared update is no longer newer than this installation");
    const auto &files = manifest.at("files");
    if (!files.is_array() || files.size() > 4095)
        throw std::runtime_error("Invalid installed update manifest");
    std::string expected;
    uint64_t helperSize = 0;
    for (const auto &file : files) {
        if (file.at("path") != "CyCodeUpdater.exe")
            continue;
        if (!expected.empty() || !file.at("size").is_number_unsigned())
            throw std::runtime_error("Invalid installed update helper record");
        expected = file.at("sha256").get<std::string>();
        helperSize = file.at("size").get<uint64_t>();
    }
    if (!detail::digest(expected) || !helperSize || helperSize > 64 * 1024 * 1024 ||
        ordinarySize(p.helper, 64 * 1024 * 1024) != helperSize ||
        ordinarySize(p.installation / "CyCodeUpdater.exe", 64 * 1024 * 1024) != helperSize ||
        detail::hashFile(p.helper, 64 * 1024 * 1024, &cancel) != expected ||
        detail::hashFile(p.installation / "CyCodeUpdater.exe", 64 * 1024 * 1024, &cancel) != expected)
        throw std::runtime_error("Prepared or installed update helper changed; download again or reinstall");
    for (const auto &file : files) {
        const auto name = file.at("path").get<std::string>();
        if (name.find('/') != std::string::npos || !name.ends_with(".dll")) continue;
        const auto size = file.at("size").get<uint64_t>();
        const auto digest = file.at("sha256").get<std::string>();
        if (!size || size > 64 * 1024 * 1024 || !detail::digest(digest) ||
            ordinarySize(p.helper.parent_path() / utf8Path(name), 64 * 1024 * 1024) != size ||
            detail::hashFile(p.helper.parent_path() / utf8Path(name), 64 * 1024 * 1024, &cancel) != digest)
            throw std::runtime_error("Staged update runtime changed");
    }
    if (installedInstallation(p.installation)) {
        const auto owned = detail::managedManifest(manifest);
        for (const auto &[name, file] : owned) {
            const auto source = p.installation / utf8Path(name);
            if (ordinarySize(source, 64 * 1024 * 1024) != file.size ||
                detail::hashFile(source, 64 * 1024 * 1024, &cancel) != file.digest)
                throw std::runtime_error("Installed package files changed; review them before updating");
        }
    }
    ordinarySize(p.package, archiveLimit);
    if (detail::hashFile(p.package, archiveLimit, &cancel) != p.digest)
        throw std::runtime_error("Prepared update SHA-256 verification failed; download it again");
    cancelled(cancel);
}
void verifyPrepared(const Prepared &p, const std::atomic<bool> &cancel) {
    verify(p, cancel, maximumPackage);
}
void recordOutcome(const Prepared &p, std::string_view state, std::string_view failure) {
    binding(p);
    if (!detail::recordedOutcome(state))
        throw std::runtime_error("Invalid update outcome");
    if (!failure.empty() && (state != "failed" || detail::installFailure(failure) == InstallFailure::none))
        throw std::runtime_error("Invalid installation failure code");
    const auto persisted = detail::stagedMetadata(p.installation, p.package.parent_path());
    if (persisted.version != p.version || persisted.digest != p.digest)
        throw std::runtime_error("Prepared update metadata changed");
    const auto path = p.package.parent_path() / "result.json";
    detail::safe(path);
    const auto baseline =
        fs::exists(path) ? std::optional<std::string>(detail::read(path, 8192)) : std::nullopt;
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    Json outcome{
        {"format", 1}, {"version", p.version}, {"sha256", p.digest}, {"state", state}, {"time", now}};
    if (!failure.empty())
        outcome["failure"] = failure;
    writeTextFileAtomic(path, outcome.dump(2) + "\n", baseline);
}
Inventory inspect(const fs::path &supplied, const std::atomic<bool> &cancel) {
    Inventory result;
    cancelled(cancel);
    const auto root = fs::absolute(supplied).lexically_normal();
    if (!portableInstallation(root))
        return result;
    const auto base = root / "data/updates";
    detail::safe(base);
    if (!fs::exists(base))
        return result;
    std::vector<Prepared> candidates;
    size_t visited = 0;
    for (const auto &entry : fs::directory_iterator(base)) {
        cancelled(cancel);
        if (++visited > 128) {
            result.limited = true;
            break;
        }
        if (!directoryName(pathUtf8(entry.path().filename())))
            continue;
        try {
            detail::safe(entry.path());
            if (!entry.is_directory())
                continue;
            if (!fs::exists(entry.path() / "prepared.json")) {
                if (fs::exists(entry.path() / "resume.json")) {
                    auto partial = detail::partialMetadata(root, entry.path());
                    if (newer(partial.version, CYCODE_VERSION))
                        result.partials.push_back(std::move(partial));
                }
                continue;
            }
            auto p = detail::stagedMetadata(root, entry.path());
            if (fs::exists(entry.path() / "result.json")) {
                const auto j = Json::parse(detail::read(entry.path() / "result.json", 8192));
                result.attempts.push_back(detail::outcomeMetadata(p, j));
            }
            if (newer(p.version, CYCODE_VERSION))
                candidates.push_back(std::move(p));
        } catch (...) {
            cancelled(cancel);
            ++result.rejected;
        }
    }
    std::sort(result.attempts.begin(), result.attempts.end(), [](const Attempt &a, const Attempt &b) {
        return a.time != b.time ? a.time > b.time : a.directory < b.directory;
    });
    std::sort(result.partials.begin(), result.partials.end(), [](const auto &a, const auto &b) {
        return a.version != b.version     ? newer(a.version, b.version)
               : a.received != b.received ? a.received > b.received
                                          : a.directory < b.directory;
    });
    if (result.partials.size() > 12) {
        result.partials.resize(12);
        result.limited = true;
    }
    if (result.attempts.size() > 12) {
        result.attempts.resize(12);
        result.limited = true;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Prepared &a, const Prepared &b) {
        return a.version != b.version ? newer(a.version, b.version) : a.package < b.package;
    });
    // Bound both work and bytes even with many corrupt cached archives.
    uint64_t budget = 2 * maximumPackage;
    size_t checked = 0;
    for (const auto &candidate : candidates) {
        cancelled(cancel);
        try {
            if (++checked > 4) {
                result.limited = true;
                break;
            }
            const auto size = ordinarySize(candidate.package, maximumPackage);
            if (size > budget) {
                result.limited = true;
                break;
            }
            budget -= size;
            verify(candidate, cancel, size);
            result.prepared = candidate;
            break;
        } catch (...) {
            cancelled(cancel);
            ++result.rejected;
        }
    }
    return result;
}
} // namespace cycode::updates

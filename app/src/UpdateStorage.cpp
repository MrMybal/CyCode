#include "cycode/UpdateStorage.h"
#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/update/detail/UpdateManifest.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace cycode::updates {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr uint64_t payloadLimit = 2 * maximumPackage;
struct Budget : std::runtime_error {
    Budget() : std::runtime_error("Update storage inspection budget reached") {}
};
void cancelled(const std::atomic<bool> &cancel) {
    if (cancel)
        throw std::runtime_error("Update storage action cancelled");
}
bool format(const Json &j, int value) {
    return j.contains("format") && j.at("format").is_number_integer() && j.at("format") == value;
}
struct Root {
    fs::path path;
    std::string version, digest;
};
Root root(const fs::path &supplied, const std::atomic<bool> &cancel) {
    cancelled(cancel);
    detail::safe(supplied);
    const auto path = fs::absolute(supplied).lexically_normal();
    if (platform() != "windows-x64" || !portableInstallation(path))
        throw std::runtime_error("Update storage requires a Windows x64 portable installation");
    detail::safe(path / "data/updates/pending.json");
    if (fs::exists(path / "data/updates/pending.json"))
        throw std::runtime_error("Recover the pending installation before managing cached downloads");
    const auto bytes = detail::read(path / "manifest.json", 2 * 1024 * 1024);
    const auto manifest = Json::parse(bytes);
    (void)detail::managedManifest(manifest);
    const auto hash = detail::hashFile(path / "manifest.json", 2 * 1024 * 1024, &cancel);
    if (detail::read(path / "manifest.json", 2 * 1024 * 1024) != bytes)
        throw std::runtime_error("Installed manifest changed");
    return {path, manifest.at("version").get<std::string>(), hash};
}
Json fileRows(const std::vector<StoredFile> &files) {
    Json rows = Json::object();
    for (const auto &file : files)
        rows[file.name] = {{"size", file.size}, {"sha256", file.digest}};
    return rows;
}
struct Record {
    CachedDownload download;
    std::string metadataBytes;
};
Record record(const Root &r, const fs::path &directory, const std::atomic<bool> &cancel) {
    const auto prepared = detail::stagedMetadata(r.path, directory);
    if (directory.parent_path() != r.path / "data/updates" ||
        !detail::workName(pathUtf8(directory.filename()), "download-"))
        throw std::runtime_error("Invalid cached download directory");
    const auto bytes = detail::read(directory / "prepared.json", 8192);
    const auto j = Json::parse(bytes);
    if (!format(j, 2) || !j.at("files").is_object() || j.at("files").size() < 2 || j.at("files").size() > 3)
        throw std::runtime_error("Legacy cache metadata is protected");
    const auto archive = pathUtf8(prepared.package.filename());
    if (j.at("version") != prepared.version || j.at("sha256") != prepared.digest ||
        j.at("installation") != pathUtf8(r.path) || j.at("package") != archive)
        throw std::runtime_error("Cache metadata changed while validating its binding");
    Record result;
    auto &d = result.download;
    d.directory = directory;
    d.version = prepared.version;
    result.metadataBytes = bytes;
    for (const auto &[name, row] : j.at("files").items()) {
        const auto hash = row.at("sha256").get<std::string>();
        const auto &size = row.at("size");
        const auto limit = name == archive               ? maximumPackage
                           : name == "CyCodeUpdater.exe" ? 64ull * 1024 * 1024
                                                         : 4ull * 1024 * 1024;
        if ((name != archive && name != "CyCodeUpdater.exe" && name != "updater-messages.json") ||
            !detail::digest(hash) || !size.is_number_integer() || size <= 0 || size > limit ||
            (name == archive && hash != prepared.digest))
            throw std::runtime_error("Invalid owned cache payload");
        d.files.push_back({name, hash, size.get<uint64_t>()});
        d.bytes += size.get<uint64_t>();
    }
    if (!j.at("files").contains(archive) || !j.at("files").contains("CyCodeUpdater.exe") ||
        d.bytes > payloadLimit)
        throw std::runtime_error("Incomplete cache ownership record");
    d.metadataDigest = detail::hashFile(directory / "prepared.json", 8192, &cancel);
    if (detail::read(directory / "prepared.json", 8192) != bytes)
        throw std::runtime_error("Cache metadata changed");
    std::set<std::string> allowed{"prepared.json", "result.json", "update-error.txt"};
    for (const auto &file : d.files)
        allowed.insert(file.name);
    size_t visited = 0;
    for (const auto &entry : fs::directory_iterator(directory)) {
        cancelled(cancel);
        if (++visited > 8 || !allowed.contains(pathUtf8(entry.path().filename())))
            throw std::runtime_error("Unknown cache content is protected");
        detail::safe(entry.path());
        if (!entry.is_regular_file() || fs::hard_link_count(entry.path()) != 1)
            throw std::runtime_error("Unsafe cache file");
    }
    return result;
}
Json observed(const fs::path &path, const StoredFile &file, uint64_t &budget,
              const std::atomic<bool> &cancel) {
    cancelled(cancel);
    detail::safe(path);
    if (!fs::exists(path))
        return {{"missing", true}};
    if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1 || fs::file_size(path) != file.size)
        return {{"changed", true}};
    if (file.size > budget)
        throw Budget();
    budget -= file.size;
    const auto hash = detail::hashFile(path, file.size, &cancel);
    return {{"size", file.size}, {"sha256", hash}};
}
bool matches(const Json &j, const StoredFile &file) {
    return j.contains("sha256") && j.at("sha256") == file.digest && j.at("size") == file.size;
}
bool missing(const Json &j) {
    return j.contains("missing");
}
CachedDownload candidate(const Root &r, const fs::path &directory, uint64_t &budget,
                         const std::atomic<bool> &cancel) {
    auto entry = record(r, directory, cancel);
    auto &d = entry.download;
    if (!newer(r.version, d.version) || !newer(CYCODE_VERSION, d.version))
        throw std::runtime_error("Current or newer cached releases are protected");
    Json baseline{{"metadata", d.metadataDigest}, {"files", Json::object()}};
    if (fs::exists(directory / "result.json")) {
        const auto bytes = detail::read(directory / "result.json", 8192);
        const auto j = Json::parse(bytes);
        const auto state = j.at("state").get<std::string>();
        if (!format(j, 1) || j.at("version") != d.version ||
            j.at("sha256") != detail::stagedMetadata(r.path, directory).digest ||
            !detail::recordedOutcome(state) || state == "waiting")
            throw std::runtime_error("Active or unknown update outcome is protected");
        baseline["outcome"] = bytes;
    }
    for (const auto &file : d.files) {
        const auto state = observed(directory / utf8Path(file.name), file, budget, cancel);
        if (!matches(state, file))
            throw std::runtime_error("Cached payload changed or was already retired");
        baseline["files"][file.name] = state;
    }
    d.baseline = baseline.dump();
    return d;
}
struct Retired {
    RetiredDownload view;
    Record origin;
    std::string receiptBytes;
    std::optional<std::string> statusBytes;
};
Retired retired(const Root &r, const fs::path &directory, uint64_t &budget, const std::atomic<bool> &cancel) {
    if (directory.parent_path() != r.path / "data/updates" ||
        !detail::workName(pathUtf8(directory.filename()), "retired-"))
        throw std::runtime_error("Invalid retired cache binding");
    const auto bytes = detail::read(directory / "retirement.json", 1024 * 1024);
    const auto j = Json::parse(bytes);
    const auto sourceName = j.at("source").get<std::string>();
    if (!format(j, 1) || j.at("installation") != pathUtf8(r.path) ||
        !detail::workName(sourceName, "download-"))
        throw std::runtime_error("Foreign retired cache record");
    Retired result;
    auto &v = result.view;
    v.directory = directory;
    v.source = r.path / "data/updates" / utf8Path(sourceName);
    result.origin = record(r, v.source, cancel);
    const auto &d = result.origin.download;
    if (j.at("metadataSha256") != d.metadataDigest || j.at("version") != d.version ||
        j.at("files") != fileRows(d.files))
        throw std::runtime_error("Retired cache no longer matches its source metadata");
    v.version = d.version;
    v.bytes = d.bytes;
    v.files = d.files;
    result.receiptBytes = bytes;
    if (fs::exists(directory / "status.json")) {
        result.statusBytes = detail::read(directory / "status.json", 8192);
        const auto status = Json::parse(*result.statusBytes);
        const auto phase = status.at("phase").get<std::string>();
        if (!format(status, 1) || status.at("source") != sourceName || status.at("version") != d.version ||
            (phase != "retiring" && phase != "retired" && phase != "restoring" && phase != "restored" &&
             phase != "partial"))
            throw std::runtime_error("Unknown retired-cache status is protected");
    }
    std::set<std::string> names;
    for (const auto &file : d.files)
        names.insert(file.name);
    size_t count = 0;
    for (const auto &entry : fs::directory_iterator(directory)) {
        if (++count > 3)
            throw std::runtime_error("Unknown retired content is protected");
        detail::safe(entry.path());
        const auto name = pathUtf8(entry.path().filename());
        if (name == sourceName && entry.is_directory()) {
            size_t payloads = 0;
            for (const auto &payload : fs::directory_iterator(entry.path())) {
                detail::safe(payload.path());
                if (++payloads > 3 || !names.contains(pathUtf8(payload.path().filename())) ||
                    !payload.is_regular_file() || fs::hard_link_count(payload.path()) != 1)
                    throw std::runtime_error("Unknown retired payload is protected");
            }
        } else if ((name != "retirement.json" && name != "status.json") || !entry.is_regular_file() ||
                   fs::hard_link_count(entry.path()) != 1)
            throw std::runtime_error("Unknown retired content is protected");
    }
    Json baseline{{"receipt", bytes},
                  {"metadata", d.metadataDigest},
                  {"status", result.statusBytes ? Json(*result.statusBytes) : Json(nullptr)},
                  {"files", Json::object()}};
    for (const auto &file : d.files) {
        const auto atSource = observed(v.source / utf8Path(file.name), file, budget, cancel);
        const auto atRetired =
            observed(directory / utf8Path(sourceName) / utf8Path(file.name), file, budget, cancel);
        baseline["files"][file.name] = {{"source", atSource}, {"retired", atRetired}};
        if (matches(atSource, file) && missing(atRetired))
            ++v.unchanged;
        else if (missing(atSource) && matches(atRetired, file))
            ++v.restore;
        else
            ++v.conflicts;
    }
    v.baseline = baseline.dump();
    return result;
}
void status(const fs::path &directory, const CachedDownload &d, const char *phase,
            std::optional<std::string> &baseline) {
    const auto path = directory / "status.json";
    detail::safe(path);
    const auto bytes = Json{{"format", 1},
                            {"source", pathUtf8(d.directory.filename())},
                            {"version", d.version},
                            {"phase", phase}}
                           .dump(2) +
                       "\n";
    writeTextFileAtomic(path, bytes, baseline);
    baseline = bytes;
}
void verifyRoot(const StorageReview &review, const Root &r) {
    if (review.installation != r.path || review.manifestDigest != r.digest)
        throw std::runtime_error("Installation changed after storage review; inspect again");
}
StorageResult perform(const StorageReview &review, size_t index, bool restore,
                      const std::atomic<bool> &cancel) {
    const auto initial = root(review.installation, cancel);
    verifyRoot(review, initial);
    auto installation = installationUse(initial.path);
    if (!installation)
        throw std::runtime_error("Unsupported installation");
    auto cache = detail::cacheLease(initial.path, NativeFileLeaseMode::exclusive);
    const auto r = root(initial.path, cancel);
    verifyRoot(review, r);
    uint64_t budget = payloadLimit;
    CachedDownload d;
    fs::path directory;
    std::optional<std::string> statusBaseline;
    if (restore) {
        if (index >= review.retired.size())
            throw std::runtime_error("Unknown retired cache selection");
        const auto &selected = review.retired[index];
        auto current = retired(r, selected.directory, budget, cancel);
        if (current.view.baseline != selected.baseline || current.view.conflicts || !current.view.restore)
            throw std::runtime_error("Retired cache changed or has conflicts; inspect again");
        directory = selected.directory;
        d = current.origin.download;
        statusBaseline = current.statusBytes;
    } else {
        if (index >= review.downloads.size())
            throw std::runtime_error("Unknown cache selection");
        const auto &selected = review.downloads[index];
        d = candidate(r, selected.directory, budget, cancel);
        if (d.baseline != selected.baseline)
            throw std::runtime_error("Cache changed after review; inspect again");
        directory = detail::work(r.path, "retired-");
        detail::jsonFile(directory / "retirement.json", {{"format", 1},
                                                         {"installation", pathUtf8(r.path)},
                                                         {"source", pathUtf8(d.directory.filename())},
                                                         {"version", d.version},
                                                         {"metadataSha256", d.metadataDigest},
                                                         {"files", fileRows(d.files)}});
    }
    StorageResult result;
    result.directory = directory;
    try {
        cancelled(cancel);
        status(directory, d, restore ? "restoring" : "retiring", statusBaseline);
        for (const auto &file : d.files) {
            cancelled(cancel);
            const auto original = d.directory / utf8Path(file.name);
            const auto saved = directory / d.directory.filename() / utf8Path(file.name);
            const auto from = restore ? saved : original, to = restore ? original : saved;
            if (restore && !fs::exists(from))
                continue; // Previously restored; complete preflight checked it.
            uint64_t one = file.size;
            if (!matches(observed(from, file, one, cancel), file) || fs::exists(to))
                throw std::runtime_error("Cache payload changed before move");
            detail::moveFile(from, to);
            one = file.size;
            if (!matches(observed(to, file, one, cancel), file))
                throw std::runtime_error("Changed payload retained in " + pathUtf8(directory));
            ++result.files;
            result.bytes += file.size;
        }
        budget = payloadLimit;
        verifyRoot(review, root(r.path, cancel));
        auto post = retired(r, directory, budget, cancel);
        if (post.view.conflicts || (restore ? post.view.restore != 0 : post.view.restore != d.files.size()))
            throw std::runtime_error("Cache postflight is incomplete; retain the retirement record");
        status(directory, d, restore ? "restored" : "retired", statusBaseline);
        result.complete = true;
    } catch (const std::exception &e) {
        result.cancelled = cancel.load();
        result.error = e.what();
        try {
            status(directory, d, "partial", statusBaseline);
        } catch (...) {
        }
    }
    return result;
}
} // namespace
StorageReview reviewStorage(const fs::path &supplied, const std::atomic<bool> &cancel) {
    const auto r = root(supplied, cancel);
    StorageReview result;
    result.installation = r.path;
    result.manifestDigest = r.digest;
    const auto base = r.path / "data/updates";
    detail::safe(base);
    if (!fs::exists(base))
        return result;
    size_t visited = 0;
    uint64_t budget = payloadLimit;
    for (const auto &entry : fs::directory_iterator(base)) {
        cancelled(cancel);
        if (++visited > 128) {
            result.limited = true;
            break;
        }
        const auto name = pathUtf8(entry.path().filename());
        try {
            detail::safe(entry.path());
            if (!entry.is_directory())
                continue;
            if (detail::workName(name, "download-")) {
                if (result.downloads.size() >= 8) {
                    result.limited = true;
                    continue;
                }
                result.downloads.push_back(candidate(r, entry.path(), budget, cancel));
            } else if (detail::workName(name, "retired-")) {
                if (result.retired.size() >= 8) {
                    result.limited = true;
                    continue;
                }
                result.retired.push_back(retired(r, entry.path(), budget, cancel).view);
            } else
                ++result.protectedEntries;
        } catch (const Budget &) {
            result.limited = true;
            break;
        } catch (...) {
            cancelled(cancel);
            ++result.rejected;
        }
    }
    std::sort(result.downloads.begin(), result.downloads.end(),
              [](const auto &a, const auto &b) { return a.directory < b.directory; });
    std::sort(result.retired.begin(), result.retired.end(),
              [](const auto &a, const auto &b) { return a.directory < b.directory; });
    return result;
}
StorageResult retireDownload(const StorageReview &review, size_t index, const std::atomic<bool> &cancel) {
    return perform(review, index, false, cancel);
}
StorageResult restoreDownload(const StorageReview &review, size_t index, const std::atomic<bool> &cancel) {
    return perform(review, index, true, cancel);
}
} // namespace cycode::updates

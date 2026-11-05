#include "UpdateBudget.h"
#include "UpdateFiles.h"
#include "UpdateInstall.h"
#include "UpdateManifest.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include "cycode/UpdateRecovery.h"
#include "cycode/Updates.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace cycode::updates {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
struct Entry {
    std::string name;
    size_t offset, size;
};
uint32_t number(const std::string &bytes, size_t offset, size_t count) {
    if (offset > bytes.size() || count > bytes.size() - offset)
        throw std::runtime_error("Truncated update archive");
    uint32_t result = 0;
    for (size_t i = 0; i < count; ++i)
        result |= uint32_t(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
    return result;
}
uint32_t crc32(std::string_view bytes) {
    uint32_t crc = ~0u;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
std::string folded(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return name;
}
std::vector<Entry> entries(const std::string &bytes, const std::string &expected) {
    if (bytes.size() < 22 || bytes.size() > maximumPackage)
        throw std::runtime_error("Invalid update archive size");
    const auto end = bytes.size() - 22;
    if (number(bytes, end, 4) != 0x06054b50 || number(bytes, end + 4, 2) || number(bytes, end + 6, 2) ||
        number(bytes, end + 20, 2))
        throw std::runtime_error("Unsupported update ZIP format");
    const auto count = number(bytes, end + 10, 2), central = number(bytes, end + 16, 4);
    size_t at = central;
    if (!count || count > 4096 || count != number(bytes, end + 8, 2) ||
        uint64_t(central) + number(bytes, end + 12, 4) != end)
        throw std::runtime_error("Invalid update ZIP directory");
    const auto prefix = "CyCode-" + expected + "/";
    std::vector<Entry> result;
    std::set<std::string> names;
    std::vector<std::pair<size_t, size_t>> extents;
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (number(bytes, at, 4) != 0x02014b50 || (number(bytes, at + 8, 2) & ~0x800u) ||
            number(bytes, at + 10, 2) || number(bytes, at + 20, 4) != number(bytes, at + 24, 4) ||
            number(bytes, at + 34, 2))
            throw std::runtime_error("Update ZIP must use stored regular files");
        const auto length = number(bytes, at + 28, 2), extra = number(bytes, at + 30, 2),
                   comment = number(bytes, at + 32, 2), external = number(bytes, at + 38, 4);
        const auto type = (external >> 16) & 0170000;
        if ((type && type != 0100000) || (external & 0x10) || at + 46 + length + extra + comment > end)
            throw std::runtime_error("Update ZIP contains a special file");
        const auto raw = bytes.substr(at + 46, length);
        if (!raw.starts_with(prefix))
            throw std::runtime_error("Update archive root differs from release version");
        Entry entry{raw.substr(prefix.size()), 0, number(bytes, at + 24, 4)};
        if (!packagePath(entry.name) || !names.insert(folded(entry.name)).second ||
            entry.size > 64 * 1024 * 1024)
            throw std::runtime_error("Unsafe or duplicate update path");
        const auto local = size_t(number(bytes, at + 42, 4));
        const auto crc = number(bytes, at + 16, 4);
        if (number(bytes, local, 4) != 0x04034b50 ||
            number(bytes, local + 6, 2) != number(bytes, at + 8, 2) || number(bytes, local + 8, 2) ||
            number(bytes, local + 14, 4) != crc || number(bytes, local + 18, 4) != entry.size ||
            number(bytes, local + 22, 4) != entry.size)
            throw std::runtime_error("Invalid update ZIP local header");
        const auto localName = number(bytes, local + 26, 2), localExtra = number(bytes, local + 28, 2);
        entry.offset = local + 30 + localName + localExtra;
        if (localName != length || bytes.substr(local + 30, localName) != raw ||
            entry.offset + entry.size > central)
            throw std::runtime_error("Invalid update ZIP data range");
        total += entry.size;
        if (total > maximumPackage || crc32(std::string_view(bytes).substr(entry.offset, entry.size)) != crc)
            throw std::runtime_error("Corrupt or oversized update archive");
        extents.emplace_back(local, entry.offset + entry.size);
        result.push_back(std::move(entry));
        at += 46 + length + extra + comment;
    }
    if (at != end)
        throw std::runtime_error("Invalid update ZIP directory length");
    std::sort(extents.begin(), extents.end());
    size_t next = 0;
    for (const auto &[first, last] : extents) {
        if (first != next)
            throw std::runtime_error("Overlapping or unexplained update ZIP bytes");
        next = last;
    }
    if (next != central)
        throw std::runtime_error("Invalid update ZIP payload extent");
    for (const auto &name : names)
        for (auto slash = name.find('/'); slash != name.npos; slash = name.find('/', slash + 1))
            if (names.contains(name.substr(0, slash)))
                throw std::runtime_error("Conflicting update paths");
    return result;
}

} // namespace
static void performApply(const fs::path &archive, const fs::path &installation, const std::string &expected) {
    if (!portableInstallation(installation))
        throw std::runtime_error("Update requires an update-enabled portable distribution");
    (void)version(expected);
    detail::safe(installation);
    const auto root = fs::canonical(installation), pending = root / "data/updates/pending.json";
    detail::safe(pending);
    if (fs::exists(pending))
        throw std::runtime_error("An interrupted update needs recovery; inspect data/updates/pending.json");
    const auto bytes = detail::read(archive, maximumPackage);
    const auto items = entries(bytes, expected);
    const auto record = std::find_if(items.begin(), items.end(),
                                     [](const auto &entry) { return entry.name == "manifest.json"; });
    if (record == items.end() || record->size > 2 * 1024 * 1024)
        throw std::runtime_error("Missing update manifest");
    const auto freshManifest = Json::parse(std::string_view(bytes).substr(record->offset, record->size));
    const auto oldBytes = detail::read(root / "manifest.json", 2 * 1024 * 1024);
    const auto oldManifest = Json::parse(oldBytes);
    if (freshManifest.at("version") != expected ||
        !newer(expected, oldManifest.at("version").get<std::string>()) || !newer(expected, CYCODE_VERSION))
        throw std::runtime_error("Update version does not advance the installed application");
    const auto freshFiles = detail::managedManifest(freshManifest),
               oldFiles = detail::managedManifest(oldManifest);
    std::set<std::string> actual, declared{"manifest.json"};
    const auto affected = detail::managedUnion(oldFiles, freshFiles);
    for (const auto &entry : items)
        actual.insert(entry.name);
    for (const auto &[name, file] : freshFiles) {
        declared.insert(name);
    }
    if (actual != declared)
        throw std::runtime_error("Update ZIP and manifest file sets differ");
    std::map<std::string, std::string> original;
    for (const auto &name : affected) {
        const auto target = root / utf8Path(name);
        detail::safe(target);
        if (fs::exists(target)) {
            if (!fs::is_regular_file(target) || fs::hard_link_count(target) != 1)
                throw std::runtime_error("Update destination is not an independent regular file");
            if (name != "manifest.json" &&
                (!oldFiles.contains(name) || fs::file_size(target) != oldFiles.at(name).size))
                throw std::runtime_error("A managed or new update file has local changes: " + name);
            if (name == "manifest.json" && fs::file_size(target) > 2 * 1024 * 1024)
                throw std::runtime_error("Installed manifest grew beyond its limit");
            const auto hash = sha256File(target);
            if (name != "manifest.json" && (!oldFiles.contains(name) || hash != oldFiles.at(name).digest))
                throw std::runtime_error("A managed or new update file has local changes: " + name);
            original.emplace(name, hash);
        }
    }
    const auto unit = detail::allocationUnit(root);
    uint64_t allocatedPayload = 0, payloadBytes = 0, directoryAllowances = 0;
    for (const auto &entry : items) {
        const auto allocated = detail::allocatedBytes(entry.size, unit);
        if (!allocated)
            throw InstallationError(InstallFailure::preparation, "Invalid update allocation estimate");
        allocatedPayload += *allocated;
        payloadBytes += entry.size;
        directoryAllowances += 2 * std::count(entry.name.begin(), entry.name.end(), '/');
    }
    for (const auto &[name, hash] : original)
        directoryAllowances += std::count(name.begin(), name.end(), '/');
    const auto initialRequired =
                   detail::installationHeadroom(allocatedPayload, items.size(), directoryAllowances, unit),
               commitRequired = detail::installationHeadroom(0, items.size(), directoryAllowances, unit);
    if (!initialRequired || !commitRequired)
        throw InstallationError(InstallFailure::preparation, "Invalid installation-space estimate");
    auto requireSpace = [&](uint64_t required) {
        const auto available = detail::availableSpace(root);
        if (available < required)
            throw InstallationError(InstallFailure::space,
                                    "Insufficient installation space: required " + std::to_string(required) +
                                        " bytes, available " + std::to_string(available) +
                                        " bytes. Installed files were not replaced.");
        return available;
    };
    const auto initialAvailable = requireSpace(*initialRequired);
    const auto directory = detail::work(root, "transaction-"), fresh = directory / "new",
               backup = directory / "backup";
    Json journal = {{"format", 2},
                    {"version", expected},
                    {"installation", pathUtf8(root)},
                    {"phase", "extracting"},
                    {"original", original}};
    std::vector<const std::string *> saved, installed;
    saved.reserve(original.size());
    installed.reserve(declared.size());
    std::map<std::string, std::string> newHashes;
    bool started = false;
    std::string pendingBytes;
    try {
        detail::jsonFile(directory / "space.json", {{"format", 1},
                                                    {"version", expected},
                                                    {"allocationUnit", unit},
                                                    {"payloadBytes", payloadBytes},
                                                    {"allocatedPayload", allocatedPayload},
                                                    {"files", items.size()},
                                                    {"directoryAllowances", directoryAllowances},
                                                    {"initialRequired", *initialRequired},
                                                    {"commitRequired", *commitRequired},
                                                    {"initialAvailable", initialAvailable}});
        for (const auto &entry : items) {
            const auto path = fresh / utf8Path(entry.name);
            fs::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary);
            out.write(bytes.data() + entry.offset, entry.size);
            out.close();
            if (!out)
                throw std::runtime_error("Cannot extract update file");
            if (entry.name != "manifest.json" && (entry.size != freshFiles.at(entry.name).size ||
                                                  sha256File(path) != freshFiles.at(entry.name).digest))
                throw std::runtime_error("Update file digest differs from manifest");
        }
        for (const auto &name : declared)
            newHashes.emplace(name, sha256File(fresh / utf8Path(name)));
        if (detail::read(root / "manifest.json", 2 * 1024 * 1024) != oldBytes)
            throw std::runtime_error("Installed manifest changed during preparation");
        // Immutable recovery inputs are committed before the pending marker and
        // before moving a single original. Recovery never depends on move counters.
        writeTextFileAtomic(directory / "before.json", oldBytes, std::nullopt);
        writeTextFileAtomic(directory / "after.json", detail::read(fresh / "manifest.json", 2 * 1024 * 1024),
                            std::nullopt);
        if (sha256File(directory / "before.json") != original.at("manifest.json") ||
            sha256File(directory / "after.json") != newHashes.at("manifest.json"))
            throw std::runtime_error("Recovery manifest snapshot changed during preparation");
        Json originals = Json::object(), replacements = Json::object();
        for (const auto &[name, hash] : original)
            originals[name] = {{"sha256", hash},
                               {"size", name == "manifest.json" ? oldBytes.size() : oldFiles.at(name).size}};
        for (const auto &[name, hash] : newHashes)
            replacements[name] = {
                {"sha256", hash},
                {"size", name == "manifest.json" ? record->size : freshFiles.at(name).size}};
        detail::jsonFile(directory / "recovery.json", {{"format", 1},
                                                       {"installation", pathUtf8(root)},
                                                       {"version", expected},
                                                       {"original", originals},
                                                       {"installed", replacements}});
        const auto planHash = detail::hashFile(directory / "recovery.json", 8 * 1024 * 1024, nullptr);
        writeTextFileAtomic(
            directory / "RECOVERY.txt",
            "CyCode update recovery / Recuperation de mise a jour\n\n"
            "Close all CyCode instances. Keep pending.json, this transaction and its backups.\n"
            "Run the staged helper below to review restoration of the previous version.\n"
            "Declining confirmation changes no installation file. Local conflicts block restoration.\n\n"
            "Fermez toutes les instances de CyCode. Conservez pending.json, cette transaction et ses "
            "sauvegardes.\n"
            "Lancez l'outil ci-dessous pour examiner la restauration de la version precedente.\n"
            "Le refus de confirmation ne modifie aucun fichier. Les conflits locaux bloquent la "
            "restauration.\n\n" +
                pathUtf8(fs::absolute(archive).parent_path() / "CyCodeUpdater.exe") +
                "\n\n"
                "If the helper or a required backup is missing/modified, extract a complete release into a "
                "new folder.\n"
                "Si l'outil ou une sauvegarde manque ou a ete modifie, extrayez une distribution complete "
                "dans un nouveau dossier.\n",
            std::nullopt);
        const auto commitAvailable = requireSpace(*commitRequired);
        detail::jsonFile(directory / "space-before-commit.json", {{"format", 1},
                                                                  {"version", expected},
                                                                  {"required", *commitRequired},
                                                                  {"available", commitAvailable}});
        pendingBytes = Json{{"format", 2},
                            {"transaction", pathUtf8(directory)},
                            {"version", expected},
                            {"planSha256", planHash}}
                           .dump(2) +
                       "\n";
        if (pendingBytes.size() > 8192)
            throw std::runtime_error("Pending update marker exceeds its size limit");
        journal["phase"] = "applying";
        detail::jsonFile(directory / "journal.json", journal);
        // Create only: a marker appearing during preparation belongs to another
        // actor and must never become the baseline for an overwrite.
        writeTextFileAtomic(pending, pendingBytes, std::nullopt);
        started = true;
        for (const auto &[name, hash] : original) {
            const auto source = root / utf8Path(name), destination = backup / utf8Path(name);
            detail::safe(source);
            fs::create_directories(destination.parent_path());
            fs::rename(source, destination);
            saved.push_back(&name);
            if (sha256File(destination) != hash)
                throw std::runtime_error("Update source changed during installation");
        }
        for (const auto &name : declared) {
            const auto destination = root / utf8Path(name);
            detail::safe(destination);
            fs::create_directories(destination.parent_path());
            if (fs::exists(destination))
                throw std::runtime_error("Update target appeared during installation");
            fs::rename(fresh / utf8Path(name), destination);
            installed.push_back(&name);
        }
        for (const auto &[name, hash] : newHashes)
            if (sha256File(root / utf8Path(name)) != hash)
                throw std::runtime_error("Installed update digest differs from manifest");
        journal["phase"] = "complete";
        detail::jsonFile(directory / "journal.json", journal);
        detail::removeOwnedText(pending, pendingBytes);
    } catch (...) {
        const auto failure = std::current_exception();
        if (!started) {
            // No original was moved. Failure to report this cannot turn a
            // preparation error into a claim that installation recovery is needed.
            try {
                journal["phase"] = "preparation-failed";
                detail::jsonFile(directory / "journal.json", journal);
            } catch (...) {
            }
            std::rethrow_exception(failure);
        }
        bool restored = true;
        for (auto it = installed.rbegin(); it != installed.rend(); ++it) {
            const auto path = root / utf8Path(**it);
            try {
                if (fs::exists(path)) {
                    if (sha256File(path) != newHashes.at(**it)) {
                        restored = false;
                        continue;
                    }
                    fs::remove(path);
                }
            } catch (...) {
                restored = false;
            }
        }
        for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
            std::error_code ec;
            if (fs::exists(root / utf8Path(**it), ec)) {
                restored = false;
                continue;
            }
            fs::rename(backup / utf8Path(**it), root / utf8Path(**it), ec);
            restored &= !ec;
        }
        // Verify the complete original presence/hash baseline, including files
        // never moved by this transaction and paths that were originally absent.
        for (const auto &name : affected) {
            try {
                const auto path = root / utf8Path(name);
                detail::safe(path);
                const auto prior = original.find(name);
                if (prior == original.end()) {
                    if (fs::exists(path))
                        restored = false;
                } else {
                    const uint64_t expectedSize =
                        name == "manifest.json" ? oldBytes.size() : oldFiles.at(name).size;
                    if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1 ||
                        fs::file_size(path) != expectedSize ||
                        detail::hashFile(path, std::max<uint64_t>(1, expectedSize), nullptr) != prior->second)
                        restored = false;
                }
            } catch (...) {
                restored = false;
            }
        }
        journal["phase"] = restored ? "rolled-back" : "recovery-needed";
        try {
            detail::jsonFile(directory / "journal.json", journal);
            if (restored)
                detail::removeOwnedText(pending, pendingBytes);
        } catch (...) {
            restored = false;
        }
        if (!restored) {
            try {
                journal["phase"] = "recovery-needed";
                detail::jsonFile(directory / "journal.json", journal);
            } catch (...) {
            }
            throw InstallationError(InstallFailure::recoveryRequired,
                                    "Update recovery required; preserve the backup and journal in " +
                                        pathUtf8(directory));
        }
        try {
            std::rethrow_exception(failure);
        } catch (const std::exception &error) {
            throw InstallationError(InstallFailure::rolledBack, error.what());
        }
    }
    // Backups and a completed journal intentionally survive success for manual recovery.
}
void apply(const fs::path &archive, const fs::path &installation, const std::string &expected) {
    try {
        performApply(archive, installation, expected);
    } catch (const InstallationError &) {
        throw;
    } catch (const std::exception &error) {
        throw InstallationError(InstallFailure::preparation, error.what());
    }
}
} // namespace cycode::updates

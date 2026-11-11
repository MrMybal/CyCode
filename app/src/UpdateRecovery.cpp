#include "cycode/UpdateRecovery.h"
#include "UpdateBudget.h"
#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/update/detail/UpdateManifest.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include "cycode/UpdateRecoveryProgress.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace cycode::updates {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using detail::ManagedFile;
using detail::ManagedFiles;
constexpr uint64_t manifestLimit = 2 * 1024 * 1024, planLimit = 8 * 1024 * 1024;
constexpr uint64_t workLimit = 65536, preparedLimit = 4 * 1024 * 1024;
constexpr size_t preparedDirectoryLimit = 32;
void cancelled(const std::atomic<bool> &cancel) {
    if (cancel)
        throw detail::Cancellation("Update recovery cancelled");
}
bool format(const Json &j, const char *key, int expected) {
    return j.contains(key) && j.at(key).is_number_integer() && j.at(key) == expected;
}
ManagedFiles records(const Json &j, bool installed) {
    if (!j.is_object() || j.empty() || j.size() > 4096)
        throw std::runtime_error("Invalid recovery file set");
    ManagedFiles result;
    uint64_t total = 0;
    for (const auto &[name, row] : j.items()) {
        const auto hash = row.at("sha256").get<std::string>();
        const auto &size = row.at("size");
        const uint64_t limit = name == "manifest.json" ? manifestLimit : 64 * 1024 * 1024;
        if (!packagePath(name) || !detail::digest(hash) || !size.is_number_integer() || size < 0 ||
            size > limit)
            throw std::runtime_error("Invalid recovery file record");
        total += size.get<uint64_t>();
        if (total > maximumPackage + manifestLimit)
            throw std::runtime_error("Recovery file set exceeds size limit");
        result.emplace(name, ManagedFile{hash, size.get<uint64_t>()});
    }
    if (!result.contains("manifest.json") || (installed && result.size() < productIdentity().requiredFiles.size() + 1) ||
        (!installed && (!result.contains(productIdentity().application) || !result.contains(productIdentity().helper))))
        throw std::runtime_error("Missing recovery manifest record");
    return result;
}
std::string boundBytes(const fs::path &path, const ManagedFile &file, uint64_t limit,
                       const std::atomic<bool> &cancel) {
    auto bytes = detail::read(path, limit);
    if (bytes.size() != file.size || detail::hashFile(path, limit, &cancel) != file.digest ||
        detail::read(path, limit) != bytes)
        throw std::runtime_error("Recovery manifest changed or failed SHA-256 verification");
    return bytes;
}
struct Context {
    fs::path root, directory;
    std::string pendingBytes, planDigest, previousVersion, version, previousHelper, nextHelper;
    ManagedFiles original, installed;
    std::set<std::string> affected;
};
Context load(const fs::path &supplied, const std::atomic<bool> &cancel) {
    cancelled(cancel);
    detail::safe(supplied);
    Context c;
    c.root = fs::canonical(supplied);
    if (!fs::is_directory(c.root) || fs::exists(c.root / ".git") || platform() != "windows-x64")
        throw std::runtime_error("Recovery requires a Windows x64 portable installation");
    c.pendingBytes = detail::read(c.root / "data/updates/pending.json", 8192);
    const auto pending = Json::parse(c.pendingBytes);
    detail::requireProduct(pending);
    if (!format(pending, "format", 2))
        throw std::runtime_error("This older update journal requires manual recovery; retain its backups");
    c.directory = utf8Path(pending.at("transaction").get<std::string>());
    c.planDigest = pending.at("planSha256").get<std::string>();
    c.version = pending.at("version").get<std::string>();
    (void)version(c.version);
    if (!c.directory.is_absolute() || c.directory != c.directory.lexically_normal() ||
        c.directory.parent_path() != c.root / "data/updates" ||
        !detail::workName(pathUtf8(c.directory.filename()), "transaction-") || !detail::digest(c.planDigest))
        throw std::runtime_error("Invalid recovery transaction binding");
    const auto planFile = c.directory / "recovery.json";
    const auto planBytes = detail::read(planFile, planLimit);
    if (detail::hashFile(planFile, planLimit, &cancel) != c.planDigest ||
        detail::read(planFile, planLimit) != planBytes)
        throw std::runtime_error("Recovery plan changed or failed SHA-256 verification");
    const auto plan = Json::parse(planBytes);
    detail::requireProduct(plan);
    if (!format(plan, "format", 1) || plan.at("installation") != pathUtf8(c.root) ||
        plan.at("version") != c.version)
        throw std::runtime_error("Recovery plan belongs to another installation");
    c.original = records(plan.at("original"), false);
    c.installed = records(plan.at("installed"), true);
    const auto before = Json::parse(
        boundBytes(c.directory / "before.json", c.original.at("manifest.json"), manifestLimit, cancel));
    detail::requireProduct(before);
    const auto after = Json::parse(
        boundBytes(c.directory / "after.json", c.installed.at("manifest.json"), manifestLimit, cancel));
    detail::requireProduct(after);
    const auto oldFiles = detail::managedManifest(before), newFiles = detail::managedManifest(after);
    c.previousVersion = before.at("version").get<std::string>();
    if (after.at("version") != c.version || !newer(c.version, c.previousVersion) ||
        c.installed.size() != newFiles.size() + 1)
        throw std::runtime_error("Recovery version or manifest file set differs");
    for (const auto &[name, file] : c.original) {
        if (name != "manifest.json" && (!oldFiles.contains(name) || oldFiles.at(name).digest != file.digest ||
                                        oldFiles.at(name).size != file.size))
            throw std::runtime_error("Recovery originals differ from previous manifest");
    }
    for (const auto &[name, file] : newFiles) {
        if (!c.installed.contains(name) || c.installed.at(name).digest != file.digest ||
            c.installed.at(name).size != file.size)
            throw std::runtime_error("Recovery targets differ from next manifest");
    }
    c.affected = detail::managedUnion(oldFiles, newFiles);
    c.previousHelper = oldFiles.at(productIdentity().helper).digest;
    c.nextHelper = newFiles.at(productIdentity().helper).digest;
    return c;
}
Json observe(const fs::path &path, const std::set<uint64_t> &sizes, const std::atomic<bool> &cancel) {
    cancelled(cancel);
    try {
        detail::safe(path);
        if (!fs::exists(path))
            return {{"exists", false}};
        if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1)
            return {{"bad", "special file"}};
        const auto size = fs::file_size(path);
        if (!sizes.contains(size))
            return {{"bad", "unexpected size"}, {"size", size}};
        const auto hash = detail::hashFile(path, std::max<uint64_t>(1, size), &cancel);
        if (fs::file_size(path) != size)
            return {{"bad", "changed size"}};
        return {{"size", size}, {"sha256", hash}};
    } catch (...) {
        cancelled(cancel);
        return {{"bad", "unreadable or unsafe path"}};
    }
}
bool matches(const Json &observed, const ManagedFile &file) {
    return observed.contains("sha256") && observed.at("sha256") == file.digest &&
           observed.at("size") == file.size;
}
bool absent(const Json &observed) {
    return observed.contains("exists") && observed.at("exists") == false;
}
struct Row {
    std::string name;
    Json target, backup;
    bool restore = false, remove = false;
    fs::path prepared, preparedWork;
    bool preparedReused = false;
};
struct Scan {
    Context context;
    Recovery review;
    std::vector<Row> rows;
    std::map<fs::path, std::string> preparedIdentities;
};
std::string ordinaryBytes(const fs::path &path, uint64_t limit) {
    detail::safe(path);
    if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1)
        throw std::runtime_error("Recovery metadata is not an independent regular file");
    return detail::read(path, limit);
}
std::string workIdentity(const fs::path &work, const Context &c) {
    detail::safe(work);
    if (work.parent_path() != c.root / "data/updates" ||
        !detail::workName(pathUtf8(work.filename()), "recovery-") || !fs::is_directory(work))
        throw std::runtime_error("Invalid prepared recovery directory");
    const auto bytes = ordinaryBytes(work / "work.json", workLimit);
    const auto j = Json::parse(bytes);
    detail::requireProduct(j);
    if (!format(j, "format", 1) || j.at("installation") != pathUtf8(c.root) ||
        j.at("transaction") != pathUtf8(c.directory) || j.at("version") != c.version ||
        j.at("planSha256") != c.planDigest || j.at("pending") != c.pendingBytes)
        throw std::runtime_error("Prepared recovery data belongs to another transaction");
    return bytes;
}
void preparedCopies(Scan &s, const std::atomic<bool> &cancel) {
    if (s.review.conflictCount)
        return;
    auto &space = s.review.space;
    const auto &c = s.context;
    try {
        cancelled(cancel);
        const auto statusFile = c.directory / "recovery-status.json";
        detail::safe(statusFile);
        if (!fs::exists(statusFile))
            return;
        const auto status = Json::parse(ordinaryBytes(statusFile, 8192));
        detail::requireProduct(status);
        if (status.at("planSha256") != c.planDigest)
            throw std::runtime_error("Previous recovery status belongs to another plan");
        const auto hint = utf8Path(status.at("files").get<std::string>());
        if (!hint.is_absolute() || hint != hint.lexically_normal())
            throw std::runtime_error("Invalid prepared recovery hint");
        std::map<fs::path, std::optional<std::string>> identities;
        identities.emplace(hint, workIdentity(hint, c));
        const auto prepared = Json::parse(ordinaryBytes(hint / "prepared.json", preparedLimit));
        detail::requireProduct(prepared);
        if (!format(prepared, "format", 1) || prepared.at("planSha256") != c.planDigest ||
            !prepared.at("files").is_object() || prepared.at("files").size() > 4096)
            throw std::runtime_error("Invalid prepared recovery file map");
        // Validate all names before following any source-directory hint. The map
        // carries only work basenames; neither arbitrary paths nor recursion.
        std::map<std::string, std::string> folders;
        for (const auto &[name, folder] : prepared.at("files").items()) {
            const auto value = folder.get<std::string>();
            if (!c.original.contains(name) || !detail::workName(value, "recovery-"))
                throw std::runtime_error("Invalid prepared recovery source");
            folders.emplace(name, value);
        }
        for (auto &row : s.rows) {
            cancelled(cancel);
            if (!row.restore || !folders.contains(row.name))
                continue;
            const auto work = c.root / "data/updates" / utf8Path(folders.at(row.name));
            auto found = identities.find(work);
            if (found == identities.end()) {
                if (identities.size() >= preparedDirectoryLimit) {
                    space.preparedLimited = true;
                    continue;
                }
                found = identities.emplace(work, std::nullopt).first;
                try {
                    found->second = workIdentity(work, c);
                } catch (const std::exception &) {
                    cancelled(cancel);
                }
            }
            if (!found->second) {
                space.preparedIgnored = true;
                continue;
            }
            const auto path = work / "restored" / utf8Path(row.name);
            const auto file = c.original.at(row.name);
            if (!matches(observe(path, {file.size}, cancel), file)) {
                space.preparedIgnored = true;
                continue;
            }
            row.prepared = path;
            row.preparedWork = work;
            row.preparedReused = true;
            s.preparedIdentities.emplace(work, *found->second);
        }
    } catch (const std::exception &) {
        cancelled(cancel);
        space.preparedIgnored = true;
        s.preparedIdentities.clear();
        for (auto &row : s.rows) {
            row.prepared.clear();
            row.preparedWork.clear();
            row.preparedReused = false;
        }
    }
}
void spaceObservation(Scan &s, const std::atomic<bool> &cancel) {
    auto &space = s.review.space;
    if (s.review.conflictCount)
        return;
    try {
        cancelled(cancel);
        space.allocationUnit = detail::allocationUnit(s.context.root);
        for (const auto &row : s.rows) {
            const auto parents = std::count(row.name.begin(), row.name.end(), '/');
            if (row.restore) {
                const auto bytes = s.context.original.at(row.name).size;
                const auto allocated = detail::allocatedBytes(bytes, space.allocationUnit);
                if (!allocated)
                    throw std::runtime_error("Invalid recovery allocation estimate");
                space.payloadBytes += bytes;
                space.allocatedCopies += *allocated;
                ++space.copyFiles;
                space.directoryAllowances += 2 * parents;
                if (row.preparedReused) {
                    ++space.reusedFiles;
                    space.reusedBytes += bytes;
                    space.reusedAllocation += *allocated;
                }
            }
            if ((row.restore || row.remove) && !absent(row.target)) {
                ++space.retainedFiles;
                space.directoryAllowances += parents;
            }
        }
        const auto required = detail::recoveryResumeHeadroom(
            space.allocatedCopies, space.reusedAllocation, space.copyFiles, space.reusedFiles,
            space.retainedFiles, space.directoryAllowances, space.allocationUnit);
        if (!required)
            throw std::runtime_error("Invalid recovery-space estimate");
        space.requiredBytes = *required;
        space.availableBytes = detail::availableSpace(s.context.root);
        cancelled(cancel);
        space.known = true;
    } catch (const std::exception &error) {
        cancelled(cancel);
        space.error = std::string(error.what()).substr(0, 1024);
    }
}
void requireSpace(const RecoverySpace &space) {
    if (!space.known)
        throw std::runtime_error("Cannot inspect recovery storage: " + space.error);
    if (space.availableBytes < space.requiredBytes)
        throw std::runtime_error("Insufficient recovery space: required " +
                                 std::to_string(space.requiredBytes) + " bytes, available " +
                                 std::to_string(space.availableBytes) +
                                 " bytes. This recovery attempt did not replace installed files.");
}
Json spaceReceipt(const RecoverySpace &space, const Context &c, bool copiesPrepared = false) {
    return {{"format", 1},
            {"version", c.version},
            {"planSha256", c.planDigest},
            {"allocationUnit", space.allocationUnit},
            {"payloadBytes", space.payloadBytes},
            {"allocatedCopies", space.allocatedCopies},
            {"reusedFiles", space.reusedFiles},
            {"reusedBytes", space.reusedBytes},
            {"reusedAllocation", space.reusedAllocation},
            {"preparedHintsIgnored", space.preparedIgnored},
            {"preparedHintsLimited", space.preparedLimited},
            {"copiesPrepared", copiesPrepared},
            {"copyFiles", space.copyFiles},
            {"retainedFiles", space.retainedFiles},
            {"directoryAllowances", space.directoryAllowances},
            {"required", space.requiredBytes},
            {"available", space.availableBytes}};
}
Scan scan(const fs::path &root, const std::atomic<bool> &cancel, bool inspectPrepared = true) {
    Scan s;
    s.context = load(root, cancel);
    auto &c = s.context;
    auto &r = s.review;
    r.installation = c.root;
    r.transaction = c.directory;
    r.previousVersion = c.previousVersion;
    r.version = c.version;
    r.planDigest = c.planDigest;
    r.previousHelper = c.previousHelper;
    r.nextHelper = c.nextHelper;
    Json baseline = {{"pending", c.pendingBytes}, {"plan", c.planDigest}, {"files", Json::array()}};
    for (const auto &name : c.affected) {
        const auto original = c.original.find(name), installed = c.installed.find(name);
        std::set<uint64_t> sizes, backups;
        if (original != c.original.end()) {
            sizes.insert(original->second.size);
            backups.insert(original->second.size);
        }
        if (installed != c.installed.end())
            sizes.insert(installed->second.size);
        Row row{name, observe(c.root / utf8Path(name), sizes, cancel),
                observe(c.directory / "backup" / utf8Path(name), backups, cancel)};
        auto conflict = [&](const char *why) {
            ++r.conflictCount;
            if (r.conflicts.size() < 32)
                r.conflicts.push_back(name + ": " + why);
        };
        if (row.backup.contains("bad") ||
            (!absent(row.backup) && (original == c.original.end() || !matches(row.backup, original->second))))
            conflict("backup changed or is unsafe");
        else if (original != c.original.end() && matches(row.target, original->second))
            ++r.unchanged;
        else if (!absent(row.target) &&
                 (installed == c.installed.end() || !matches(row.target, installed->second)))
            conflict("destination has local changes or is unsafe");
        else if (original != c.original.end()) {
            if (!matches(row.backup, original->second))
                conflict("original and verified backup are missing");
            else {
                row.restore = true;
                ++r.restore;
            }
        } else if (!absent(row.target)) {
            row.remove = true;
            ++r.remove;
        } else
            ++r.unchanged;
        baseline["files"].push_back({{"path", name}, {"target", row.target}, {"backup", row.backup}});
        s.rows.push_back(std::move(row));
    }
    r.baseline = baseline.dump();
    if (inspectPrepared)
        preparedCopies(s, cancel);
    spaceObservation(s, cancel);
    return s;
}

void copy(const fs::path &source, const fs::path &destination, uint64_t size, const std::atomic<bool> &cancel,
          RecoveryProgress &progress) {
#ifdef _WIN32
    detail::safe(source);
    detail::safe(destination);
    fs::create_directories(destination.parent_path());
    detail::safe(destination);
    struct Output {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~Output() {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    } out;
    out.handle = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out.handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create exclusive recovery copy");
    std::ifstream in(source, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read recovery backup");
    std::array<char, 65536> buffer{};
    uint64_t total = 0;
    while (in) {
        cancelled(cancel);
        in.read(buffer.data(), buffer.size());
        const auto count = static_cast<DWORD>(in.gcount());
        total += count;
        if (total > size)
            throw std::runtime_error("Recovery backup grew beyond its recorded size");
        DWORD written = 0;
        if (count) {
            const auto ok = WriteFile(out.handle, buffer.data(), count, &written, nullptr);
            if (written <= count)
                progress.copiedBytes.fetch_add(written);
            if (!ok || written != count)
                throw std::runtime_error("Cannot write recovery copy");
        }
    }
    if (!in.eof() || total != size || !FlushFileBuffers(out.handle))
        throw std::runtime_error("Recovery backup changed while copying");
#else
    throw std::runtime_error("Application update recovery currently requires Windows x64");
#endif
}
} // namespace
Recovery reviewRecovery(const fs::path &root, const std::atomic<bool> &cancel) {
    return scan(root, cancel).review;
}
static Json observation(const RecoverySnapshot &s) {
    return {{"phase", recoveryPhaseCode(s.phase)},
            {"prepared", s.prepared},
            {"prepareTotal", s.prepareTotal},
            {"completed", s.completed},
            {"total", s.total},
            {"copiedBytes", s.copiedBytes},
            {"installationChanged", s.installationChanged}};
}
static void performRecovery(const Recovery &expected, const std::atomic<bool> &cancel,
                            RecoveryProgress &progress) {
    detail::safe(expected.installation);
    if (!expected.installation.is_absolute() ||
        fs::canonical(expected.installation) != expected.installation ||
        fs::exists(expected.installation / ".git") || platform() != "windows-x64")
        throw std::runtime_error("Invalid recovery installation binding");
    detail::safe(expected.installation / "data/updates/instances.lock");
    NativeFileLease lease(expected.installation / "data/updates/instances.lock");
    if (!lease.acquired())
        throw std::runtime_error("Close all application instances and other update helpers before recovery");
    auto s = scan(expected.installation, cancel);
    const auto &c = s.context;
    if (s.review.planDigest != expected.planDigest || s.review.transaction != expected.transaction ||
        s.review.baseline != expected.baseline)
        throw std::runtime_error("Recovery state changed after review; inspect it again");
    if (s.review.conflictCount)
        throw std::runtime_error("Recovery has conflicts; no installation file was changed");
    cancelled(cancel);
    progress.prepareTotal = s.review.restore;
    progress.total = s.review.restore + s.review.remove;
    progress.phase = RecoveryPhase::preparing;
    requireSpace(s.review.space);
    const auto work = detail::work(c.root, "recovery-");
    const auto status = c.directory / "recovery-status.json";
    auto state = [&](const char *phase) {
        detail::jsonFile(status, {{"phase", phase}, {"planSha256", c.planDigest}, {"files", pathUtf8(work)}});
    };
    auto result = [&](const char *outcome, bool pendingCleared) {
        try {
            const auto owner = s.preparedIdentities.find(work);
            if (owner == s.preparedIdentities.end() || workIdentity(work, c) != owner->second)
                return;
            auto record = observation(progress.snapshot());
            record.update({{"format", 1},
                           {"time", std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count()},
                           {"version", c.version},
                           {"planSha256", c.planDigest},
                           {"state", outcome},
                           {"pendingCleared", pendingCleared}});
            const auto bytes = detail::productMetadata(std::move(record)).dump(2) + "\n";
            if (bytes.size() > 8192)
                return;
            detail::safe(work / "recovery-result.json");
            writeTextFileAtomic(work / "recovery-result.json", bytes, std::nullopt);
        } catch (...) { /* Reporting cannot undo a known completed restoration. */
        }
    };
    bool mutated = false, advertised = false;
    try {
        const auto identity = detail::productMetadata(Json{{"format", 1},
                                   {"installation", pathUtf8(c.root)},
                                   {"transaction", pathUtf8(c.directory)},
                                   {"version", c.version},
                                   {"planSha256", c.planDigest},
                                   {"pending", c.pendingBytes}})
                                  .dump(2) +
                              "\n";
        if (identity.size() > workLimit)
            throw std::runtime_error("Recovery work identity exceeds its size limit");
        detail::safe(work / "work.json");
        writeTextFileAtomic(work / "work.json", identity, std::nullopt);
        s.preparedIdentities.emplace(work, identity);
        Json sources = Json::object();
        for (auto &row : s.rows) {
            if (!row.restore)
                continue;
            if (!row.preparedReused) {
                row.prepared = work / "restored" / utf8Path(row.name);
                row.preparedWork = work;
            }
            // The stored map is a hint for the next attempt; every file will
            // still need independent identity/type/size/hash validation.
            sources[row.name] = pathUtf8(row.preparedWork.filename());
        }
        const auto prepared =
            detail::productMetadata(Json{{"format", 1}, {"planSha256", c.planDigest}, {"files", sources}}).dump(2) + "\n";
        if (prepared.size() > preparedLimit)
            throw std::runtime_error("Prepared recovery map exceeds its size limit");
        detail::safe(work / "prepared.json");
        writeTextFileAtomic(work / "prepared.json", prepared, std::nullopt);
        state("preparing");
        advertised = true;
        detail::jsonFile(work / "space.json", spaceReceipt(s.review.space, c));
        // Finish every bounded, verified copy before vacating any destination.
        // Copy/hash failures retain work and original backups with targets intact.
        for (const auto &row : s.rows) {
            cancelled(cancel);
            if (!row.restore)
                continue;
            const auto file = c.original.at(row.name);
            const auto backup = c.directory / "backup" / utf8Path(row.name);
            if (!matches(observe(backup, {file.size}, cancel), file))
                throw std::runtime_error("Recovery backup changed: " + row.name);
            if (!row.preparedReused)
                copy(backup, row.prepared, file.size, cancel, progress);
            if (!matches(observe(row.prepared, {file.size}, cancel), file))
                throw std::runtime_error("Recovery copy failed verification");
            progress.prepared.fetch_add(1);
        }
        progress.phase = RecoveryPhase::verifying;
        const auto beforeCommit = scan(c.root, cancel, false);
        if (beforeCommit.review.planDigest != c.planDigest ||
            beforeCommit.review.baseline != s.review.baseline || beforeCommit.review.conflictCount)
            throw std::runtime_error("Recovery state changed while preparing copies; review it again");
        auto commitSpace = s.review.space;
        commitSpace.known = beforeCommit.review.space.known;
        commitSpace.availableBytes = beforeCommit.review.space.availableBytes;
        commitSpace.error = beforeCommit.review.space.error;
        if (beforeCommit.review.space.allocationUnit != s.review.space.allocationUnit)
            throw std::runtime_error("Recovery volume geometry changed during preparation");
        for (const auto &[sourceWork, baseline] : s.preparedIdentities)
            if (workIdentity(sourceWork, c) != baseline)
                throw std::runtime_error("Prepared recovery ownership changed before replacement");
        for (const auto &row : s.rows)
            if (row.restore && !matches(observe(row.prepared, {c.original.at(row.name).size}, cancel),
                                        c.original.at(row.name)))
                throw std::runtime_error("Prepared recovery copy changed before replacement: " + row.name);
        const auto commitRequired =
            detail::recoveryHeadroom(0, commitSpace.copyFiles, commitSpace.retainedFiles,
                                     commitSpace.directoryAllowances, commitSpace.allocationUnit);
        if (!commitRequired)
            throw std::runtime_error("Invalid recovery commit-space estimate");
        commitSpace.requiredBytes = *commitRequired;
        requireSpace(commitSpace);
        detail::jsonFile(work / "space-before-commit.json", spaceReceipt(commitSpace, c, true));
        state("recovering");
        progress.phase = RecoveryPhase::restoring;
        // Restore the package manifest last, after its managed payload.
        std::stable_sort(s.rows.begin(), s.rows.end(), [](const Row &a, const Row &b) {
            return a.name != "manifest.json" && b.name == "manifest.json";
        });
        for (const auto &row : s.rows) {
            cancelled(cancel);
            if (!row.restore && !row.remove)
                continue;
            const auto target = c.root / utf8Path(row.name);
            const auto newFile = c.installed.find(row.name), oldFile = c.original.find(row.name);
            std::set<uint64_t> sizes;
            if (newFile != c.installed.end())
                sizes.insert(newFile->second.size);
            if (oldFile != c.original.end())
                sizes.insert(oldFile->second.size);
            if (observe(target, sizes, cancel) != row.target)
                throw std::runtime_error("Recovery destination changed: " + row.name);
            if (row.restore &&
                (workIdentity(row.preparedWork, c) != s.preparedIdentities.at(row.preparedWork) ||
                 !matches(observe(row.prepared, {oldFile->second.size}, cancel), oldFile->second)))
                throw std::runtime_error("Prepared recovery copy changed: " + row.name);
            if (!absent(row.target)) {
                const auto retained = work / "replaced" / utf8Path(row.name);
                detail::moveFile(target, retained);
                mutated = true;
                progress.installationChanged = true;
                if (!matches(observe(retained, sizes, cancel), newFile->second))
                    throw std::runtime_error("Changed destination retained in " + pathUtf8(work));
            }
            if (row.restore) {
                detail::moveFile(row.prepared, target);
                mutated = true;
                progress.installationChanged = true;
            }
            progress.completed.fetch_add(1);
        }
        progress.phase = RecoveryPhase::checking;
        const auto post = scan(c.root, cancel, false);
        if (post.review.planDigest != c.planDigest || post.context.pendingBytes != c.pendingBytes ||
            post.review.restore || post.review.remove || post.review.conflictCount)
            throw std::runtime_error("Recovery postflight is incomplete; backups and pending marker remain");
        state("complete");
        detail::removeOwnedText(c.root / "data/updates/pending.json", c.pendingBytes);
        progress.phase = RecoveryPhase::complete;
        result("restored", true);
    } catch (...) {
        const auto error = std::current_exception();
        bool stopped = false;
        try {
            std::rethrow_exception(error);
        } catch (const detail::Cancellation &) {
            stopped = true;
        } catch (...) {
        }
        try {
            if (advertised)
                state(stopped ? (mutated ? "recovery-cancelled" : "preparation-cancelled")
                              : (mutated ? "recovery-needed" : "preparation-failed"));
        } catch (...) {
        }
        if (advertised)
            result(stopped ? "cancelled" : "failed", false);
        std::rethrow_exception(error);
    }
}
void recover(const Recovery &expected, const std::atomic<bool> &cancel, RecoveryProgress *supplied) {
    RecoveryProgress local(expected.planDigest, pathUtf8(expected.transaction));
    auto &progress = supplied ? *supplied : local;
    if (progress.planDigest != expected.planDigest ||
        progress.transaction != pathUtf8(expected.transaction) || progress.claimed.exchange(true))
        throw std::runtime_error("Recovery progress is already used or belongs to another transaction");
    try {
        performRecovery(expected, cancel, progress);
        progress.phase = RecoveryPhase::complete;
    } catch (const detail::Cancellation &) {
        progress.phase = RecoveryPhase::cancelled;
        throw;
    } catch (...) {
        progress.phase = RecoveryPhase::failed;
        throw;
    }
}
std::string recoveryReport(const Recovery &review, const RecoverySnapshot &snapshot) {
    auto report = observation(snapshot);
    report.update({{"format", 1},
                   {"version", review.version},
                   {"previousVersion", review.previousVersion},
                   {"installation", pathUtf8(review.installation)},
                   {"transaction", pathUtf8(review.transaction)},
                   {"planSha256", review.planDigest}});
    auto bytes = report.dump(2) + "\n";
    if (bytes.size() > 32768)
        throw std::runtime_error("Recovery report exceeds its size limit");
    return bytes;
}
} // namespace cycode::updates

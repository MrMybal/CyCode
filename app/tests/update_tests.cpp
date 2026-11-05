#include "../src/UpdateInstall.h"
#include "cycode/Paths.h"
#include "cycode/Updates.h"
#include <fstream>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace cycode;
using Json = nlohmann::json;
void updateRecoveryContracts(const std::filesystem::path &root);
void updateStorageContracts(const std::filesystem::path &root);
void updateDownloadContracts(const std::filesystem::path &root);
void updateInstallContracts(const std::filesystem::path &root);
void updateHistoryContracts(const std::filesystem::path &root);
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool bad = false;
    try {
        fn();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, "Invalid update accepted");
}
Json release() {
    return {
        {"draft", false},
        {"prerelease", false},
        {"tag_name", "v0.3.0"},
        {"body", "Release notes"},
        {"assets", Json::array({{{"name", "CyCode-0.3.0-windows-x64-update.zip"},
                                 {"url", "https://api.github.com/repos/MrMybal/CyCode/releases/assets/17"},
                                 {"size", 42},
                                 {"digest", "sha256:" + std::string(64, 'a')},
                                 {"state", "uploaded"}}})}};
}
#ifdef _WIN32
void put(const std::filesystem::path &path, const std::string &text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}
uint32_t crc(const std::string &text) {
    uint32_t c = ~0u;
    for (unsigned char b : text) {
        c ^= b;
        for (int n = 0; n < 8; ++n)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}
void word(std::string &bytes, uint32_t value, int count) {
    for (int i = 0; i < count; ++i)
        bytes += char(value >> (i * 8));
}
std::string zip(const std::map<std::string, std::string> &files) {
    std::string data, central;
    for (const auto &[name, bytes] : files) {
        const auto offset = data.size();
        const auto hash = crc(bytes);
        word(data, 0x04034b50, 4);
        word(data, 20, 2);
        word(data, 0, 2);
        word(data, 0, 2);
        word(data, 0, 4);
        word(data, hash, 4);
        word(data, bytes.size(), 4);
        word(data, bytes.size(), 4);
        word(data, name.size(), 2);
        word(data, 0, 2);
        data += name;
        data += bytes;
        word(central, 0x02014b50, 4);
        word(central, 20, 2);
        word(central, 20, 2);
        word(central, 0, 2);
        word(central, 0, 2);
        word(central, 0, 4);
        word(central, hash, 4);
        word(central, bytes.size(), 4);
        word(central, bytes.size(), 4);
        word(central, name.size(), 2);
        word(central, 0, 2);
        word(central, 0, 2);
        word(central, 0, 2);
        word(central, 0, 2);
        word(central, 0, 4);
        word(central, offset, 4);
        central += name;
    }
    const auto at = data.size();
    data += central;
    word(data, 0x06054b50, 4);
    word(data, 0, 4);
    word(data, files.size(), 2);
    word(data, files.size(), 2);
    word(data, central.size(), 4);
    word(data, at, 4);
    word(data, 0, 2);
    return data;
}
Json manifest(const std::filesystem::path &root, const std::string &version,
              const std::vector<std::string> &files) {
    Json rows = Json::array();
    for (const auto &file : files)
        rows.push_back({{"path", file},
                        {"size", std::filesystem::file_size(root / utf8Path(file))},
                        {"sha256", updates::sha256File(root / utf8Path(file))}});
    return {{"version", version}, {"updateFormat", 1}, {"platform", "windows-x64"}, {"files", rows}};
}
#endif
} // namespace
void updateContracts(const std::filesystem::path &parent) {
    updateHistoryContracts(parent);
    updateStorageContracts(parent);
    updateDownloadContracts(parent);
    updateInstallContracts(parent);
    check(updates::newer("v0.10.0", "0.9.9") && !updates::newer("v0.2.0", "0.2.0"),
          "Release comparison is lexical");
    for (const auto *value : {"1.2", "1.2.3.4", "v1.2.3-beta", "01.2.3", "1.2.65536", "1.2.3\n"})
        rejects([&] { updates::version(value); });
    const auto parsed = updates::parseRelease(release(), "0.2.0", "windows-x64");
    check(parsed && parsed->version == "0.3.0" && parsed->asset.size == 42, "Compatible release lost");
    check(!updates::parseRelease(release(), "0.3.0", "windows-x64"), "Equal release proposed");
    check(updates::parseRelease(release(), "0.2.0", "unsupported")->asset.url.empty(),
          "Wrong platform package selected");
    for (const auto &patch : Json::array({{{"draft", true}}, {{"prerelease", true}}})) {
        auto raw = release();
        raw.update(patch);
        rejects([&] { updates::parseRelease(raw, "0.2.0", "windows-x64"); });
    }
    auto raw = release();
    raw["assets"][0]["url"] = "https://example.org/update";
    rejects([&] { updates::parseRelease(raw, "0.2.0", "windows-x64"); });
    raw = release();
    raw["assets"][0]["digest"] = nullptr;
    check(updates::parseRelease(raw, "0.2.0", "windows-x64")->asset.url.empty(),
          "Old digestless release was automatically installable");
    raw = release();
    raw["assets"].push_back(raw["assets"][0]);
    rejects([&] { updates::parseRelease(raw, "0.2.0", "windows-x64"); });
    for (const auto *path : {"../CyCode.exe", "app/data/prefs.json", "runtime/config", "app/CON.txt",
                             "app/x.", "app/a:b", "app//x", "app/.git/config"})
        check(!updates::packagePath(path), "Private or unsafe update path allowed");
#ifdef _WIN32
    const auto root = parent / "update-fixture", fresh = parent / "update-payload";
    std::filesystem::create_directories(root);
    const std::vector<std::string> owned = {"CyCode.exe",      "cycode_cli.exe",      "CyCodeUpdater.exe",
                                            "app/CyTool.json", "app/locales/ui.json", "LICENSE",
                                            "COPYRIGHT"};
    for (const auto &file : owned) {
        put(root / utf8Path(file), "Old data-only fixture");
        put(fresh / utf8Path(file), "New data-only fixture");
    }
    put(root / "manifest.json", manifest(root, "0.1.0", owned).dump());
    put(root / "data/settings/user.json", "preserved");
    put(root / "extensions/user.plugin", "preserved");
    auto newOwned = owned;
    newOwned.push_back("app/added.txt");
    put(fresh / "app/added.txt", "New data-only fixture");
    const auto newManifest = manifest(fresh, "99.0.0", newOwned);
    std::map<std::string, std::string> files;
    for (const auto &file : newOwned)
        files["CyCode-99.0.0/" + file] = "New data-only fixture";
    files["CyCode-99.0.0/manifest.json"] = newManifest.dump();
    const auto archive = parent / "update-fixture.zip";
    put(archive, zip(files));
    // Future opt-in native coverage: persisted download discovery never restores
    // installation consent, and every reuse revalidates archive/helper/ownership.
    const auto stage = root / "data/updates/download-123-456-789";
    const auto stagedArchive = stage / "CyCode-99.0.0-windows-x64-update.zip";
    put(stagedArchive, zip(files));
    put(stage / "CyCodeUpdater.exe", "Old data-only fixture");
    updates::Prepared prepared{stagedArchive, stage / "CyCodeUpdater.exe", root, "99.0.0",
                               updates::sha256File(stagedArchive)};
    const Json preparedRecord{{"version", prepared.version},
                              {"sha256", prepared.digest},
                              {"installation", pathUtf8(root)},
                              {"package", pathUtf8(stagedArchive.filename())}};
    put(stage / "prepared.json", preparedRecord.dump());
    std::atomic<bool> cancelled{false};
    auto inventory = updates::inspect(root, cancelled);
    check(inventory.prepared && inventory.prepared->digest == prepared.digest,
          "Verified staged update was not rediscovered");
    updates::recordOutcome(prepared, "failed");
    inventory = updates::inspect(root, cancelled);
    check(inventory.prepared && inventory.attempts.size() == 1 && inventory.attempts[0].state == "failed",
          "Retryable failure lost archive or outcome history");
    for (const auto *failure : {"preparation", "space", "rolled-back", "recovery-required"}) {
        updates::recordOutcome(prepared, "failed", failure);
        inventory = updates::inspect(root, cancelled);
        check(inventory.prepared && inventory.attempts.size() == 1 &&
                  inventory.attempts[0].state == "failed" && inventory.attempts[0].failure == failure,
              "Structured installation failure lost legacy state or retryable cache");
    }
    rejects([&] { updates::recordOutcome(prepared, "installed", "space"); });
    rejects([&] { updates::recordOutcome(prepared, "failed", "unknown"); });
    const auto outcome = Json::parse(std::ifstream(stage / "result.json"));
    auto invalidOutcome = outcome;
    invalidOutcome["failure"] = "unknown";
    put(stage / "result.json", invalidOutcome.dump());
    inventory = updates::inspect(root, cancelled);
    check(!inventory.prepared && inventory.attempts.empty() && inventory.rejected == 1 &&
              std::filesystem::exists(stagedArchive),
          "Invalid failure code reused or removed a cached package");
    put(stage / "result.json", outcome.dump());
    updates::recordOutcome(prepared, "failed");
    check(updates::inspect(root, cancelled).attempts[0].failure.empty(),
          "Legacy failure retained a stale structured code");
    cancelled = true;
    rejects([&] { updates::inspect(root, cancelled); });
    rejects([&] { updates::verifyPrepared(prepared, cancelled); });
    cancelled = false;
    put(stage / "CyCodeUpdater.exe", "Changed data-only fixture");
    check(!updates::inspect(root, cancelled).prepared, "Changed cached helper was accepted");
    put(stage / "CyCodeUpdater.exe", "Old data-only fixture");
    auto foreign = preparedRecord;
    foreign["installation"] = pathUtf8(parent);
    put(stage / "prepared.json", foreign.dump());
    check(!updates::inspect(root, cancelled).prepared, "Foreign staged metadata was accepted");
    put(stage / "prepared.json", preparedRecord.dump());
    put(root / "data/updates/pending.json", "{}");
    rejects([&] { updates::verifyPrepared(prepared, cancelled); });
    std::filesystem::remove(root / "data/updates/pending.json");
    put(stagedArchive, "Corrupt archive");
    check(!updates::inspect(root, cancelled).prepared, "Corrupt staged archive was accepted");
    put(stagedArchive, zip(files));
    auto outside = prepared;
    outside.package = archive;
    rejects([&] { updates::verifyPrepared(outside, cancelled); });
    const auto before = updates::sha256File(root / "CyCode.exe");
    const auto beforeManifest = updates::sha256File(root / "manifest.json");
    auto badPayload = files;
    badPayload["CyCode-99.0.0/app/added.txt"] = "Incorrect manifest digest with a valid ZIP CRC";
    put(archive, zip(badPayload));
    updates::InstallFailure failure = updates::InstallFailure::none;
    try {
        updates::apply(archive, root, "99.0.0");
    } catch (const updates::InstallationError &error) {
        failure = error.failure();
    }
    check(failure == updates::InstallFailure::preparation &&
              updates::sha256File(root / "CyCode.exe") == before &&
              updates::sha256File(root / "manifest.json") == beforeManifest &&
              !std::filesystem::exists(root / "data/updates/pending.json") &&
              !std::filesystem::exists(root / "app/added.txt"),
          "Extraction failure changed installation or claimed incomplete rollback");
    bool observedPreparation = false;
    for (const auto &entry : std::filesystem::directory_iterator(root / "data/updates")) {
        const auto journal = entry.path() / "journal.json";
        if (entry.is_directory() && std::filesystem::exists(journal) &&
            Json::parse(std::ifstream(journal)).value("phase", "") == "preparation-failed") {
            observedPreparation = std::filesystem::exists(entry.path() / "space.json") &&
                                  !std::filesystem::exists(entry.path() / "backup");
        }
    }
    check(observedPreparation, "Preparation failure lost its budget receipt or moved originals");
    put(archive, zip(files));
    HANDLE locked = CreateFileW((root / "app/CyTool.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(locked != INVALID_HANDLE_VALUE, "Cannot reserve rollback fixture file");
    failure = updates::InstallFailure::none;
    try {
        updates::apply(archive, root, "99.0.0");
    } catch (const updates::InstallationError &error) {
        failure = error.failure();
    }
    CloseHandle(locked);
    check(failure == updates::InstallFailure::rolledBack &&
              updates::sha256File(root / "CyCode.exe") == before &&
              updates::sha256File(root / "manifest.json") == beforeManifest &&
              !std::filesystem::exists(root / "data/updates/pending.json"),
          "Failed update did not restore originals");
    updateRecoveryContracts(root);
    updates::apply(archive, root, "99.0.0");
    check(updates::sha256File(root / "CyCode.exe") == updates::sha256File(fresh / "CyCode.exe") &&
              std::filesystem::exists(root / "extensions/user.plugin") &&
              std::filesystem::exists(root / "data/settings/user.json"),
          "Successful update lost payload or user files");
    rejects([&] { updates::apply(archive, root, "99.0.0"); });
    auto shared = updates::installationUse(root);
    NativeFileLease exclusive(root / "data/updates/instances.lock");
    check(shared && !exclusive.acquired(), "Multiple-instance update exclusion failed");
#else
    (void)parent;
#endif
}

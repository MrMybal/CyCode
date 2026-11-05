#include "cycode/Paths.h"
#include "cycode/UpdateStorage.h"
#include <fstream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Unsafe cache maintenance accepted");
}
void put(const fs::path &path, const std::string &bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << bytes;
}
std::string read(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
} // namespace
void updateStorageContracts(const std::filesystem::path &parent) {
#ifdef _WIN32
    using namespace cycode;
    using namespace cycode::updates;
    const auto root = parent / "update-storage-fixture";
    const auto base = root / "data/updates";
    Json managed = Json::array();
    for (const auto *name : {"CyCode.exe", "CyCodeUpdater.exe", "cycode_cli.exe", "app/CyTool.json",
                             "app/locales/ui.json", "LICENSE", "COPYRIGHT"}) {
        put(root / utf8Path(name), "Installed fixture data");
        managed.push_back({{"path", name},
                           {"size", fs::file_size(root / utf8Path(name))},
                           {"sha256", sha256File(root / utf8Path(name))}});
    }
    const auto manifest = Json{
        {"version", "1.0.0"},
        {"platform", "windows-x64"},
        {"updateFormat", 1},
        {"files", managed}}.dump();
    put(root / "manifest.json", manifest);
    put(root / "data/settings/user.json", "Preserved settings");
    put(base / "transaction-123-456/backup/original.txt", "Protected recovery backup");
    auto stage = [&](const std::string &folder, const std::string &version, bool modern) {
        const auto directory = base / utf8Path(folder);
        const auto archive = "CyCode-" + version + "-windows-x64-update.zip";
        put(directory / utf8Path(archive), "Owned cache archive fixture");
        put(directory / "CyCodeUpdater.exe", "Owned cache helper fixture");
        Json files = Json::object();
        for (const auto &name : {archive, std::string("CyCodeUpdater.exe")})
            files[name] = {{"size", fs::file_size(directory / utf8Path(name))},
                           {"sha256", sha256File(directory / utf8Path(name))}};
        Json metadata{{"version", version},
                      {"installation", pathUtf8(root)},
                      {"sha256", sha256File(directory / utf8Path(archive))},
                      {"package", archive}};
        if (modern) {
            metadata["format"] = 2;
            metadata["files"] = files;
        }
        put(directory / "prepared.json", metadata.dump());
        return directory;
    };
    const auto old = stage("download-123-456-789", "0.0.0", true);
    const auto recent = stage("download-124-456-789", "65535.0.0", true);
    const auto legacy = stage("download-125-456-789", "0.0.0", false);
    const auto archive = old / "CyCode-0.0.0-windows-x64-update.zip";
    const auto archiveBytes = read(archive), metadataBytes = read(old / "prepared.json");
    std::atomic<bool> cancel{false};
    auto review = reviewStorage(root, cancel);
    check(review.downloads.size() == 1 && review.downloads[0].directory == old &&
              !fs::exists(base / "cache.lock") && read(archive) == archiveBytes,
          "Cache review mutated data or selected recent/legacy downloads");
    {
        NativeFileLease active(base / "cache.lock", NativeFileLeaseMode::shared);
        rejects([&] { retireDownload(review, 0, cancel); });
    }
    put(archive, "Changed cached archive");
    rejects([&] { retireDownload(review, 0, cancel); });
    check(read(archive) == "Changed cached archive", "Changed cache bytes were overwritten");
    put(archive, archiveBytes);
    put(root / "manifest.json", manifest + " ");
    rejects([&] { retireDownload(review, 0, cancel); });
    put(root / "manifest.json", manifest);
    put(base / "pending.json", "{}");
    rejects([&] { reviewStorage(root, cancel); });
    fs::remove(base / "pending.json");
    put(old / "user.txt", "Unknown staged data");
    check(reviewStorage(root, cancel).downloads.empty() && read(old / "user.txt") == "Unknown staged data",
          "Unknown staged data was eligible");
    fs::remove(old / "user.txt");
    cancel = true;
    rejects([&] { retireDownload(review, 0, cancel); });
    cancel = false;
    review = reviewStorage(root, cancel);
    const auto moved = retireDownload(review, 0, cancel);
    check(moved.complete && moved.files == 2 && !fs::exists(archive) &&
              fs::exists(moved.directory / old.filename() / archive.filename()) &&
              read(old / "prepared.json") == metadataBytes &&
              fs::exists(base / "transaction-123-456/backup/original.txt") &&
              fs::exists(recent / "CyCodeUpdater.exe") && fs::exists(legacy / "CyCodeUpdater.exe"),
          "Retirement lost metadata/backups or touched protected downloads");
    review = reviewStorage(root, cancel);
    check(review.retired.size() == 1 && review.retired[0].restore == 2 && !review.retired[0].conflicts,
          "Retired cache could not be reviewed for restoration");
    const auto receipt = moved.directory / "retirement.json";
    const auto receiptBytes = read(receipt);
    put(receipt, receiptBytes + " ");
    rejects([&] { restoreDownload(review, 0, cancel); });
    put(receipt, receiptBytes);
    put(archive, "External destination");
    check(reviewStorage(root, cancel).retired[0].conflicts != 0, "Restore target conflict was ignored");
    rejects([&] { restoreDownload(review, 0, cancel); });
    check(read(archive) == "External destination", "Restore overwrote an external destination");
    fs::remove(archive);
    // A preceding interrupted restore may already have returned one payload.
    fs::rename(moved.directory / old.filename() / archive.filename(), archive);
    review = reviewStorage(root, cancel);
    check(review.retired[0].restore == 1 && review.retired[0].unchanged == 1 && !review.retired[0].conflicts,
          "Partial restore was not resumable");
    const auto restored = restoreDownload(review, 0, cancel);
    check(restored.complete && restored.files == 1 && read(archive) == archiveBytes &&
              fs::exists(old / "CyCodeUpdater.exe") && fs::exists(receipt) &&
              fs::exists(root / "data/settings/user.json"),
          "Cache restoration lost payload/history/user data");
    rejects([&] { restoreDownload(review, 0, cancel); });
#else
    (void)parent;
#endif
}

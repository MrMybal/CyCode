#include "../src/UpdateBudget.h"
#include "../src/UpdateDownload.h"
#include "../src/UpdateFeedback.h"
#include "cycode/Paths.h"
#include <fstream>
#include <stdexcept>

namespace {
using cycode::updates::detail::downloadHeadroom;
using cycode::updates::detail::downloadReserve;
using cycode::updates::detail::DownloadResponse;
using cycode::updates::detail::downloadResponsePolicy;
static_assert(downloadHeadroom(100, 0, 10) == downloadReserve + 110);
static_assert(downloadHeadroom(100, 90, 10) == downloadReserve + 20);
static_assert(downloadHeadroom(100, 100, 10) == downloadReserve + 10);
static_assert(downloadHeadroom(cycode::updates::maximumPackage, 0, 64ull * 1024 * 1024) ==
              downloadReserve + cycode::updates::maximumPackage + 64ull * 1024 * 1024);
static_assert(!downloadHeadroom(0, 0, 10));
static_assert(!downloadHeadroom(cycode::updates::maximumPackage + 1, 0, 10));
static_assert(!downloadHeadroom(100, 101, 10));
static_assert(!downloadHeadroom(100, 0, 0));
static_assert(!downloadHeadroom(100, 0, 64ull * 1024 * 1024 + 1));
constexpr bool meterContract() {
    cycode::updates::DownloadMeter meter;
    meter.observe(0, 0, true);
    meter.observe(1, 10, true);
    if (meter.bytesPerSecond != 10 || meter.remaining(10, 100) != 9)
        return false;
    meter.observe(2, 30, true);
    if (meter.bytesPerSecond != 15 || meter.remaining(30, 100) != 5)
        return false;
    meter.observe(6, 30, true);
    if (meter.bytesPerSecond != 0 || meter.remaining(30, 100))
        return false;
    meter.observe(7, 40, true);
    if (meter.bytesPerSecond != 10 || meter.remaining(40, 100) != 6)
        return false;
    meter.observe(8, 0, true); // New transfer/fallback counter cannot underflow.
    if (meter.bytesPerSecond != 0 || meter.remaining(0, 100))
        return false;
    meter.observe(9, 10, true);
    if (meter.remaining(101, 100))
        return false;
    meter.observe(8, 10, true); // Clock rollback resets the sample.
    if (meter.bytesPerSecond != 0)
        return false;
    meter.observe(9, 20, false); // Hashing/staging do not show a transfer rate.
    return !meter.active && !meter.remaining(20, 100);
}
static_assert(meterContract());
constexpr bool meterBoundsContract() {
    cycode::updates::DownloadMeter meter;
    meter.observe(-1, 10, true);
    if (meter.active)
        return false;
    meter.observe(0, 0, true);
    meter.observe(1, 1, true);
    return !meter.remaining(0, UINT64_MAX) && meter.remaining(0, 1) == 1;
}
static_assert(meterBoundsContract());
static_assert(downloadResponsePolicy(200, "", "", "42", 0, 42) == DownloadResponse::receive);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "identity", "25", 17, 42) ==
              DownloadResponse::receive);
static_assert(downloadResponsePolicy(206, "bytes 0-41/42", "", "", 0, 42) == DownloadResponse::receive);
static_assert(downloadResponsePolicy(200, "", "", "42", 17, 42) == DownloadResponse::restart);
static_assert(downloadResponsePolicy(200, "", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 0-41/42", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-40/42", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/43", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/*", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 017-41/42", "", "25", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42, bytes 17-41/42", "", "25", 17, 42) ==
              DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "gzip", "25", 17, 42) ==
              DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "", "-1", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "", "18446744073709551616", 17, 42) ==
              DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "", "25, 25", 17, 42) ==
              DownloadResponse::reject);
static_assert(downloadResponsePolicy(206, "bytes 17-41/42", "", "25 ", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(416, "bytes */42", "", "", 17, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(200, "", "", "", 42, 42) == DownloadResponse::reject);
static_assert(downloadResponsePolicy(200, "", "", "", 0, cycode::updates::maximumPackage + 1) ==
              DownloadResponse::reject);
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
    check(bad, "Unsafe update download accepted");
}
void put(const std::filesystem::path &file, const std::string &data) {
    std::ofstream(file, std::ios::binary) << data;
}
} // namespace
void updateDownloadContracts(const std::filesystem::path &parent) {
    using namespace cycode;
    using namespace cycode::updates;
    using namespace cycode::updates::detail;
    try {
        downloadResponse(206, "bytes 0-41/42", "", "25", 17, 42);
        throw std::logic_error("Unsafe response accepted");
    } catch (const DownloadError &error) {
        check(error.issue() == DownloadIssue::response && downloadFeedback(error.issue()).code == "response",
              "Response rejection lost its structured failure category");
    }
    check(downloadResponse(200, "", "", "42", 0, 42), "Full response refused");
    check(downloadResponse(206, "bytes 17-41/42", "identity", "25", 17, 42), "Resume refused");
    check(downloadResponse(206, "bytes 0-41/42", "", "", 0, 42), "Complete range refused");
    check(!downloadResponse(200, "", "identity", "42", 17, 42), "Ignored range appended");
    for (const auto *range : {"", "bytes 0-41/42", "bytes 17-40/42", "bytes 17-41/43", "bytes 17-41/*",
                              "bytes 17-41/42, bytes 17-41/42", "bytes 017-41/42"})
        rejects([&] { downloadResponse(206, range, "", "25", 17, 42); });
    for (const auto *length : {"-1", "+25", "24", "25, 25", "18446744073709551616", "25 "})
        rejects([&] { downloadResponse(206, "bytes 17-41/42", "", length, 17, 42); });
    rejects([] { downloadResponse(206, "bytes 17-41/42", "gzip", "25", 17, 42); });
    rejects([] { downloadResponse(200, "bytes 0-41/42", "", "42", 0, 42); });
    rejects([] { downloadResponse(200, "", "", "25", 17, 42); });
    rejects([] { downloadResponse(416, "bytes */42", "", "", 17, 42); });
    rejects([] { downloadResponse(200, "", "", "", 42, 42); });
    rejects([] { downloadResponse(200, "", "", "", 0, maximumPackage + 1); });
#ifdef _WIN32
    namespace fs = std::filesystem;
    const auto root = fs::absolute(parent / "download-resume-fixture").lexically_normal();
    const auto directory = root / "data/updates/download-123-456";
    fs::create_directories(directory);
    Release release{"65535.0.0",
                    {},
                    {},
                    {"CyCode-65535.0.0-windows-x64-update.zip",
                     "https://api.github.com/repos/MrMybal/CyCode/releases/assets/17", std::string(64, 'a'),
                     42}};
    createPartialMetadata(root, directory, release);
    auto partial = partialMetadata(root, directory);
    check(partial.received == 0 && sameDownload(partial, release), "Empty checkpoint lost");
    rejects([&] { createPartialMetadata(root, directory, release); });
    put(directory / "download.partial", "Seventeen bytes!!");
    partial = partialMetadata(root, directory);
    check(partial.received == 17 && !fs::exists(root / "data/updates/cache.lock"),
          "Inspection changed cache");
    const auto diskStatus = std::make_shared<DownloadStatus>(42, release.version);
    bool spaceBlocked = false;
    try {
        requireDownloadSpace(directory, 42, 17, 10, diskStatus);
    } catch (const DownloadError &error) {
        check(error.issue() == DownloadIssue::space, "Unexpected disk-space observation failure");
        spaceBlocked = true;
    }
    check(diskStatus->spaceKnown && diskStatus->requiredBytes == *downloadHeadroom(42, 17, 10) &&
              spaceBlocked == (diskStatus->availableBytes < diskStatus->requiredBytes),
          "Disk-space guard ignored its observation");
    rejects([&] { requireDownloadSpace(directory, 0, 17, 10, diskStatus); });
    check(fs::file_size(directory / "download.partial") == 17 &&
              !fs::exists(root / "data/updates/cache.lock"),
          "Space inspection changed retained data");
    std::atomic<bool> cancel{false};
    const auto before = read(directory / "resume.json", 8192);
    auto selected = selectPartial(root, release, DownloadMode::resume, cancel);
    check(selected && selected->directory == directory && selected->received == 17,
          "Matching checkpoint not selected");
    const auto second = root / "data/updates/download-124-456";
    fs::create_directory(second);
    createPartialMetadata(root, second, release);
    put(second / "download.partial", std::string(20, 'x'));
    selected = selectPartial(root, release, DownloadMode::resume, cancel);
    check(selected && selected->directory == second && selected->received == 20,
          "Most advanced checkpoint not selected");
    check(!selectPartial(root / "absent", release, DownloadMode::fresh, cancel),
          "Fresh mode inspected absent storage");
    check(!selectPartial(root, release, DownloadMode::fresh, cancel) &&
              read(directory / "resume.json", 8192) == before &&
              fs::file_size(second / "download.partial") == 20,
          "Fresh selection changed retained data");
    auto otherAsset = release;
    otherAsset.asset.digest[0] = 'b';
    check(!selectPartial(root, otherAsset, DownloadMode::resume, cancel), "Different digest reused old data");
    rejects([&] { selectPartial(root, release, static_cast<DownloadMode>(99), cancel); });
    cancel = true;
    rejects([&] { selectPartial(root, release, DownloadMode::resume, cancel); });
    rejects([&] { selectPartial(root, release, DownloadMode::fresh, cancel); });
    cancel = false;
    check(read(directory / "resume.json", 8192) == before && fs::file_size(second / "download.partial") == 20,
          "Cancelled selection changed retained data");
    for (const auto *url : {"https://github.com/MrMybal/CyCode/releases/assets/17",
                            "https://api.github.com/repos/MrMybal/Other/releases/assets/17",
                            "https://api.github.com/repos/MrMybal/CyCode/releases/assets/17?token=invalid",
                            "https://api.github.com/repos/MrMybal/CyCode/releases/assets/17/extra"}) {
        auto bad = release;
        bad.asset.url = url;
        rejects([&] { downloadBinding(bad); });
    }
    auto changed = release;
    changed.asset.url.back() = '8';
    check(!sameDownload(partial, changed), "Replaced asset resumed");
    changed = release;
    ++changed.asset.size;
    check(!sameDownload(partial, changed), "Changed size resumed");
    changed = release;
    changed.asset.digest[0] = 'b';
    check(!sameDownload(partial, changed), "Changed digest resumed");
    put(directory / "unknown.txt", "Retained external file");
    rejects([&] { partialMetadata(root, directory); });
    fs::remove(directory / "unknown.txt");
    put(directory / "download.partial", std::string(43, 'x'));
    rejects([&] { partialMetadata(root, directory); });
    put(directory / "download.partial", std::string(42, 'x'));
    check(partialMetadata(root, directory).received == 42, "Complete unverified bytes lost");
    fs::rename(directory / "download.partial", directory / release.asset.name);
    check(partialMetadata(root, directory).received == 42, "Interrupted staging lost");
    put(directory / "download.partial", "x");
    rejects([&] { partialMetadata(root, directory); });
    fs::remove(directory / "download.partial");
    const auto marker = directory / "resume.json";
    auto metadata = nlohmann::json::parse(read(marker, 8192));
    metadata["rejected"] = true;
    put(marker, metadata.dump());
    rejects([&] { partialMetadata(root, directory); });
    metadata.erase("rejected");
    metadata["installation"] = pathUtf8(parent);
    put(marker, metadata.dump());
    rejects([&] { partialMetadata(root, directory); });
    put(marker, std::string(8193, 'x'));
    rejects([&] { partialMetadata(root, directory); });
#else
    (void)parent;
#endif
}

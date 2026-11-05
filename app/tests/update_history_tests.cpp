#include "../src/UpdateFiles.h"
#include "../src/UpdateHistoryPolicy.h"
#include "cycode/Paths.h"
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {
using namespace cycode::updates;
using namespace cycode::updates::detail;
static_assert(historyAttention(HistoryKind::installation, "waiting"));
static_assert(historyAttention(HistoryKind::installation, "restart-failed"));
static_assert(historyAttention(HistoryKind::installation, "failed"));
static_assert(!historyAttention(HistoryKind::installation, "installed"));
static_assert(!historyAttention(HistoryKind::installation, "recovered"));
static_assert(historyAttention(HistoryKind::recovery, "cancelled"));
static_assert(!historyAttention(HistoryKind::recovery, "restored"));
static_assert(historyPhase("checking") == RecoveryPhase::checking);
static_assert(!historyPhase("unknown"));
static_assert(historyRecoveryValid("restored", {RecoveryPhase::complete}, true));
static_assert(!historyRecoveryValid("restored", {RecoveryPhase::complete}, false));
static_assert(!historyRecoveryValid("restored", {RecoveryPhase::checking}, true));
static_assert(historyRecoveryValid("cancelled", {RecoveryPhase::restoring, 1, 1, 0, 2, 25, true}, false));
static_assert(!historyRecoveryValid("failed", {RecoveryPhase::complete}, false));
static_assert(!historyRecoveryValid("failed", {RecoveryPhase::preparing, 2, 1, 0, 2}, false));
static_assert(!historyRecoveryValid("failed", {RecoveryPhase::restoring, 1, 1, 1, 2}, false));
static_assert(!historyRecoveryValid("failed", {RecoveryPhase::inspecting, 0, 4097, 0, 8192}, false));
static_assert(!historyRecoveryValid("failed", {RecoveryPhase::restoring, 0, 0, 0, 8193}, false));
static_assert(!historyRecoveryValid("failed",
                                    {RecoveryPhase::preparing, 0, 1, 0, 1, 258ull * 1024 * 1024 + 1}, false));
static_assert(!historyRecoveryValid("unknown", {RecoveryPhase::failed}, false));
namespace fs = std::filesystem;
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void put(const fs::path &file, const std::string &bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << bytes;
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid history accepted");
}
} // namespace
void updateHistoryContracts(const std::filesystem::path &parent) {
#ifdef _WIN32
    using namespace cycode;
    const auto root = fs::absolute(parent / "update-history-fixture").lexically_normal();
    const auto base = root / "data/updates";
    put(root / "CyCode.exe", "Data-only fixture");
    put(root / "CyCodeUpdater.exe", "Data-only fixture");
    put(root / "manifest.json",
        Json{{"version", "1.0.0"}, {"platform", "windows-x64"}, {"updateFormat", 1}, {"files", Json::array()}}
            .dump());
    for (int i = 0; i < 15; ++i) {
        const auto directory = base / ("download-" + std::to_string(100 + i));
        put(directory / "prepared.json", Json{{"version", "2.0.0"},
                                              {"installation", pathUtf8(root)},
                                              {"package", "CyCode-2.0.0-windows-x64-update.zip"},
                                              {"sha256", std::string(64, 'a')}}
                                             .dump());
        put(directory / "result.json", Json{{"format", 1},
                                            {"version", "2.0.0"},
                                            {"sha256", std::string(64, 'a')},
                                            {"state", "failed"},
                                            {"failure", "space"},
                                            {"time", 1000 + i},
                                            {"untrustedExtra", "excluded-extra"}}
                                           .dump());
    }
    const auto transaction = base / "transaction-200";
    const auto work = transaction / "recovery-201";
    const auto marker = Json{{"format", 2},
                             {"transaction", pathUtf8(transaction)},
                             {"version", "2.0.0"},
                             {"planSha256", std::string(64, 'b')}}
                            .dump();
    const auto owner = Json{
        {"format", 1},        {"installation", pathUtf8(root)},     {"transaction", pathUtf8(transaction)},
        {"version", "2.0.0"}, {"planSha256", std::string(64, 'b')}, {"pending", marker}};
    put(work / "work.json", owner.dump());
    const auto receipt = Json{{"format", 1},
                              {"version", "2.0.0"},
                              {"planSha256", std::string(64, 'b')},
                              {"state", "cancelled"},
                              {"phase", "restoring"},
                              {"prepared", 1},
                              {"prepareTotal", 1},
                              {"completed", 0},
                              {"total", 2},
                              {"copiedBytes", 42},
                              {"installationChanged", true},
                              {"pendingCleared", false},
                              {"technical", "excluded-technical"}};
    put(work / "recovery-result.json", receipt.dump());
    std::atomic<bool> cancel{false};
    auto history = inspectHistory(root, cancel);
    check(history.entries.size() == 16 && history.rejected == 0 && !history.limited &&
              !fs::exists(base / "cache.lock") && !fs::exists(base / "instances.lock"),
          "History inspection lost metadata or created a lock/payload");
    check(history.entries.front().time == 1014 && !history.entries.back().time &&
              history.entries.back().recovery->copiedBytes == 42,
          "Recorded times or legacy unknown-time ordering changed");
    const auto report = historyReport(history);
    const auto parsed = Json::parse(report);
    check(parsed.at("observationOnly") == true && parsed.at("entries").size() == 16 &&
              parsed.at("entries").at(15).at("pendingCleared") == false &&
              parsed.at("entries").at(15).at("directory").get<std::string>().starts_with("data/updates/") &&
              report.find("excluded-extra") == std::string::npos &&
              report.find("excluded-technical") == std::string::npos,
          "History report included raw metadata or lost advisory context");
    check(Json::parse(historyReport(history, 15)).at("entries").size() == 1,
          "Selected report included other entries");
    rejects([&] { historyReport(history, 16); });
    auto invalid = receipt;
    invalid["pendingCleared"] = true;
    put(work / "recovery-result.json", invalid.dump());
    history = inspectHistory(root, cancel);
    check(history.entries.size() == 15 && history.rejected == 1, "Inconsistent recovery receipt displayed");
    put(work / "recovery-result.json", receipt.dump());
    auto foreign = owner;
    foreign["installation"] = pathUtf8(parent);
    put(work / "work.json", foreign.dump());
    check(inspectHistory(root, cancel).rejected == 1, "Foreign recovery identity accepted");
    put(work / "work.json", owner.dump());
    invalid = receipt;
    invalid["time"] = std::numeric_limits<uint64_t>::max();
    put(work / "recovery-result.json", invalid.dump());
    check(inspectHistory(root, cancel).rejected == 1, "Overflowing history time accepted");
    invalid = receipt;
    invalid["prepared"] = -1;
    put(work / "recovery-result.json", invalid.dump());
    check(inspectHistory(root, cancel).rejected == 1, "Negative recovery counter accepted");
    put(work / "recovery-result.json", receipt.dump());
    put(base / "download-100/result.json", std::string(8193, 'x'));
    check(inspectHistory(root, cancel).rejected == 1, "Oversized history metadata accepted");
    cancel = true;
    rejects([&] { inspectHistory(root, cancel); });
    cancel = false;
    for (int i = 0; i < 550; ++i)
        fs::create_directories(base / ("unknown-" + std::to_string(i)));
    history = inspectHistory(root, cancel);
    check(history.limited && history.visited <= 640 && history.metadataBytes <= 4 * 1024 * 1024,
          "History inspection exceeded its observed-directory/metadata bounds");
#else
    (void)parent;
#endif
}

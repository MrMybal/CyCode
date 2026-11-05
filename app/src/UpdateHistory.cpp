#include "cycode/UpdateHistory.h"
#include "UpdateFiles.h"
#include "UpdateHistoryPolicy.h"
#include "cycode/Paths.h"
#include <algorithm>
#include <stdexcept>

namespace cycode::updates {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr uint64_t metadataLimit = 4 * 1024 * 1024;
struct Budget {};
struct Reader {
    History &history;
    const std::atomic<bool> &cancel;
    void checkpoint() const {
        if (cancel)
            throw detail::Cancellation("Update history inspection cancelled");
    }
    Json read(const fs::path &path, uint64_t limit = 8192) {
        checkpoint();
        detail::safe(path);
        if (!fs::is_regular_file(path) || fs::hard_link_count(path) != 1 || fs::file_size(path) > limit)
            throw std::runtime_error("Invalid update history metadata file");
        const auto remaining = metadataLimit - history.metadataBytes;
        if (fs::file_size(path) > remaining)
            throw Budget{};
        const auto bytes = detail::read(path, std::min(limit, remaining));
        history.metadataBytes += bytes.size();
        checkpoint();
        if (fs::hard_link_count(path) != 1)
            throw std::runtime_error("Update history metadata changed during inspection");
        return Json::parse(bytes);
    }
};
uint64_t counter(const Json &j, const char *key, uint64_t limit) {
    const auto &value = j.at(key);
    if (!value.is_number_unsigned() || value.get<uint64_t>() > limit)
        throw std::runtime_error("Invalid recovery history counter");
    return value.get<uint64_t>();
}
std::optional<int64_t> recordedTime(const Json &j) {
    if (!j.contains("time"))
        return std::nullopt;
    const auto &value = j.at("time");
    if (!value.is_number_integer() || value < 0 || value > 253402300799LL)
        throw std::runtime_error("Invalid recovery history time");
    return value.get<int64_t>();
}
HistoryEntry recovery(Reader &reader, const fs::path &transaction, const fs::path &work) {
    const auto &root = reader.history.installation;
    const auto owner = reader.read(work / "work.json", 65536);
    if (!owner.at("format").is_number_integer() || owner.at("format") != 1 ||
        owner.at("installation") != pathUtf8(root) || owner.at("transaction") != pathUtf8(transaction))
        throw std::runtime_error("Foreign recovery history identity");
    const auto release = owner.at("version").get<std::string>();
    (void)version(release);
    const auto digest = owner.at("planSha256").get<std::string>();
    if (!detail::digest(digest))
        throw std::runtime_error("Invalid recovery history plan identity");
    const auto pending = owner.at("pending").get<std::string>();
    if (pending.size() > 8192)
        throw std::runtime_error("Recovery history pending identity exceeds its limit");
    const auto binding = Json::parse(pending);
    if (!binding.at("format").is_number_integer() || binding.at("format") != 2 ||
        binding.at("transaction") != pathUtf8(transaction) || binding.at("version") != release ||
        binding.at("planSha256") != digest)
        throw std::runtime_error("Recovery history pending identity differs");
    const auto j = reader.read(work / "recovery-result.json");
    if (!j.at("format").is_number_integer() || j.at("format") != 1 || j.at("version") != release ||
        j.at("planSha256") != digest || !j.at("installationChanged").is_boolean() ||
        !j.at("pendingCleared").is_boolean())
        throw std::runtime_error("Invalid recovery history receipt");
    const auto phase = detail::historyPhase(j.at("phase").get<std::string>());
    if (!phase)
        throw std::runtime_error("Unknown recovery history phase");
    RecoverySnapshot snapshot{*phase,
                              uint32_t(counter(j, "prepared", 4096)),
                              uint32_t(counter(j, "prepareTotal", 4096)),
                              uint32_t(counter(j, "completed", 8192)),
                              uint32_t(counter(j, "total", 8192)),
                              counter(j, "copiedBytes", 258ull * 1024 * 1024),
                              j.at("installationChanged").get<bool>()};
    const auto state = j.at("state").get<std::string>();
    const auto cleared = j.at("pendingCleared").get<bool>();
    if (!detail::historyRecoveryValid(state, snapshot, cleared))
        throw std::runtime_error("Inconsistent recovery history outcome");
    return {HistoryKind::recovery, release,         state,    {},     digest, work,
            transaction,           recordedTime(j), snapshot, cleared};
}
} // namespace
History inspectHistory(const fs::path &supplied, const std::atomic<bool> &cancel) {
    History result;
    Reader reader{result, cancel};
    reader.checkpoint();
    result.installation = fs::absolute(supplied).lexically_normal();
    detail::safe(result.installation);
    if (!portableInstallation(result.installation))
        return result;
    const auto base = result.installation / "data/updates";
    detail::safe(base);
    if (!fs::exists(base))
        return result;
    std::vector<fs::path> downloads, transactions;
    size_t visited = 0, works = 0;
    for (const auto &entry : fs::directory_iterator(base)) {
        reader.checkpoint();
        if (++visited > 512) {
            result.limited = true;
            break;
        }
        ++result.visited;
        const auto name = pathUtf8(entry.path().filename());
        if (detail::workName(name, "download-"))
            downloads.push_back(entry.path());
        else if (detail::workName(name, "transaction-"))
            transactions.push_back(entry.path());
    }
    // Stable traversal within the observed bounded prefix. Times below are advisory.
    std::sort(downloads.begin(), downloads.end());
    std::sort(transactions.begin(), transactions.end());
    auto attempt = [&](const auto &fn) {
        reader.checkpoint();
        try {
            fn();
        } catch (const Budget &) {
            result.limited = true;
            return false;
        } catch (const detail::Cancellation &) {
            throw;
        } catch (...) {
            reader.checkpoint();
            ++result.rejected;
        }
        return true;
    };
    bool budget = true;
    for (const auto &directory : downloads) {
        if (result.entries.size() >= 128) {
            result.limited = true;
            break;
        }
        budget = attempt([&] {
            detail::safe(directory);
            if (!fs::is_directory(directory) || !fs::exists(directory / "result.json"))
                return;
            const auto prepared = detail::stagedMetadata(result.installation, directory,
                                                         reader.read(directory / "prepared.json"));
            const auto row = detail::outcomeMetadata(prepared, reader.read(directory / "result.json"));
            result.entries.push_back({HistoryKind::installation,
                                      row.version,
                                      row.state,
                                      row.failure,
                                      {},
                                      row.directory,
                                      {},
                                      row.time,
                                      std::nullopt,
                                      false});
        });
        if (!budget)
            break;
    }
    if (budget)
        for (const auto &transaction : transactions) {
            if (result.entries.size() >= 128 || works >= 128) {
                result.limited = true;
                break;
            }
            budget = attempt([&] {
                detail::safe(transaction);
                if (!fs::is_directory(transaction))
                    return;
                for (const auto &entry : fs::directory_iterator(transaction)) {
                    reader.checkpoint();
                    if (++works > 128 || result.entries.size() >= 128) {
                        result.limited = true;
                        break;
                    }
                    ++result.visited;
                    if (!detail::workName(pathUtf8(entry.path().filename()), "recovery-"))
                        continue;
                    if (!attempt([&] {
                            detail::safe(entry.path());
                            if (entry.is_directory() && fs::exists(entry.path() / "recovery-result.json"))
                                result.entries.push_back(recovery(reader, transaction, entry.path()));
                        }))
                        throw Budget{};
                }
            });
            if (!budget)
                break;
        }
    std::sort(result.entries.begin(), result.entries.end(), [](const auto &a, const auto &b) {
        return a.time != b.time ? a.time > b.time : a.directory < b.directory;
    });
    reader.checkpoint();
    return result;
}
std::string historyReport(const History &history, std::optional<size_t> selected) {
    if (selected && *selected >= history.entries.size())
        throw std::runtime_error("Update history selection is out of bounds");
    Json rows = Json::array();
    auto add = [&](const HistoryEntry &row) {
        Json j{{"kind", row.kind == HistoryKind::recovery ? "recovery" : "installation"},
               {"version", row.version},
               {"state", row.state},
               {"failure", row.failure},
               {"directory", pathUtf8(row.directory.lexically_relative(history.installation))},
               {"time", row.time ? Json(*row.time) : Json(nullptr)}};
        if (row.recovery) {
            const auto &s = *row.recovery;
            j.update({{"transaction", pathUtf8(row.transaction.lexically_relative(history.installation))},
                      {"planSha256", row.planDigest},
                      {"phase", recoveryPhaseCode(s.phase)},
                      {"prepared", s.prepared},
                      {"prepareTotal", s.prepareTotal},
                      {"completed", s.completed},
                      {"total", s.total},
                      {"copiedBytes", s.copiedBytes},
                      {"installationChanged", s.installationChanged},
                      {"pendingCleared", row.pendingCleared}});
        }
        rows.push_back(std::move(j));
    };
    if (selected)
        add(history.entries[*selected]);
    else
        for (const auto &entry : history.entries)
            add(entry);
    Json report{{"format", 1},
                {"observationOnly", true},
                {"installation", pathUtf8(history.installation)},
                {"entries", rows},
                {"rejected", history.rejected},
                {"limited", history.limited},
                {"visited", history.visited},
                {"metadataBytes", history.metadataBytes}};
    auto bytes = report.dump(2) + "\n";
    if (bytes.size() > 256 * 1024)
        throw std::runtime_error("Update history report exceeds its size limit");
    return bytes;
}
} // namespace cycode::updates

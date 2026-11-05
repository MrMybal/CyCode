#pragma once
#include "cycode/UpdateRecoveryProgress.h"
#include "cycode/Updates.h"

namespace cycode::updates {
enum class HistoryKind { installation, recovery };
struct HistoryEntry {
    HistoryKind kind = HistoryKind::installation;
    std::string version, state, failure, planDigest;
    std::filesystem::path directory, transaction;
    std::optional<int64_t> time; // Informational Unix seconds; old receipts may lack it.
    std::optional<RecoverySnapshot> recovery;
    bool pendingCleared = false;
};
struct History {
    std::filesystem::path installation;
    std::vector<HistoryEntry> entries;
    uint64_t metadataBytes = 0;
    uint32_t rejected = 0, visited = 0;
    bool limited = false;
};
// Explicit read-only metadata inspection. No payload hashes, recovery action or network.
History inspectHistory(const std::filesystem::path &, const std::atomic<bool> &);
// A cached observation only; raw errors, metadata extras and environment are excluded.
std::string historyReport(const History &, std::optional<size_t> selected = std::nullopt);
} // namespace cycode::updates

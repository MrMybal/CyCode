#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cycode::updates {
enum class RecoveryPhase {
    inspecting,
    preparing,
    verifying,
    restoring,
    checking,
    complete,
    cancelled,
    failed
};
constexpr std::string_view recoveryPhaseCode(RecoveryPhase phase) noexcept {
    switch (phase) {
    case RecoveryPhase::inspecting:
        return "inspecting";
    case RecoveryPhase::preparing:
        return "preparing";
    case RecoveryPhase::verifying:
        return "verifying";
    case RecoveryPhase::restoring:
        return "restoring";
    case RecoveryPhase::checking:
        return "checking";
    case RecoveryPhase::complete:
        return "complete";
    case RecoveryPhase::cancelled:
        return "cancelled";
    default:
        return "failed";
    }
}
struct RecoverySnapshot {
    RecoveryPhase phase = RecoveryPhase::inspecting;
    uint32_t prepared = 0, prepareTotal = 0, completed = 0, total = 0;
    uint64_t copiedBytes = 0;
    bool installationChanged = false;
    // Phase-local file completion only. Hash/inspection/postflight work has no
    // fabricated percentage; even all restored entries still need postflight.
    constexpr std::optional<unsigned> percent() const noexcept {
        if (phase == RecoveryPhase::complete)
            return 100;
        if (phase == RecoveryPhase::preparing && prepareTotal && prepareTotal <= 4096 &&
            prepared <= prepareTotal)
            return unsigned(uint64_t(prepared) * 100 / prepareTotal);
        if (phase == RecoveryPhase::restoring && total && total <= 8192 && completed <= total)
            return unsigned(uint64_t(completed) * 100 / total);
        return std::nullopt;
    }
};
struct RecoveryProgress {
    RecoveryProgress(std::string plan, std::string directory)
        : planDigest(std::move(plan)), transaction(std::move(directory)) {}
    const std::string planDigest, transaction;
    std::atomic<bool> claimed{false}, installationChanged{false};
    std::atomic<RecoveryPhase> phase{RecoveryPhase::inspecting};
    std::atomic<uint32_t> prepared{0}, prepareTotal{0}, completed{0}, total{0};
    std::atomic<uint64_t> copiedBytes{0};
    RecoverySnapshot snapshot() const noexcept {
        return {phase.load(), prepared.load(),    prepareTotal.load(),       completed.load(),
                total.load(), copiedBytes.load(), installationChanged.load()};
    }
};
} // namespace cycode::updates

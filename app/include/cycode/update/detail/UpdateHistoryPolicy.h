#pragma once
#include "cycode/UpdateHistory.h"

namespace cycode::updates::detail {
constexpr bool historyAttention(HistoryKind kind, std::string_view state) noexcept {
    return kind == HistoryKind::recovery
               ? state == "cancelled" || state == "failed"
               : state == "waiting" || state == "failed" || state == "restart-failed";
}
constexpr std::optional<RecoveryPhase> historyPhase(std::string_view code) noexcept {
    for (const auto phase : {RecoveryPhase::inspecting, RecoveryPhase::preparing, RecoveryPhase::verifying,
                             RecoveryPhase::restoring, RecoveryPhase::checking, RecoveryPhase::complete,
                             RecoveryPhase::cancelled, RecoveryPhase::failed})
        if (recoveryPhaseCode(phase) == code)
            return phase;
    return std::nullopt;
}
constexpr bool historyRecoveryValid(std::string_view state, const RecoverySnapshot &s,
                                    bool cleared) noexcept {
    if (s.prepareTotal > 4096 || s.total > 8192 || s.prepared > s.prepareTotal || s.completed > s.total ||
        s.prepareTotal > s.total || s.copiedBytes > 258ull * 1024 * 1024 ||
        (s.completed && !s.installationChanged))
        return false;
    if (state == "restored")
        return cleared && s.phase == RecoveryPhase::complete && s.prepared == s.prepareTotal &&
               s.completed == s.total;
    return (state == "cancelled" || state == "failed") && !cleared && s.phase != RecoveryPhase::complete;
}
} // namespace cycode::updates::detail

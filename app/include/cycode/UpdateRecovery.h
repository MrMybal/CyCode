#pragma once
#include "cycode/Updates.h"

namespace cycode::updates {
struct RecoveryProgress;
struct RecoverySnapshot;
struct RecoverySpace {
    // Observations only. A new native preflight owns authorization and capacity.
    bool known = false;
    uint64_t allocationUnit = 0, payloadBytes = 0, allocatedCopies = 0;
    uint64_t copyFiles = 0, retainedFiles = 0, directoryAllowances = 0;
    uint64_t requiredBytes = 0, availableBytes = 0;
    uint64_t reusedFiles = 0, reusedBytes = 0, reusedAllocation = 0;
    bool preparedIgnored = false, preparedLimited = false;
    std::string error;
};
struct Recovery {
    std::filesystem::path installation, transaction;
    std::string previousVersion, version, planDigest, baseline;
    std::string previousHelper, nextHelper;
    uint32_t restore = 0, remove = 0, unchanged = 0;
    std::vector<std::string> conflicts; // Bounded details; conflictCount remains complete.
    uint32_t conflictCount = 0;
    RecoverySpace space;
};
// Read-only preview; accepts only the active, format-2 installation transaction.
Recovery reviewRecovery(const std::filesystem::path &, const std::atomic<bool> &);
// Acquires exclusive installation ownership and repeats the reviewed preflight.
// Retains original backups and the pending guard until full baseline postflight.
void recover(const Recovery &, const std::atomic<bool> &, RecoveryProgress *progress = nullptr);
// Bounded context/numeric report; excludes exception messages and environment.
std::string recoveryReport(const Recovery &, const RecoverySnapshot &);
} // namespace cycode::updates

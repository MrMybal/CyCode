#pragma once
#include "cycode/Updates.h"

namespace cycode::updates::detail {
// Download/staging estimate only; extraction, filesystem allocation and concurrent
// writers can require more. Keep 32 MiB spare plus catalogue/journal allowances.
inline constexpr uint64_t downloadReserve = 32ull * 1024 * 1024 + 4ull * 1024 * 1024 + 65536;
constexpr std::optional<uint64_t> downloadHeadroom(uint64_t total, uint64_t received,
                                                   uint64_t helperSize) noexcept {
    if (!total || total > maximumPackage || received > total || !helperSize ||
        helperSize > 64ull * 1024 * 1024)
        return std::nullopt;
    return total - received + helperSize + downloadReserve;
}
inline constexpr uint64_t installMetadataReserve = 32ull * 1024 * 1024;
inline constexpr uint64_t installSpare = 32ull * 1024 * 1024;
constexpr bool allocationUnitValid(uint64_t unit) noexcept {
    return unit >= 512 && unit <= 16ull * 1024 * 1024 && (unit & (unit - 1)) == 0;
}
constexpr std::optional<uint64_t> allocatedBytes(uint64_t size, uint64_t unit) noexcept {
    if (!allocationUnitValid(unit) || size > maximumPackage + 2ull * 1024 * 1024)
        return std::nullopt;
    return (size / unit + (size % unit != 0 ? 1 : 0)) * unit;
}
constexpr std::optional<uint64_t> installationHeadroom(uint64_t allocatedPayload, uint64_t entries,
                                                       uint64_t directoryAllowances, uint64_t unit) noexcept {
    if (!allocationUnitValid(unit) || entries == 0 || entries > 4096 || directoryAllowances > 5000000 ||
        allocatedPayload > maximumPackage + entries * unit)
        return std::nullopt;
    // File metadata, directory growth and atomic journal temporaries are charged
    // conservatively. Existing originals/archive already occupy their disk space.
    return allocatedPayload + (entries + directoryAllowances + 64) * unit + installMetadataReserve +
           installSpare;
}
constexpr std::optional<uint64_t> recoveryHeadroom(uint64_t allocatedCopies, uint64_t copyFiles,
                                                   uint64_t retainedFiles, uint64_t directoryAllowances,
                                                   uint64_t unit) noexcept {
    // Two bounded manifest sets: originals are copied; replacements are renamed
    // into retention. A zero-action recovery still needs metadata/cleanup space.
    if (!allocationUnitValid(unit) || copyFiles > 4096 || retainedFiles > 4096 ||
        directoryAllowances > 5000000 || (!copyFiles && allocatedCopies) ||
        allocatedCopies > maximumPackage + 2ull * 1024 * 1024 + copyFiles * unit)
        return std::nullopt;
    return allocatedCopies + (copyFiles + retainedFiles + directoryAllowances + 64) * unit +
           installMetadataReserve + installSpare;
}
constexpr std::optional<uint64_t> recoveryResumeHeadroom(uint64_t allocatedCopies, uint64_t reusedAllocation,
                                                         uint64_t copyFiles, uint64_t reusedFiles,
                                                         uint64_t retainedFiles, uint64_t directoryAllowances,
                                                         uint64_t unit) noexcept {
    const auto full = recoveryHeadroom(allocatedCopies, copyFiles, retainedFiles, directoryAllowances, unit);
    if (!full || reusedFiles > copyFiles || reusedAllocation > allocatedCopies ||
        (!reusedFiles && reusedAllocation))
        return std::nullopt;
    // Only verified, already-existing payload allocation is deducted. Metadata,
    // directory and spare allowances still cover the complete remaining action.
    return *full - reusedAllocation;
}
} // namespace cycode::updates::detail

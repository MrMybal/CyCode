#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace cycode {
enum class AtomicPublication { CreateExclusive, ReplacePreservingMetadata, Replace };
enum class AtomicAccess { Inherited, PrivateUser };
struct AtomicFileOptions {
    AtomicPublication publication = AtomicPublication::CreateExclusive;
    AtomicAccess access = AtomicAccess::Inherited;
    unsigned replacementAttempts = 1;
};
// One exclusive sibling temporary, durable byte writes, pre-publication
// validation and atomic publication. The callback runs before creation and
// again immediately before publication; it is not an OS compare-and-swap.
// PrivateUser supplies and verifies restricted permissions before writing.
// Callers retain path, size, revision, cancellation and transaction policies.
void writeFileAtomic(const std::filesystem::path &, std::string_view,
                     const AtomicFileOptions & = {}, const std::function<void()> &validate = {});
// Generated CMake/MSBuild metadata, bounded to 32 MiB, with unchanged-content
// short circuit and a disk baseline. A conflict never silently overwrites data.
void writeMetadataFileAtomic(const std::filesystem::path &, const std::string &,
                             unsigned replacementAttempts = 1);
} // namespace cycode

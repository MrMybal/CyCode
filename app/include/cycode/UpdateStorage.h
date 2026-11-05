#pragma once
#include "cycode/Updates.h"

namespace cycode::updates {
struct StoredFile {
    std::string name, digest;
    uint64_t size = 0;
};
struct CachedDownload {
    std::filesystem::path directory;
    std::string version, metadataDigest, baseline;
    std::vector<StoredFile> files;
    uint64_t bytes = 0;
};
struct RetiredDownload {
    std::filesystem::path directory, source;
    std::string version, baseline;
    uint64_t bytes = 0;
    uint32_t restore = 0, unchanged = 0, conflicts = 0;
    std::vector<StoredFile> files;
};
struct StorageReview {
    std::filesystem::path installation;
    std::string manifestDigest;
    std::vector<CachedDownload> downloads;
    std::vector<RetiredDownload> retired;
    uint32_t protectedEntries = 0, rejected = 0;
    bool limited = false;
};
struct StorageResult {
    std::filesystem::path directory;
    uint64_t bytes = 0;
    uint32_t files = 0;
    bool complete = false, cancelled = false;
    std::string error;
};
// Explicit inspection only. Never runs at startup or deletes any file.
StorageReview reviewStorage(const std::filesystem::path &, const std::atomic<bool> &);
// Both actions acquire installation/cache leases and repeat the selected review.
// Payloads move without overwrite; metadata, history and recovery backups remain.
StorageResult retireDownload(const StorageReview &, size_t index, const std::atomic<bool> &);
StorageResult restoreDownload(const StorageReview &, size_t index, const std::atomic<bool> &);
} // namespace cycode::updates

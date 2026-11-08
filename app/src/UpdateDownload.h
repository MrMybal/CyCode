#pragma once
#include "cycode/update/detail/UpdateFiles.h"
#include "UpdateRange.h"
#include "cycode/UpdateProgress.h"

namespace cycode::updates::detail {
void downloadBinding(const Release &);
bool sameDownload(const PartialDownload &, const Release &);
std::optional<PartialDownload> selectPartial(const std::filesystem::path &root, const Release &, DownloadMode,
                                             const std::atomic<bool> &cancel);
PartialDownload partialMetadata(const std::filesystem::path &root, const std::filesystem::path &directory);
void createPartialMetadata(const std::filesystem::path &root, const std::filesystem::path &directory,
                           const Release &);
void requireDownloadSpace(const std::filesystem::path &directory, uint64_t total, uint64_t received,
                          uint64_t helperSize, const std::shared_ptr<DownloadStatus> &);
// False means that a valid full response ignored Range. No body has been consumed.
bool downloadResponse(unsigned status, std::string_view range, std::string_view encoding,
                      std::string_view length, uint64_t offset, uint64_t size);
} // namespace cycode::updates::detail

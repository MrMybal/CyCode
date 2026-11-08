#pragma once
#include "cycode/Updates.h"
#include <string_view>
namespace cycode::updates::detail {
class Cancellation : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
void safe(const std::filesystem::path &);
std::string read(const std::filesystem::path &, uint64_t limit);
std::filesystem::path work(const std::filesystem::path &installation, const char *prefix);
bool digest(std::string_view);
bool workName(std::string_view name, std::string_view prefix);
Prepared stagedMetadata(const std::filesystem::path &root, const std::filesystem::path &directory);
Prepared stagedMetadata(const std::filesystem::path &root, const std::filesystem::path &directory,
                        const nlohmann::json &);
Attempt outcomeMetadata(const Prepared &, const nlohmann::json &);
bool recordedOutcome(std::string_view);
std::unique_ptr<NativeFileLease> cacheLease(const std::filesystem::path &, NativeFileLeaseMode);
void moveFile(const std::filesystem::path &source, const std::filesystem::path &destination);
std::string hashFile(const std::filesystem::path &, uint64_t maximum, const std::atomic<bool> *cancel);
void jsonFile(const std::filesystem::path &, const nlohmann::json &);
uint64_t availableSpace(const std::filesystem::path &directory);
uint64_t allocationUnit(const std::filesystem::path &directory);
void removeOwnedText(const std::filesystem::path &, const std::string &baseline);
} // namespace cycode::updates::detail

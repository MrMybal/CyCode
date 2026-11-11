#pragma once
#include "cycode/AtomicFile.h"
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace cycode {
class SpoolStore;
struct SpoolBlob {
    std::shared_ptr<SpoolStore> owner;
    std::filesystem::path file;
    uint64_t bytes = 0;
    std::string digest;
    std::filesystem::file_time_type stamp;
    void validate() const;
    std::string read(uint64_t offset, size_t count) const;
    std::string load() const;
    void stream(const AtomicByteSink &) const;
};
using StoredBlob = std::shared_ptr<const SpoolBlob>;
// Private, process-owned immutable payloads. Paths are never exposed to clients.
// One shared owner retains data for live buffers and their transaction history.
class SpoolStore : public std::enable_shared_from_this<SpoolStore> {
  public:
    explicit SpoolStore(const std::filesystem::path &workspace);
    ~SpoolStore();
    StoredBlob put(std::string_view);
    StoredBlob importFile(const std::filesystem::path &);

  private:
    StoredBlob create(uint64_t, const AtomicByteProducer &);
    std::filesystem::path directory_;
    size_t sequence_ = 0;
    struct Owned {
        std::filesystem::path file;
        uint64_t bytes;
        std::filesystem::file_time_type stamp;
    };
    std::vector<Owned> owned_;
    std::map<std::string, std::weak_ptr<const SpoolBlob>> cached_;
};
} // namespace cycode

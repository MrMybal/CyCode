#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
namespace cycode {
class RecoveryStore {
  public:
    using Json = nlohmann::json;
    struct Snapshot {
        std::string path, text, disk;
        uint64_t revision = 0;
        bool bom = false, crlf = false, crOnly = false, existed = true;
    };
    struct Entry {
        Snapshot snapshot;
        Json info;
    };
    static constexpr size_t maxDocuments = 128, maxBytes = 128 * 1024 * 1024;
    explicit RecoveryStore(std::filesystem::path);
    ~RecoveryStore();
    void checkpoint(const Snapshot &);
    void erase(const std::string &path);
    void retain(const std::set<std::string> &);
    uint64_t savedRevision(const std::string &) const;
    Json status() const;
    Json list(size_t limit = 256) const;
    Json inspect(const std::string &session, const std::string &id, const std::string &checkpoint,
                 const std::function<Json(const Entry &)> &, bool consume = false);
    Json discard(const std::string &session, const std::string &id, const std::string &checkpoint);
    static std::string fingerprint(const std::string &);

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace cycode

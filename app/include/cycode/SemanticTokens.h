#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace cycode {
// Worker-owned LSP integer-array snapshots; independent from documents and UI.
class SemanticTokens {
  public:
    struct Data {
        std::optional<std::string> resultId;
        std::vector<uint32_t> words;
        nlohmann::json result() const;
    };
    using Snapshot = std::shared_ptr<const Data>;
    static constexpr size_t maxWords = 1000000, maxEdits = 4096;
    static constexpr size_t maxEntries = 16, maxCachedWords = 4000000;
    // Delta offsets refer to the same previous array, and need not be sorted or
    // aligned to five-integer token boundaries. A delta may also return full data.
    static Snapshot decode(const nlohmann::json &, const Snapshot &base = {});
    Snapshot find(const std::string &uri);
    void remember(const std::string &uri, Snapshot);
    void erase(const std::string &uri);
    void clear();
    size_t size() const {
        return entries_.size();
    }
    size_t words() const {
        return words_;
    }

  private:
    struct Entry {
        Snapshot data;
        uint64_t used;
    };
    std::map<std::string, Entry> entries_;
    size_t words_ = 0;
    uint64_t clock_ = 0;
};
} // namespace cycode

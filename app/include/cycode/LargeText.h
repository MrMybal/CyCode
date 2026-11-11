#pragma once
#include "cycode/SpoolStore.h"
namespace cycode {
struct TextPiece {
    StoredBlob blob;
    uint64_t offset = 0, bytes = 0;
};
struct LargeTextState {
    std::vector<TextPiece> pieces;
    uint64_t bytes = 0;
};
using LargeTextSnapshot = std::shared_ptr<const LargeTextState>;
// The Engine owns revisions and transactions. This is its native piece storage,
// not another editor/service or a set of writes to the original source.
class LargeText {
  public:
    LargeText(const std::filesystem::path &, std::shared_ptr<SpoolStore>);
    LargeTextSnapshot snapshot() const { return state_; }
    void commit(LargeTextSnapshot state) noexcept { state_.swap(state); }
    uint64_t bytes() const { return state_->bytes; }
    bool dirty() const { return state_ != saved_; }
    bool matchesDisk() const;
    std::string read(uint64_t, size_t, LargeTextSnapshot = {}) const;
    LargeTextSnapshot edited(uint64_t, uint64_t, const std::string &);
    void validate(LargeTextSnapshot) const;
    void save();

  private:
    std::string raw(uint64_t, size_t, LargeTextSnapshot) const;
    bool boundary(uint64_t, LargeTextSnapshot) const;
    std::filesystem::path source_;
    std::shared_ptr<SpoolStore> store_;
    LargeTextSnapshot state_, saved_;
    std::string savedDigest_;
};
} // namespace cycode

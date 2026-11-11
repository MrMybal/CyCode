#pragma once
#include "cycode/paged/SpoolStore.h"
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
    const std::string &baselineDigest() const { return savedDigest_; }
    void stream(const AtomicByteSink &, LargeTextSnapshot = {}) const;
    LargeTextSnapshot recovered(const std::filesystem::path &, const std::string &digest) const;
    void save();
    void saveAs(std::filesystem::path);
    struct Lines {
        std::string text;
        uint64_t offset = 0, nextOffset = 0, nextLine = 0, nextCharacter = 0, totalLines = 0;
    };
    Lines readLines(uint64_t line, uint64_t character, size_t length) const;

  private:
    std::string raw(uint64_t, size_t, LargeTextSnapshot) const;
    bool boundary(uint64_t, LargeTextSnapshot) const;
    std::filesystem::path source_;
    std::shared_ptr<SpoolStore> store_;
    LargeTextSnapshot state_, saved_;
    std::string savedDigest_;
    mutable LargeTextSnapshot indexed_;
    mutable std::vector<uint64_t> lineAnchors_;
    mutable uint64_t lineCount_ = 0;
    void indexLines() const;
    uint64_t lineOffset(uint64_t, uint64_t) const;
    std::string publish(const std::filesystem::path &, const AtomicFileOptions &) const;
};
} // namespace cycode

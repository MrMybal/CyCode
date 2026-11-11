#include "cycode/LargeText.h"
#include "cycode/Error.h"
#include "cycode/StreamingData.h"
#include <algorithm>
#include <array>
#include <limits>
#include <monocypher.h>
#include <set>
namespace cycode {
namespace {
class Utf8Check {
    unsigned remaining = 0;
    uint32_t value = 0, minimum = 0;

  public:
    void feed(std::string_view text) {
        for (unsigned char c : text) {
            if (!remaining) {
                if (c == 0)
                    throw NativeError(ErrorCode::InvalidParams,
                                      "Binary sources cannot be opened as large text");
                if (c < 0x80)
                    continue;
                if (c >= 0xc2 && c <= 0xdf) {
                    remaining = 1;
                    value = c & 31;
                    minimum = 0x80;
                } else if (c >= 0xe0 && c <= 0xef) {
                    remaining = 2;
                    value = c & 15;
                    minimum = 0x800;
                } else if (c >= 0xf0 && c <= 0xf4) {
                    remaining = 3;
                    value = c & 7;
                    minimum = 0x10000;
                } else
                    throw NativeError(ErrorCode::InvalidParams, "Large text must contain valid UTF-8");
            } else {
                if ((c & 0xc0) != 0x80)
                    throw NativeError(ErrorCode::InvalidParams, "Invalid UTF-8 continuation");
                value = (value << 6) | (c & 63);
                if (!--remaining &&
                    (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)))
                    throw NativeError(ErrorCode::InvalidParams, "Invalid UTF-8 code point");
            }
        }
    }
    void finish() {
        if (remaining)
            throw NativeError(ErrorCode::InvalidParams, "Truncated UTF-8 sequence");
    }
};
} // namespace
LargeText::LargeText(const std::filesystem::path &source, std::shared_ptr<SpoolStore> store)
    : source_(source), store_(std::move(store)) {
    const auto blob = store_->importFile(source);
    Utf8Check validator;
    blob->stream([&](std::string_view bytes) { validator.feed(bytes); });
    validator.finish();
    auto initial = std::make_shared<LargeTextState>();
    initial->bytes = blob->bytes;
    if (blob->bytes)
        initial->pieces.push_back({blob, 0, blob->bytes});
    state_ = saved_ = initial;
    savedDigest_ = blob->digest;
    if (!matchesDisk())
        throw NativeError(ErrorCode::DiskConflict, "Source changed while opening large text");
}
bool LargeText::matchesDisk() const { return digestFileStreaming(source_) == savedDigest_; }
std::string LargeText::raw(uint64_t offset, size_t count, LargeTextSnapshot state) const {
    if (offset > state->bytes || count > state->bytes - offset)
        throw NativeError(ErrorCode::InvalidParams, "Large text range exceeds its extent");
    std::string result;
    result.reserve(count);
    uint64_t position = 0;
    for (const auto &piece : state->pieces) {
        if (offset >= position + piece.bytes) {
            position += piece.bytes;
            continue;
        }
        const auto local = offset - position;
        const auto take = std::min<uint64_t>(piece.bytes - local, count - result.size());
        result += piece.blob->read(piece.offset + local, size_t(take));
        offset += take;
        position += piece.bytes;
        if (result.size() == count)
            break;
    }
    if (result.size() != count)
        throw NativeError(ErrorCode::FileSystemError, "Incomplete large text piece range");
    return result;
}
bool LargeText::boundary(uint64_t offset, LargeTextSnapshot state) const {
    if (offset > state->bytes)
        return false;
    return offset == state->bytes || (static_cast<unsigned char>(raw(offset, 1, state)[0]) & 0xc0) != 0x80;
}
std::string LargeText::read(uint64_t offset, size_t count, LargeTextSnapshot state) const {
    if (!state)
        state = state_;
    if (count < 4 || count > 256 * 1024 || !boundary(offset, state))
        throw NativeError(ErrorCode::InvalidParams, "Use a UTF-8 byte boundary and a 4..262144 byte page");
    auto end = offset + std::min<uint64_t>(count, state->bytes - offset);
    while (end > offset && !boundary(end, state))
        --end;
    return raw(offset, size_t(end - offset), state);
}
LargeTextSnapshot LargeText::edited(uint64_t offset, uint64_t removed, const std::string &text) {
    if (offset > state_->bytes || removed > state_->bytes - offset || !boundary(offset, state_) ||
        !boundary(offset + removed, state_))
        throw NativeError(ErrorCode::InvalidParams, "Large text edit needs valid UTF-8 byte boundaries");
    if (text.size() > 1024 * 1024)
        throw NativeError(ErrorCode::CapacityExceeded,
                          "An inserted text chunk exceeds the 1 MiB operation budget");
    Utf8Check check;
    check.feed(text);
    check.finish();
    if (removed == text.size() && removed <= 256 * 1024 && raw(offset, size_t(removed), state_) == text)
        return state_;
    if (text.size() > uint64_t(std::numeric_limits<std::streamoff>::max()) - (state_->bytes - removed))
        throw NativeError(ErrorCode::CapacityExceeded, "Edited text exceeds native stream offsets");
    auto result = std::make_shared<LargeTextState>();
    result->bytes = state_->bytes - removed + text.size();
    auto append = [&](TextPiece piece) {
        if (!piece.bytes)
            return;
        if (!result->pieces.empty()) {
            auto &last = result->pieces.back();
            if (last.blob == piece.blob && last.offset + last.bytes == piece.offset) {
                last.bytes += piece.bytes;
                return;
            }
        }
        result->pieces.push_back(std::move(piece));
    };
    auto retain = [&](uint64_t start, uint64_t length) {
        uint64_t position = 0;
        for (const auto &piece : state_->pieces) {
            if (start >= position + piece.bytes) {
                position += piece.bytes;
                continue;
            }
            const auto local = start - position, take = std::min(piece.bytes - local, length);
            append({piece.blob, piece.offset + local, take});
            start += take;
            length -= take;
            position += piece.bytes;
            if (!length)
                break;
        }
        if (length)
            throw NativeError(ErrorCode::FileSystemError, "Incomplete piece retention");
    };
    if (offset)
        retain(0, offset);
    if (!text.empty())
        append({store_->put(text), 0, text.size()});
    const auto tail = offset + removed;
    if (tail < state_->bytes)
        retain(tail, state_->bytes - tail);
    return result;
}
void LargeText::validate(LargeTextSnapshot state) const {
    std::set<const SpoolBlob *> seen;
    for (const auto &piece : state->pieces)
        if (seen.insert(piece.blob.get()).second) {
            piece.blob->validate();
            if (digestFileStreaming(piece.blob->file) != piece.blob->digest)
                throw NativeError(ErrorCode::RevisionConflict, "Immutable large-text payload changed");
        }
}
void LargeText::stream(const AtomicByteSink &sink, LargeTextSnapshot state) const {
    if (!state)
        state = state_;
    validate(state);
    for (const auto &piece : state->pieces)
        for (uint64_t at = 0; at < piece.bytes;) {
            const auto count = std::min<uint64_t>(65536, piece.bytes-at);
            sink(piece.blob->read(piece.offset+at, size_t(count)));
            at += count;
        }
}
LargeTextSnapshot LargeText::recovered(const std::filesystem::path &file, const std::string &digest) const {
    // Import/validate into this document's private store, retaining its original disk baseline.
    LargeText imported(file, store_);
    if (imported.baselineDigest() != digest)
        throw NativeError(ErrorCode::RevisionConflict, "Recovery payload changed while importing");
    return imported.snapshot();
}
void LargeText::save() {
    if (!matchesDisk())
        throw NativeError(ErrorCode::DiskConflict, "Large source changed outside CyCode");
    if (!dirty())
        return;
    using P = std::filesystem::perms;
    if ((std::filesystem::status(source_).permissions() &
         (P::owner_write | P::group_write | P::others_write)) == P::none)
        throw NativeError(ErrorCode::ReadOnly, "Large source is read-only");
    validate(state_);
    AtomicFileOptions options;
    options.publication = AtomicPublication::ReplacePreservingMetadata;
    auto digest = publish(source_, options);
    saved_ = state_;
    savedDigest_.swap(digest);
}
void LargeText::saveAs(std::filesystem::path destination) {
    if (!matchesDisk())
        throw NativeError(ErrorCode::DiskConflict, "Large source changed before Save as");
    if (!std::filesystem::is_directory(destination.parent_path()))
        throw NativeError(ErrorCode::InvalidParams, "Save as requires an existing destination folder");
    validate(state_);
    // Allocate the new path before publication; the final rebind cannot allocate.
    auto digest = publish(destination, AtomicFileOptions{});
    source_.swap(destination);
    saved_ = state_;
    savedDigest_.swap(digest);
}
std::string LargeText::publish(const std::filesystem::path &destination,
                               const AtomicFileOptions &options) const {
    const auto prepared = state_;
    crypto_blake2b_ctx digest;
    crypto_blake2b_init(&digest, 32);
    std::string publishedDigest(64, '0');
    writeFileAtomicStream(
        destination,
        [&](const AtomicByteSink &sink) {
            for (const auto &piece : prepared->pieces)
                for (uint64_t at = 0; at < piece.bytes;) {
                    const auto count = std::min<uint64_t>(65536, piece.bytes - at);
                    const auto bytes = piece.blob->read(piece.offset + at, size_t(count));
                    sink(bytes);
                    crypto_blake2b_update(&digest, reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
                    at += count;
                }
            std::array<uint8_t, 32> hash;
            crypto_blake2b_final(&digest, hash.data());
            for (size_t i = 0; i < hash.size(); ++i) {
                publishedDigest[2*i] = "0123456789abcdef"[hash[i] >> 4];
                publishedDigest[2*i+1] = "0123456789abcdef"[hash[i] & 15];
            }
        }, options, [&] {
            if (!matchesDisk())
                throw NativeError(ErrorCode::DiskConflict, "Large source changed before publication");
        });
    return publishedDigest;
}
void LargeText::indexLines() const {
    if (indexed_ == state_)
        return;
    std::vector<uint64_t> anchors;
    const auto bom = state_->bytes >= 3 && raw(0, 3, state_) == "\xef\xbb\xbf" ? 3u : 0u;
    anchors.push_back(bom);
    uint64_t lines = 1;
    bool previousCr = false;
    for (uint64_t offset = bom; offset < state_->bytes;) {
        const auto text = raw(offset, size_t(std::min<uint64_t>(65536, state_->bytes-offset)), state_);
        for (size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (c == '\n' && previousCr) {
                if ((lines-1) % 1024 == 0)
                    anchors.back() = offset+i+1;
            } else if (c == '\r' || c == '\n') {
                if (lines == std::numeric_limits<uint64_t>::max())
                    throw NativeError(ErrorCode::CapacityExceeded, "Line count exceeds native offsets");
                ++lines;
                if ((lines-1) % 1024 == 0)
                    anchors.push_back(offset+i+1);
            }
            previousCr = c == '\r';
        }
        offset += text.size();
    }
    lineAnchors_.swap(anchors);
    lineCount_ = lines;
    indexed_ = state_;
}
uint64_t LargeText::lineOffset(uint64_t targetLine, uint64_t targetCharacter) const {
    indexLines();
    if (targetLine >= lineCount_)
        throw NativeError(ErrorCode::InvalidParams, "Line is outside the paged document");
    uint64_t line = targetLine/1024*1024, column = 0;
    uint64_t offset = lineAnchors_.at(size_t(targetLine/1024));
    while (true) {
        if (line == targetLine && column == targetCharacter)
            return offset;
        if (offset == state_->bytes)
            break;
        const auto text = read(offset, 65536);
        size_t at = 0;
        while (at < text.size()) {
            if (line == targetLine && column == targetCharacter)
                return offset+at;
            const auto c = static_cast<unsigned char>(text[at]);
            const size_t width = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
            if (c == '\r' || c == '\n') {
                if (line == targetLine)
                    throw NativeError(ErrorCode::InvalidParams, "Character is beyond the line end");
                const bool pair = c == '\r' && offset+at+1 < state_->bytes &&
                                  raw(offset+at+1, 1, state_) == "\n";
                at += pair ? 2 : 1;
                ++line;
                column = 0;
            } else {
                const uint64_t units = width == 4 ? 2 : 1;
                if (line == targetLine && targetCharacter > column && targetCharacter < column+units)
                    throw NativeError(ErrorCode::InvalidParams, "UTF-16 character splits a surrogate pair");
                column += units;
                at += width;
            }
        }
        offset += at;
    }
    throw NativeError(ErrorCode::InvalidParams, "Character is outside the paged document");
}
LargeText::Lines LargeText::readLines(uint64_t line, uint64_t character, size_t length) const {
    Lines result;
    result.offset = lineOffset(line, character);
    result.text = read(result.offset, length);
    auto end = result.offset+result.text.size();
    if (!result.text.empty() && result.text.back() == '\r' && end < state_->bytes &&
        raw(end, 1, state_) == "\n")
        result.text.pop_back(); // Never issue a continuation in the middle of CRLF.
    result.nextOffset = result.offset+result.text.size();
    result.nextLine = line;
    result.nextCharacter = character;
    result.totalLines = lineCount_;
    for (size_t at = 0; at < result.text.size();) {
        const auto c = static_cast<unsigned char>(result.text[at]);
        if (c == '\r' || c == '\n') {
            at += c == '\r' && at+1 < result.text.size() && result.text[at+1] == '\n' ? 2 : 1;
            ++result.nextLine;
            result.nextCharacter = 0;
        } else {
            const size_t width = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
            result.nextCharacter += width == 4 ? 2 : 1;
            at += width;
        }
    }
    return result;
}
} // namespace cycode

#define PCRE2_CODE_UNIT_WIDTH 8
#include "cycode/CodeIndex.h"
#include "cycode/AtomicFile.h"
#include "cycode/Error.h"
#include "cycode/NativeFileLease.h"
#include "cycode/Paths.h"
#include "cycode/Process.h"
#include "cycode/ProcessText.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <monocypher.h>
#include <pcre2.h>
#include <set>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cycode {
using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t indexFormat = 1;       // shard binary layout
constexpr uint32_t scannerRevision = 2;    // bump when SymbolScanner output changes (2: template-argument commas, elaborated parameter types)
constexpr uint64_t maximumIndexedFile = 64ull * 1024 * 1024;
constexpr size_t shardFiles = 8192;
constexpr uint64_t shardBytes = 64ull * 1024 * 1024;
constexpr size_t overlayCompaction = 2048;
constexpr uint32_t flagBinary = 1, flagOversize = 2, flagUnreadable = 4;

double milliseconds(Clock::time_point since) {
    return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}
char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
std::string folded(std::string_view text) {
    std::string result(text);
    for (auto &c : result) c = fold(c);
    return result;
}
bool hasNonAscii(std::string_view text) {
    return std::any_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
}
// Replaces invalid UTF-8 (Latin-1 sources are common in large C++ trees) with U+FFFD.
std::string utf8(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        size_t length = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        bool valid = length && i + length <= text.size();
        for (size_t k = 1; valid && k < length; ++k)
            valid = (static_cast<unsigned char>(text[i + k]) & 0xc0) == 0x80;
        if (valid && length == 2 && c < 0xc2) valid = false;
        if (valid && length == 3) {
            const auto d = static_cast<unsigned char>(text[i + 1]);
            if ((c == 0xe0 && d < 0xa0) || (c == 0xed && d >= 0xa0)) valid = false;
        }
        if (valid && length == 4) {
            const auto d = static_cast<unsigned char>(text[i + 1]);
            if ((c == 0xf0 && d < 0x90) || (c == 0xf4 && d >= 0x90) || c > 0xf4) valid = false;
        }
        if (!valid) {
            out += "\xEF\xBF\xBD";
            ++i;
            continue;
        }
        out.append(text.substr(i, length));
        i += length;
    }
    return out;
}
std::string hex(const uint8_t *data, size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < size; ++i) {
        result += digits[data[i] >> 4];
        result += digits[data[i] & 15];
    }
    return result;
}
std::string isoNow() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(seconds);
}

// ---------------------------------------------------------------- file stats
struct Stamp {
    uint64_t size = 0;
    int64_t time = 0;
    bool file = false, directory = false;
};
Stamp stampOf(const fs::path &path) {
    Stamp stamp;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return stamp;
    stamp.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    stamp.file = !stamp.directory && !(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    stamp.size = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    stamp.time = int64_t((uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
#else
    struct stat data {};
    if (::stat(path.c_str(), &data) != 0) return stamp;
    stamp.directory = S_ISDIR(data.st_mode);
    stamp.file = S_ISREG(data.st_mode);
    stamp.size = uint64_t(data.st_size);
#if defined(__APPLE__)
    stamp.time = int64_t(data.st_mtimespec.tv_sec) * 1000000000 + data.st_mtimespec.tv_nsec;
#else
    stamp.time = int64_t(data.st_mtim.tv_sec) * 1000000000 + data.st_mtim.tv_nsec;
#endif
#endif
    return stamp;
}
bool readFile(const fs::path &path, uint64_t size, std::string &out) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    out.resize(size_t(size));
    input.read(out.data(), std::streamsize(size));
    out.resize(size_t(input.gcount()));
    // Files may grow between stat and read; the stamp keeps the indexed size.
    return true;
}

// ------------------------------------------------------------- mapped shard
class MappedFile {
  public:
    explicit MappedFile(const fs::path &path) {
#ifdef _WIN32
        file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) return;
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file_, &length) || length.QuadPart <= 0) return;
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping_) return;
        data_ = static_cast<const uint8_t *>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
        if (data_) size_ = size_t(length.QuadPart);
#else
        descriptor_ = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (descriptor_ < 0) return;
        struct stat data {};
        if (fstat(descriptor_, &data) != 0 || data.st_size <= 0) return;
        void *view = mmap(nullptr, size_t(data.st_size), PROT_READ, MAP_SHARED, descriptor_, 0);
        if (view == MAP_FAILED) return;
        data_ = static_cast<const uint8_t *>(view);
        size_ = size_t(data.st_size);
#endif
    }
    ~MappedFile() {
#ifdef _WIN32
        if (data_) UnmapViewOfFile(data_);
        if (mapping_) CloseHandle(mapping_);
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
#else
        if (data_) munmap(const_cast<uint8_t *>(data_), size_);
        if (descriptor_ >= 0) ::close(descriptor_);
#endif
    }
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;
    const uint8_t *data() const { return data_; }
    size_t size() const { return size_; }

  private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE, mapping_ = nullptr;
#else
    int descriptor_ = -1;
#endif
};

#pragma pack(push, 1)
struct ShardHeader {
    char magic[4];
    uint32_t format, files, trigrams, symbols, reserved;
    uint64_t filesOffset, trigramsOffset, postingsOffset, symbolsOffset, orderOffset, stringsOffset, end;
};
struct FileRecord {
    uint32_t pathOffset, pathLength;
    uint64_t size;
    int64_t time;
    uint32_t flags, symbolBegin, symbolCount, lines;
};
struct TrigramRecord {
    uint32_t key, start;
};
struct SymbolRecord {
    uint32_t nameOffset, scopeOffset, detailOffset;
    uint16_t nameLength, scopeLength, detailLength;
    uint8_t kind, flags;
    uint32_t line, column, endLine;
};
#pragma pack(pop)
static_assert(sizeof(FileRecord) == 40 && sizeof(SymbolRecord) == 32 && sizeof(TrigramRecord) == 8);

// Indexed content of one file, before serialization or as an overlay entry.
struct FileData {
    std::string path;
    uint64_t size = 0;
    int64_t time = 0;
    uint32_t flags = 0, lines = 0;
    bool deleted = false;
    std::vector<uint32_t> trigrams; // sorted unique folded keys
    std::vector<ScannedSymbol> symbols;
};

struct TrigramScratch {
    std::vector<uint64_t> bits = std::vector<uint64_t>((1u << 24) / 64);
    std::vector<uint32_t> keys;
};
void indexContent(FileData &data, std::string_view text, TrigramScratch &scratch) {
    if (text.size() >= 3) {
        scratch.keys.clear();
        uint32_t key = (uint32_t(uint8_t(fold(text[0]))) << 8) | uint8_t(fold(text[1]));
        for (size_t i = 2; i < text.size(); ++i) {
            key = ((key << 8) | uint8_t(fold(text[i]))) & 0xffffff;
            // Matches are line-oriented: trigrams spanning a line break never constrain a query.
            const uint8_t a = uint8_t(key >> 16), b = uint8_t(key >> 8), c = uint8_t(key);
            if (a == '\n' || b == '\n' || c == '\n' || a == '\r' || b == '\r' || c == '\r') continue;
            auto &word = scratch.bits[key >> 6];
            const uint64_t bit = 1ull << (key & 63);
            if (!(word & bit)) {
                word |= bit;
                scratch.keys.push_back(key);
            }
        }
        for (const auto key : scratch.keys) scratch.bits[key >> 6] = 0;
        std::sort(scratch.keys.begin(), scratch.keys.end());
        data.trigrams.assign(scratch.keys.begin(), scratch.keys.end());
    }
    data.lines = uint32_t(std::count(text.begin(), text.end(), '\n') + (text.empty() || text.back() == '\n' ? 0 : 1));
    const auto language = sourceLanguageForPath(data.path);
    if (language != SourceLanguage::None) {
        data.symbols = scanSymbols(text, language);
        if (data.symbols.size() > 50000) data.symbols.resize(50000);
    }
}
FileData loadFileData(const fs::path &root, const std::string &path, const Stamp &stamp, TrigramScratch &scratch,
                      uint64_t &bytesRead) {
    FileData data;
    data.path = path;
    data.size = stamp.size;
    data.time = stamp.time;
    if (stamp.size > maximumIndexedFile) {
        data.flags |= flagOversize;
        return data;
    }
    std::string text;
    if (!readFile(root / utf8Path(path), stamp.size, text)) {
        data.flags |= flagUnreadable;
        return data;
    }
    bytesRead += text.size();
    if (std::memchr(text.data(), 0, std::min<size_t>(text.size(), 8192))) {
        data.flags |= flagBinary; // same default as ripgrep: binary files are not text-searched
        return data;
    }
    indexContent(data, text, scratch);
    return data;
}

void putVarint(std::string &out, uint32_t value) {
    while (value >= 0x80) {
        out += char(uint8_t(value) | 0x80);
        value >>= 7;
    }
    out += char(uint8_t(value));
}
template <class T> void append(std::string &out, const T &value) {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
}
std::string serializeShard(const std::vector<FileData> &files) {
    std::string strings;
    auto addString = [&](std::string_view value, size_t limit) {
        const auto offset = uint32_t(strings.size());
        strings.append(value.substr(0, limit));
        return std::pair<uint32_t, uint32_t>{offset, uint32_t(std::min(value.size(), limit))};
    };
    // Names and scopes repeat heavily (declaration + definition, every member of a type).
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> interned;
    auto internString = [&](std::string_view value, size_t limit) {
        const auto key = std::string(value.substr(0, limit));
        if (const auto found = interned.find(key); found != interned.end()) return found->second;
        const auto location = addString(key, limit);
        interned.emplace(key, location);
        return location;
    };
    std::vector<FileRecord> records;
    std::vector<SymbolRecord> symbols;
    std::vector<uint64_t> pairs; // key << 16 | local file id
    records.reserve(files.size());
    for (size_t id = 0; id < files.size(); ++id) {
        const auto &file = files[id];
        const auto [pathOffset, pathLength] = addString(file.path, 4096);
        FileRecord record{pathOffset, pathLength, file.size, file.time, file.flags, uint32_t(symbols.size()),
                          uint32_t(file.symbols.size()), file.lines};
        records.push_back(record);
        for (const auto key : file.trigrams) pairs.push_back((uint64_t(key) << 16) | id);
        for (const auto &symbol : file.symbols) {
            const auto [nameOffset, nameLength] = internString(symbol.name, 1024);
            const auto [scopeOffset, scopeLength] = internString(symbol.scope, 2048);
            const auto [detailOffset, detailLength] = addString(symbol.detail, 512);
            symbols.push_back({nameOffset, scopeOffset, detailOffset, uint16_t(nameLength), uint16_t(scopeLength),
                               uint16_t(detailLength), uint8_t(symbol.kind), uint8_t(symbol.declaration ? 1 : 0),
                               symbol.line, symbol.column, symbol.endLine});
        }
    }
    std::sort(pairs.begin(), pairs.end());
    std::vector<TrigramRecord> trigrams;
    std::string postings;
    uint32_t previousKey = UINT32_MAX;
    int64_t previousId = -1;
    for (const auto pair : pairs) {
        const auto key = uint32_t(pair >> 16);
        const auto id = int64_t(pair & 0xffff);
        if (key != previousKey) {
            trigrams.push_back({key, uint32_t(postings.size())});
            previousKey = key;
            previousId = -1;
        }
        putVarint(postings, uint32_t(id - previousId - 1));
        previousId = id;
    }
    std::vector<uint32_t> order(symbols.size());
    for (uint32_t i = 0; i < order.size(); ++i) order[i] = i;
    auto nameOf = [&](uint32_t i) { return std::string_view(strings).substr(symbols[i].nameOffset, symbols[i].nameLength); };
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        const auto x = nameOf(a), y = nameOf(b);
        const auto n = std::min(x.size(), y.size());
        for (size_t i = 0; i < n; ++i) {
            const auto p = fold(x[i]), q = fold(y[i]);
            if (p != q) return uint8_t(p) < uint8_t(q);
        }
        return x.size() < y.size();
    });
    ShardHeader header{};
    std::memcpy(header.magic, "CYX1", 4);
    header.format = indexFormat;
    header.files = uint32_t(records.size());
    header.trigrams = uint32_t(trigrams.size());
    header.symbols = uint32_t(symbols.size());
    std::string out;
    out.reserve(sizeof(header) + records.size() * sizeof(FileRecord) + trigrams.size() * 8 + postings.size() +
                symbols.size() * (sizeof(SymbolRecord) + 4) + strings.size());
    out.resize(sizeof(header));
    header.filesOffset = out.size();
    for (const auto &record : records) append(out, record);
    header.trigramsOffset = out.size();
    for (const auto &record : trigrams) append(out, record);
    header.postingsOffset = out.size();
    out += postings;
    header.symbolsOffset = out.size();
    for (const auto &record : symbols) append(out, record);
    header.orderOffset = out.size();
    for (const auto value : order) append(out, value);
    header.stringsOffset = out.size();
    out += strings;
    header.end = out.size();
    std::memcpy(out.data(), &header, sizeof(header));
    return out;
}

class Shard {
  public:
    static std::shared_ptr<const Shard> open(const fs::path &path) {
        auto shard = std::shared_ptr<Shard>(new Shard(path));
        if (!shard->valid()) return nullptr;
        return shard;
    }
    uint32_t files() const { return header_.files; }
    const FileRecord &file(uint32_t id) const { return files_[id]; }
    std::string_view string(uint32_t offset, uint32_t length) const {
        return {reinterpret_cast<const char *>(strings_) + offset, length};
    }
    std::string_view path(uint32_t id) const { return string(files_[id].pathOffset, files_[id].pathLength); }
    uint32_t symbolCount() const { return header_.symbols; }
    const SymbolRecord &symbol(uint32_t index) const { return symbols_[index]; }
    uint32_t orderAt(uint32_t index) const {
        uint32_t value;
        std::memcpy(&value, order_ + size_t(index) * 4, 4);
        return value;
    }
    uint32_t fileOfSymbol(uint32_t symbol) const {
        uint32_t low = 0, high = header_.files;
        while (low + 1 < high) {
            const auto mid = (low + high) / 2;
            if (files_[mid].symbolBegin <= symbol) low = mid;
            else high = mid;
        }
        return low;
    }
    // Sorted local ids for one trigram key (empty if absent).
    std::vector<uint16_t> postings(uint32_t key) const {
        std::vector<uint16_t> ids;
        const auto *begin = trigrams_, *end = trigrams_ + header_.trigrams;
        const auto *found = std::lower_bound(begin, end, key, [](const TrigramRecord &r, uint32_t k) { return r.key < k; });
        if (found == end || found->key != key) return ids;
        const uint64_t start = found->start;
        const uint64_t stop = found + 1 < end ? (found + 1)->start : header_.symbolsOffset - header_.postingsOffset;
        int64_t previous = -1;
        for (uint64_t at = start; at < stop;) {
            uint32_t value = 0;
            int shift = 0;
            while (at < stop) {
                const uint8_t byte = postings_[at++];
                value |= uint32_t(byte & 0x7f) << shift;
                shift += 7;
                if (!(byte & 0x80) || shift > 28) break;
            }
            previous += int64_t(value) + 1;
            if (previous < 0 || previous >= int64_t(header_.files)) break;
            ids.push_back(uint16_t(previous));
        }
        return ids;
    }
    size_t postingLength(uint32_t key) const {
        const auto *begin = trigrams_, *end = trigrams_ + header_.trigrams;
        const auto *found = std::lower_bound(begin, end, key, [](const TrigramRecord &r, uint32_t k) { return r.key < k; });
        if (found == end || found->key != key) return 0;
        const uint64_t stop = found + 1 < end ? (found + 1)->start : header_.symbolsOffset - header_.postingsOffset;
        return size_t(stop - found->start);
    }
    size_t bytes() const { return map_.size(); }

  private:
    explicit Shard(const fs::path &path) : map_(path) {}
    bool valid() {
        if (!map_.data() || map_.size() < sizeof(ShardHeader)) return false;
        std::memcpy(&header_, map_.data(), sizeof(header_));
        if (std::memcmp(header_.magic, "CYX1", 4) || header_.format != indexFormat || header_.end != map_.size() ||
            header_.files > 65536)
            return false;
        const uint64_t offsets[] = {sizeof(ShardHeader), header_.filesOffset, header_.trigramsOffset,
                                    header_.postingsOffset, header_.symbolsOffset, header_.orderOffset,
                                    header_.stringsOffset, header_.end};
        for (size_t i = 1; i < std::size(offsets); ++i)
            if (offsets[i] < offsets[i - 1]) return false;
        if (header_.trigramsOffset - header_.filesOffset != uint64_t(header_.files) * sizeof(FileRecord) ||
            header_.postingsOffset - header_.trigramsOffset != uint64_t(header_.trigrams) * sizeof(TrigramRecord) ||
            header_.orderOffset - header_.symbolsOffset != uint64_t(header_.symbols) * sizeof(SymbolRecord) ||
            header_.stringsOffset - header_.orderOffset != uint64_t(header_.symbols) * 4)
            return false;
        const auto *base = map_.data();
        files_ = reinterpret_cast<const FileRecord *>(base + header_.filesOffset);
        trigrams_ = reinterpret_cast<const TrigramRecord *>(base + header_.trigramsOffset);
        postings_ = base + header_.postingsOffset;
        symbols_ = reinterpret_cast<const SymbolRecord *>(base + header_.symbolsOffset);
        order_ = base + header_.orderOffset;
        strings_ = base + header_.stringsOffset;
        const uint64_t stringBytes = header_.end - header_.stringsOffset;
        for (uint32_t i = 0; i < header_.files; ++i) {
            const auto &file = files_[i];
            if (uint64_t(file.pathOffset) + file.pathLength > stringBytes ||
                uint64_t(file.symbolBegin) + file.symbolCount > header_.symbols)
                return false;
        }
        for (uint32_t i = 0; i < header_.symbols; ++i) {
            const auto &s = symbols_[i];
            if (uint64_t(s.nameOffset) + s.nameLength > stringBytes || uint64_t(s.scopeOffset) + s.scopeLength > stringBytes ||
                uint64_t(s.detailOffset) + s.detailLength > stringBytes)
                return false;
        }
        const uint64_t postingBytes = header_.symbolsOffset - header_.postingsOffset;
        for (uint32_t i = 0; i < header_.trigrams; ++i)
            if (trigrams_[i].start > postingBytes || (i && trigrams_[i].key <= trigrams_[i - 1].key)) return false;
        return true;
    }
    MappedFile map_;
    ShardHeader header_{};
    const FileRecord *files_ = nullptr;
    const TrigramRecord *trigrams_ = nullptr;
    const uint8_t *postings_ = nullptr, *order_ = nullptr, *strings_ = nullptr;
    const SymbolRecord *symbols_ = nullptr;
};

// Immutable persisted generation: shards and lookup tables.
struct Base {
    uint64_t generation = 0;
    std::vector<std::shared_ptr<const Shard>> shards;
    std::vector<std::string> shardNames;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> locations; // path -> shard, local id
    std::unordered_set<std::string> directories;
    uint64_t textBytes = 0, symbols = 0, indexBytes = 0;
    uint32_t binary = 0, oversize = 0, unreadable = 0;
    bool persisted = false;
};
struct Snapshot {
    std::shared_ptr<const Base> base;
    std::shared_ptr<const std::map<std::string, std::shared_ptr<const FileData>>> overlay;
    uint64_t syncedRevision = 0;
};

void addDirectories(std::unordered_set<std::string> &directories, std::string_view path) {
    for (auto slash = path.rfind('/'); slash != std::string_view::npos && slash; slash = path.rfind('/', slash - 1)) {
        if (!directories.emplace(path.substr(0, slash)).second) break;
    }
    directories.emplace("");
}
std::shared_ptr<Base> makeBase(uint64_t generation, std::vector<std::shared_ptr<const Shard>> shards,
                               std::vector<std::string> names, bool persisted) {
    auto base = std::make_shared<Base>();
    base->generation = generation;
    base->shards = std::move(shards);
    base->shardNames = std::move(names);
    base->persisted = persisted;
    for (uint32_t s = 0; s < base->shards.size(); ++s) {
        const auto &shard = *base->shards[s];
        base->indexBytes += shard.bytes();
        base->symbols += shard.symbolCount();
        for (uint32_t id = 0; id < shard.files(); ++id) {
            const auto path = std::string(shard.path(id));
            const auto &record = shard.file(id);
            if (record.flags & flagBinary) ++base->binary;
            else if (record.flags & flagOversize) ++base->oversize;
            else if (record.flags & flagUnreadable) ++base->unreadable;
            else base->textBytes += record.size;
            addDirectories(base->directories, path);
            base->locations.emplace(path, std::pair<uint32_t, uint32_t>{s, id});
        }
    }
    return base;
}

std::vector<std::string> enumerateFiles(const fs::path &root, const std::vector<std::string> &subpaths,
                                        const std::atomic<bool> &abort, std::string &error) {
    std::vector<std::string> paths;
    std::string pending;
    // Same enumeration as search.text: ripgrep ignore rules, hidden files, no .git.
    // runtime/ and data/ are excluded at the workspace root only (anchored globs).
    std::vector<std::string> argv{"rg", "--files", "--hidden", "--no-require-git", "-0", "-g", "!.git", "-g", "!/runtime",
                                  "-g", "!/data", "-g", "!.cycode"};
    if (!subpaths.empty()) {
        argv.push_back("--");
        for (const auto &path : subpaths) argv.push_back(path);
    }
    const auto result = runProcessStreamed(argv, root, abort, [&](std::string_view chunk) {
        pending.append(chunk);
        size_t start = 0;
        while (start < pending.size()) {
            const auto end = pending.find('\0', start);
            if (end == std::string::npos) break;
            auto path = pending.substr(start, end - start);
            std::replace(path.begin(), path.end(), '\\', '/');
            if (path.starts_with("./")) path.erase(0, 2);
            if (!path.empty()) paths.push_back(std::move(path));
            start = end + 1;
        }
        pending.erase(0, start);
    });
    if (result.cancelled || abort) error = "cancelled";
    else if (result.exitCode != 0 && result.exitCode != 1) error = "File enumeration failed: " + processOutputText(result.output, true);
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

bool ignoredPath(std::string_view path) {
    // Mirrors the enumeration globs: .git/.cycode at any depth, runtime/data at the root.
    size_t start = 0;
    bool first = true;
    while (start <= path.size()) {
        auto end = path.find('/', start);
        if (end == std::string_view::npos) end = path.size();
        auto part = std::string(path.substr(start, end - start));
#ifdef _WIN32
        for (auto &c : part) c = char(std::tolower(static_cast<unsigned char>(c)));
#endif
        if (part == ".git" || part == ".cycode" || (first && (part == "runtime" || part == "data"))) return true;
        first = false;
        start = end + 1;
        if (end == path.size()) break;
    }
    return false;
}

// -------------------------------------------------------------- query plans
// OR of alternatives; each alternative is an AND of required literals (>= 3 bytes).
using LiteralPlan = std::vector<std::vector<std::string>>;
std::optional<LiteralPlan> regexPlan(std::string_view pattern) {
    // Top-level alternation only; groups, classes and escapes are opaque. Any required
    // literal extracted here is a substring of every possible match of its alternative.
    std::vector<std::string_view> alternatives;
    size_t depth = 0, start = 0;
    bool klass = false;
    for (size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        if (c == '\\') {
            ++i;
            continue;
        }
        if (klass) {
            if (c == ']') klass = false;
            continue;
        }
        if (c == '[') klass = true;
        else if (c == '(') ++depth;
        else if (c == ')' && depth) --depth;
        else if (c == '|' && !depth) {
            alternatives.push_back(pattern.substr(start, i - start));
            start = i + 1;
        }
    }
    alternatives.push_back(pattern.substr(start));
    LiteralPlan plan;
    for (const auto alternative : alternatives) {
        std::vector<std::string> required;
        std::string run;
        auto flush = [&] {
            if (run.size() >= 3) required.push_back(run);
            run.clear();
        };
        size_t groupDepth = 0;
        for (size_t i = 0; i < alternative.size(); ++i) {
            const char c = alternative[i];
            const char next = i + 1 < alternative.size() ? alternative[i + 1] : '\0';
            const bool optional = next == '?' || next == '*' || next == '{';
            if (groupDepth) {
                if (c == '\\') ++i;
                else if (c == '(') ++groupDepth;
                else if (c == ')') --groupDepth;
                continue;
            }
            if (c == '(') {
                flush();
                ++groupDepth;
                continue;
            }
            if (c == '[') {
                flush();
                for (++i; i < alternative.size() && alternative[i] != ']'; ++i)
                    if (alternative[i] == '\\') ++i;
                continue;
            }
            std::optional<char> literal;
            if (c == '\\' && i + 1 < alternative.size()) {
                const char escaped = alternative[++i];
                if (std::ispunct(static_cast<unsigned char>(escaped))) literal = escaped;
                else {
                    flush();
                    continue;
                }
            } else if (std::string_view(".^$*+?{}|)").find(c) == std::string_view::npos)
                literal = c;
            if (!literal) {
                flush();
                continue;
            }
            const char after = i + 1 < alternative.size() ? alternative[i + 1] : '\0';
            if (after == '?' || after == '*' || after == '{' || (c != '\\' && optional && false)) {
                flush(); // this character may be absent
                continue;
            }
            run += *literal;
            if (after == '+') flush();
        }
        flush();
        if (required.empty()) return std::nullopt; // an unconstrained alternative matches anything
        plan.push_back(std::move(required));
    }
    return plan;
}
std::optional<LiteralPlan> queryPlan(const std::string &query, bool regex) {
    if (!regex) {
        if (query.size() < 3) return std::nullopt;
        return LiteralPlan{{query}};
    }
    return regexPlan(query);
}
std::vector<uint32_t> literalTrigrams(std::string_view literal, bool sensitive) {
    std::vector<uint32_t> keys;
    for (size_t i = 0; i + 2 < literal.size(); ++i) {
        const uint8_t a = uint8_t(fold(literal[i])), b = uint8_t(fold(literal[i + 1])), c = uint8_t(fold(literal[i + 2]));
        // Unicode case folding is not byte-for-byte: only ASCII trigrams constrain caseless queries.
        if (!sensitive && (a >= 0x80 || b >= 0x80 || c >= 0x80)) continue;
        keys.push_back((uint32_t(a) << 16) | (uint32_t(b) << 8) | c);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}
using TrigramPlan = std::vector<std::vector<uint32_t>>; // OR of AND trigram sets
std::optional<TrigramPlan> trigramPlan(const std::optional<LiteralPlan> &plan, bool sensitive) {
    if (!plan) return std::nullopt;
    TrigramPlan result;
    for (const auto &alternative : *plan) {
        std::vector<uint32_t> keys;
        for (const auto &literal : alternative) {
            const auto part = literalTrigrams(literal, sensitive);
            keys.insert(keys.end(), part.begin(), part.end());
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        if (keys.empty()) return std::nullopt;
        result.push_back(std::move(keys));
    }
    return result;
}
bool fileMatchesPlan(const std::vector<uint32_t> &trigrams, const TrigramPlan &plan) {
    for (const auto &alternative : plan)
        if (std::all_of(alternative.begin(), alternative.end(),
                        [&](uint32_t key) { return std::binary_search(trigrams.begin(), trigrams.end(), key); }))
            return true;
    return false;
}

// -------------------------------------------------------------- matching
class Matcher {
  public:
    Matcher(const std::string &query, bool regex, bool sensitive) : regex_(regex), sensitive_(sensitive) {
        pcre_ = regex || (!sensitive && hasNonAscii(query));
        needle_ = sensitive ? query : folded(query);
        if (!pcre_) return;
        uint32_t flags = PCRE2_UTF | PCRE2_MATCH_INVALID_UTF | PCRE2_UCP;
        if (!sensitive) flags |= PCRE2_CASELESS;
        if (!regex) flags |= PCRE2_LITERAL;
        int error = 0;
        PCRE2_SIZE offset = 0;
        code_ = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(query.data()), query.size(), flags, &error, &offset, nullptr);
        if (!code_) {
            std::array<PCRE2_UCHAR, 256> message{};
            pcre2_get_error_message(error, message.data(), message.size());
            throw NativeError(ErrorCode::InvalidParams,
                              "Invalid regular expression at offset " + std::to_string(offset) + ": " +
                                  reinterpret_cast<const char *>(message.data()));
        }
        context_ = pcre2_match_context_create(nullptr);
        pcre2_set_match_limit(context_, 10'000'000);
        pcre2_set_depth_limit(context_, 10'000);
    }
    ~Matcher() {
        if (context_) pcre2_match_context_free(context_);
        if (code_) pcre2_code_free(code_);
    }
    Matcher(const Matcher &) = delete;
    Matcher &operator=(const Matcher &) = delete;
    // Matches within one line (without terminator). Returns false on a matcher limit error.
    template <class Callback> bool line(std::string_view text, Callback &&callback, pcre2_match_data *data) const {
        if (!pcre_) {
            std::string scratch;
            std::string_view haystack = text;
            if (!sensitive_) {
                scratch = folded(text);
                haystack = scratch;
            }
            for (size_t at = haystack.find(needle_); at != std::string_view::npos;
                 at = haystack.find(needle_, at + std::max<size_t>(needle_.size(), 1)))
                if (!callback(at, at + needle_.size())) return true;
            return true;
        }
        PCRE2_SIZE offset = 0;
        while (offset <= text.size()) {
            const int result = pcre2_match(code_, reinterpret_cast<PCRE2_SPTR>(text.data()), text.size(), offset, 0,
                                           data, context_);
            if (result == PCRE2_ERROR_NOMATCH) return true;
            if (result < 0) return false;
            const auto *vector = pcre2_get_ovector_pointer(data);
            const auto start = vector[0], end = vector[1];
            if (!callback(start, end)) return true;
            offset = end > start ? end : start + 1;
        }
        return true;
    }
    // Whole-buffer prefilter for literal searches: false when the text cannot match.
    bool mayMatch(std::string_view text) const {
        if (pcre_) return true;
        if (sensitive_) return text.find(needle_) != std::string_view::npos;
        return true;
    }
    pcre2_match_data *data() const { return pcre_ ? pcre2_match_data_create_from_pattern(code_, nullptr) : nullptr; }
    bool pcre() const { return pcre_; }

  private:
    bool regex_, sensitive_, pcre_ = false;
    std::string needle_;
    pcre2_code *code_ = nullptr;
    pcre2_match_context *context_ = nullptr;
};

struct SymbolView {
    std::string name, scope;
    SymbolKind kind;
    uint32_t line, endLine;
};
// Innermost function/method/type whose range contains the line.
std::optional<SymbolView> enclosing(const std::vector<SymbolView> &symbols, uint32_t line) {
    const SymbolView *best = nullptr;
    for (const auto &symbol : symbols) {
        if (symbol.line > line) break;
        if (symbol.endLine < line) continue;
        const bool body = symbol.kind == SymbolKind::Function || symbol.kind == SymbolKind::Method ||
                          symbol.kind == SymbolKind::Constructor || symbol.kind == SymbolKind::Destructor ||
                          symbolKindIsContainer(symbol.kind) || symbol.kind == SymbolKind::Property;
        if (!body && !(symbol.line == line)) continue;
        if (!best || symbol.line >= best->line) best = &symbol;
    }
    if (!best) return std::nullopt;
    return *best;
}
std::string qualified(const std::string &scope, const std::string &name) {
    if (scope.empty()) return name;
    return scope + (scope.find('.') != std::string::npos && scope.find("::") == std::string::npos ? "." : "::") + name;
}

std::string preview(std::string_view line, size_t column) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.remove_suffix(1);
    size_t lead = 0;
    while (lead < line.size() && (line[lead] == ' ' || line[lead] == '\t')) ++lead;
    size_t begin = lead;
    constexpr size_t limit = 240;
    if (column > begin + 80 && line.size() - begin > limit) begin = column - 80;
    while (begin < line.size() && (static_cast<unsigned char>(line[begin]) & 0xc0) == 0x80) ++begin;
    size_t end = std::min(line.size(), begin + limit);
    while (end < line.size() && end > begin && (static_cast<unsigned char>(line[end]) & 0xc0) == 0x80) --end;
    std::string result;
    if (begin > lead) result = "...";
    result.append(line.substr(begin, end - begin));
    if (end < line.size()) result += "...";
    return utf8(result);
}

std::vector<std::string> stringList(const Json &params, const char *name, size_t limit, size_t length) {
    std::vector<std::string> result;
    if (!params.contains(name)) return result;
    const auto &value = params.at(name);
    if (!value.is_array() || value.size() > limit) throw NativeError(ErrorCode::InvalidParams, std::string(name) + " must be an array of at most " + std::to_string(limit) + " strings");
    for (const auto &item : value) {
        if (!item.is_string() || item.get_ref<const std::string &>().size() > length)
            throw NativeError(ErrorCode::InvalidParams, std::string(name) + " entries must be strings of at most " + std::to_string(length) + " bytes");
        result.push_back(item.get<std::string>());
    }
    return result;
}
std::vector<std::string> pathFilters(const Json &params) {
    auto paths = stringList(params, "paths", 64, 1024);
    for (auto &path : paths) {
        std::replace(path.begin(), path.end(), '\\', '/');
        while (path.starts_with("./")) path.erase(0, 2);
        while (!path.empty() && path.back() == '/') path.pop_back();
        if (path.find('\0') != std::string::npos || path.starts_with("/") || (path.size() > 1 && path[1] == ':') ||
            path == ".." || path.starts_with("../") || path.find("/../") != std::string::npos || path.ends_with("/.."))
            throw NativeError(ErrorCode::InvalidParams, "paths must be workspace-relative prefixes");
    }
    paths.erase(std::remove(paths.begin(), paths.end(), std::string{}), paths.end());
    return paths;
}
bool pathSelected(std::string_view path, const std::vector<std::string> &prefixes, const std::vector<std::string> &extensions) {
    if (!prefixes.empty()) {
        bool inside = false;
        for (const auto &prefix : prefixes) {
#ifdef _WIN32
            const bool head = path.size() >= prefix.size() &&
                              _strnicmp(path.data(), prefix.data(), prefix.size()) == 0;
#else
            const bool head = path.starts_with(prefix);
#endif
            if (head && (path.size() == prefix.size() || path[prefix.size()] == '/')) {
                inside = true;
                break;
            }
        }
        if (!inside) return false;
    }
    if (!extensions.empty()) {
        const auto dot = path.find_last_of('.');
        const auto slash = path.find_last_of('/');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return false;
        const auto extension = folded(path.substr(dot));
        return std::any_of(extensions.begin(), extensions.end(), [&](const std::string &e) { return e == extension; });
    }
    return true;
}
std::vector<std::string> extensionFilters(const Json &params) {
    auto extensions = stringList(params, "extensions", 64, 32);
    for (auto &extension : extensions) {
        extension = folded(extension);
        if (!extension.empty() && extension.front() != '.') extension.insert(extension.begin(), '.');
    }
    return extensions;
}
struct Cursor {
    std::string path;
    uint64_t line = 0, column = 0;
    bool valid = false;
};
Cursor parseCursor(const Json &params) {
    Cursor cursor;
    if (!params.contains("cursor") || params.at("cursor").is_null()) return cursor;
    if (!params.at("cursor").is_string() || params.at("cursor").get_ref<const std::string &>().size() > 8192)
        throw NativeError(ErrorCode::InvalidParams, "cursor must be the nextCursor string of a previous page");
    const auto &value = params.at("cursor").get_ref<const std::string &>();
    const auto a = value.find('\n'), b = a == std::string::npos ? a : value.find('\n', a + 1);
    if (!value.starts_with("1|") || a == std::string::npos || b == std::string::npos)
        throw NativeError(ErrorCode::InvalidParams, "cursor must be the nextCursor string of a previous page");
    try {
        cursor.path = value.substr(2, a - 2);
        cursor.line = std::stoull(value.substr(a + 1, b - a - 1));
        cursor.column = std::stoull(value.substr(b + 1));
    } catch (const std::exception &) {
        throw NativeError(ErrorCode::InvalidParams, "cursor must be the nextCursor string of a previous page");
    }
    cursor.valid = true;
    return cursor;
}
std::string makeCursor(const std::string &path, uint64_t line, uint64_t column) {
    return "1|" + path + "\n" + std::to_string(line) + "\n" + std::to_string(column);
}

// Rider-like ranking: exact, prefix, CamelHump initials, substring, subsequence.
// foldedQuery is the ASCII-lowercased query; no allocation per candidate.
int symbolScore(std::string_view name, std::string_view query, std::string_view foldedQuery) {
    if (query.empty()) return 1;
    const size_t n = name.size(), m = foldedQuery.size();
    if (name == query) return 1000;
    auto equalAt = [&](size_t at) {
        for (size_t k = 0; k < m; ++k)
            if (fold(name[at + k]) != foldedQuery[k]) return false;
        return true;
    };
    if (n >= m && equalAt(0)) return n == m ? 950 : 800 - int(std::min<size_t>(n - m, 100));
    if (m >= 2) {
        size_t q = 0;
        for (size_t i = 0; i < n && q < m; ++i) {
            const char c = name[i];
            const bool start = i == 0 || std::isupper(static_cast<unsigned char>(c)) || name[i - 1] == '_' ||
                               (std::isdigit(static_cast<unsigned char>(c)) && !std::isdigit(static_cast<unsigned char>(name[i - 1])));
            if (fold(c) == foldedQuery[q] && (start || (q && fold(name[i - 1]) == foldedQuery[q - 1]))) ++q;
        }
        if (q == m) return 600 - int(std::min<size_t>(n, 100));
    }
    for (size_t at = 1; at + m <= n; ++at)
        if (equalAt(at)) return 400 - int(std::min<size_t>(at, 100));
    if (m < 3) return 0;
    size_t q = 0;
    for (size_t i = 0; i < n && q < m; ++i)
        if (fold(name[i]) == foldedQuery[q]) ++q;
    return q == m ? 100 : 0;
}
} // namespace

// ------------------------------------------------------------------ state
struct CodeIndex::State {
    fs::path root, directory;
    // Default storage is the project's own .cycode/index; when it cannot be created (read-only
    // project, a file named .cycode) the first build falls back to the per-user cache. The
    // directory only changes before the first build starts, under `mutex`.
    fs::path fallbackDirectory;
    std::string storage = "configured", storageNote;
    WorkspaceMonitor &monitor;
    std::string disabledReason;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::shared_ptr<const Snapshot> snapshot;
    std::thread worker;
    bool working = false, stopping = false;
    std::atomic<bool> abort{false};
    std::string phase = "idle", error;
    std::atomic<uint64_t> progressDone{0}, progressTotal{0};
    // Interactive scans in progress (ripgrep fallback). The background build pauses between
    // files meanwhile: a scan competing with 12 indexing threads for the disk can exceed its
    // deadline and return partial results, whereas pausing costs the build only that time.
    std::atomic<int> foreground{0};
    Clock::time_point progressStart = Clock::now();
    Json lastBuild, lastLoad, lastReconcile, lastCompaction;
    std::unique_ptr<NativeFileLease> writer;
    bool readOnly = false;
    std::mutex syncMutex;

    State(fs::path rootPath, WorkspaceMonitor &owner) : root(std::move(rootPath)), monitor(owner) {}
    ~State() { stopWorker(); }

    void stopWorker() noexcept {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        abort = true;
        if (worker.joinable()) worker.join();
    }
    fs::path manifestPath() const { return directory / "manifest.json"; }
    fs::path lockedManifestPath() const {
        std::lock_guard lock(mutex);
        return manifestPath();
    }
    std::shared_ptr<const Snapshot> current() const {
        std::lock_guard lock(mutex);
        return snapshot;
    }
    void publish(std::shared_ptr<const Snapshot> value) {
        {
            std::lock_guard lock(mutex);
            snapshot = std::move(value);
        }
        changed.notify_all();
    }
    void setPhase(std::string value) {
        std::lock_guard lock(mutex);
        phase = std::move(value);
    }
    void yieldToForeground() const {
        while (foreground.load(std::memory_order_relaxed) > 0 && !abort)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    unsigned threads() const {
        const auto hardware = std::max(2u, std::thread::hardware_concurrency());
        return std::clamp(hardware / 2, 2u, 12u);
    }

    // Reads and indexes files in parallel. `paths` and `stamps` are aligned.
    std::vector<FileData> loadMany(const std::vector<std::string> &paths, const std::vector<Stamp> &stamps,
                                   uint64_t &bytesRead) {
        std::vector<FileData> out(paths.size());
        std::atomic<size_t> next{0};
        std::atomic<uint64_t> bytes{0};
        auto run = [&] {
            TrigramScratch scratch;
            uint64_t local = 0;
            for (size_t i; (i = next.fetch_add(1)) < paths.size() && !abort;) {
                yieldToForeground();
                out[i] = loadFileData(root, paths[i], stamps[i], scratch, local);
                ++progressDone;
            }
            bytes += local;
        };
        std::vector<std::thread> pool;
        const auto count = std::min<size_t>(threads(), std::max<size_t>(1, paths.size() / 4));
        for (size_t t = 1; t < count; ++t) pool.emplace_back(run);
        run();
        for (auto &thread : pool) thread.join();
        bytesRead += bytes;
        return out;
    }
    std::vector<Stamp> stampMany(const std::vector<std::string> &paths) {
        std::vector<Stamp> stamps(paths.size());
        std::atomic<size_t> next{0};
        auto run = [&] {
            for (size_t i; (i = next.fetch_add(1)) < paths.size() && !abort;) {
                if ((i & 255) == 0) yieldToForeground();
                stamps[i] = stampOf(root / utf8Path(paths[i]));
            }
        };
        std::vector<std::thread> pool;
        for (unsigned t = 1; t < threads(); ++t) pool.emplace_back(run);
        run();
        for (auto &thread : pool) thread.join();
        return stamps;
    }
    std::string writeShard(uint64_t generation, size_t number, const std::vector<FileData> &files) {
        const auto name = "s" + std::to_string(generation) + "-" + std::to_string(number) + ".cyx";
        const auto bytes = serializeShard(files);
        AtomicFileOptions options;
        options.publication = AtomicPublication::Replace;
        writeFileAtomic(directory / name, bytes, options);
        return name;
    }
    // Builds shards from sorted paths; returns persisted names (empty when read-only).
    std::pair<std::vector<std::shared_ptr<const Shard>>, std::vector<std::string>>
    buildShards(uint64_t generation, size_t firstNumber, const std::vector<std::string> &paths,
                const std::vector<Stamp> &stamps, uint64_t &bytesRead) {
        std::vector<std::shared_ptr<const Shard>> shards;
        std::vector<std::string> names;
        size_t begin = 0, number = firstNumber;
        while (begin < paths.size() && !abort) {
            size_t end = begin;
            uint64_t bytes = 0;
            while (end < paths.size() && end - begin < shardFiles && (bytes < shardBytes || end == begin)) {
                bytes += std::min<uint64_t>(stamps[end].size, maximumIndexedFile);
                ++end;
            }
            const std::vector<std::string> part(paths.begin() + long(begin), paths.begin() + long(end));
            const std::vector<Stamp> partStamps(stamps.begin() + long(begin), stamps.begin() + long(end));
            auto files = loadMany(part, partStamps, bytesRead);
            if (abort) break;
            fs::path file;
            std::string name;
            if (!readOnly) {
                name = writeShard(generation, number, files);
                file = directory / name;
            } else {
                // Without the writer lease, keep a private shard image in a temporary file.
                name = "private-" + std::to_string(reinterpret_cast<uintptr_t>(this)) + "-" +
                       std::to_string(generation) + "-" + std::to_string(number) + ".cyx";
                file = fs::temp_directory_path() / name;
                AtomicFileOptions options;
                options.publication = AtomicPublication::Replace;
                options.access = AtomicAccess::PrivateUser;
                writeFileAtomic(file, serializeShard(files), options);
            }
            auto shard = Shard::open(file);
            if (!shard) throw std::runtime_error("Index shard could not be reopened after writing");
            shards.push_back(std::move(shard));
            names.push_back(name);
            ++number;
            begin = end;
        }
        return {std::move(shards), std::move(names)};
    }
    void writeManifest(const Base &base) {
        if (readOnly) return;
        Json shards = Json::array();
        for (size_t i = 0; i < base.shards.size(); ++i)
            shards.push_back({{"file", base.shardNames[i]}, {"files", base.shards[i]->files()}, {"bytes", base.shards[i]->bytes()}});
        Json manifest = {{"format", "cycode-code-index/1"}, {"shardFormat", indexFormat}, {"scanner", scannerRevision},
                         {"root", pathUtf8(root)}, {"generation", base.generation}, {"shards", shards},
                         {"writtenAt", isoNow()}};
        AtomicFileOptions options;
        options.publication = AtomicPublication::Replace;
        writeFileAtomic(manifestPath(), manifest.dump(1) + "\n", options);
        // Remove shard files no longer referenced; mapped files may refuse until released.
        std::set<std::string> live(base.shardNames.begin(), base.shardNames.end());
        std::error_code ec;
        for (fs::directory_iterator it(directory, ec), end; it != end && !ec; it.increment(ec)) {
            const auto name = pathUtf8(it->path().filename());
            if (name.ends_with(".cyx") && !live.contains(name)) {
                std::error_code ignored;
                fs::remove(it->path(), ignored);
            }
        }
    }
    std::shared_ptr<const Base> loadPersisted() {
        std::error_code ec;
        if (!fs::is_regular_file(manifestPath(), ec)) return nullptr;
        std::ifstream input(manifestPath(), std::ios::binary);
        const auto manifest = Json::parse(input, nullptr, false);
        if (manifest.is_discarded() || !manifest.is_object() || manifest.value("format", "") != "cycode-code-index/1" ||
            manifest.value("shardFormat", 0u) != indexFormat || manifest.value("scanner", 0u) != scannerRevision ||
            manifest.value("root", "") != pathUtf8(root) || !manifest.contains("shards") || !manifest.at("shards").is_array())
            return nullptr;
        std::vector<std::shared_ptr<const Shard>> shards;
        std::vector<std::string> names;
        for (const auto &entry : manifest.at("shards")) {
            if (!entry.is_object() || !entry.contains("file") || !entry.at("file").is_string()) return nullptr;
            const auto name = entry.at("file").get<std::string>();
            if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos || !name.ends_with(".cyx"))
                return nullptr;
            auto shard = Shard::open(directory / name);
            if (!shard) return nullptr;
            shards.push_back(std::move(shard));
            names.push_back(name);
        }
        return makeBase(manifest.value("generation", uint64_t(1)), std::move(shards), std::move(names), true);
    }
    // Compares the current enumeration with base+overlay and returns a new overlay.
    std::shared_ptr<std::map<std::string, std::shared_ptr<const FileData>>>
    reconcile(const Base &base, const std::map<std::string, std::shared_ptr<const FileData>> &overlay, Json &report) {
        const auto started = Clock::now();
        std::string error;
        auto paths = enumerateFiles(root, {}, abort, error);
        if (!error.empty()) throw std::runtime_error(error);
        const auto stamps = stampMany(paths);
        auto next = std::make_shared<std::map<std::string, std::shared_ptr<const FileData>>>();
        std::vector<std::string> reread;
        std::vector<Stamp> rereadStamps;
        size_t added = 0, modified = 0, removed = 0;
        std::unordered_set<std::string> present;
        present.reserve(paths.size());
        for (size_t i = 0; i < paths.size(); ++i) {
            const auto &path = paths[i];
            present.insert(path);
            if (!stamps[i].file) continue;
            uint64_t size = 0;
            int64_t time = 0;
            bool known = false;
            if (const auto found = overlay.find(path); found != overlay.end() && !found->second->deleted) {
                size = found->second->size;
                time = found->second->time;
                known = true;
                if (size == stamps[i].size && time == stamps[i].time) {
                    next->emplace(path, found->second);
                    continue;
                }
            } else if (const auto location = base.locations.find(path); location != base.locations.end()) {
                const auto &record = base.shards[location->second.first]->file(location->second.second);
                size = record.size;
                time = record.time;
                known = true;
                if (size == stamps[i].size && time == stamps[i].time) continue;
            }
            (known ? modified : added) += 1;
            reread.push_back(path);
            rereadStamps.push_back(stamps[i]);
        }
        for (const auto &[path, location] : base.locations)
            if (!present.contains(path)) {
                auto data = std::make_shared<FileData>();
                data->path = path;
                data->deleted = true;
                next->emplace(path, std::move(data));
                ++removed;
            }
        uint64_t bytes = 0;
        progressTotal += reread.size();
        auto files = loadMany(reread, rereadStamps, bytes);
        for (auto &file : files) {
            auto path = file.path;
            (*next)[path] = std::make_shared<FileData>(std::move(file));
        }
        report = {{"milliseconds", milliseconds(started)}, {"enumerated", paths.size()}, {"added", added},
                  {"modified", modified}, {"removed", removed}, {"bytesRead", bytes}};
        return next;
    }
    void fullBuild(uint64_t generation, uint64_t revision) {
        const auto started = Clock::now();
        setPhase("enumerating");
        std::string error;
        auto paths = enumerateFiles(root, {}, abort, error);
        if (!error.empty()) throw std::runtime_error(error);
        const auto enumerated = milliseconds(started);
        setPhase("stat");
        auto stamps = stampMany(paths);
        // Directories reported as files or vanished entries are dropped.
        std::vector<std::string> kept;
        std::vector<Stamp> keptStamps;
        for (size_t i = 0; i < paths.size(); ++i)
            if (stamps[i].file) {
                kept.push_back(std::move(paths[i]));
                keptStamps.push_back(stamps[i]);
            }
        progressDone = 0;
        progressTotal = kept.size();
        {
            std::lock_guard lock(mutex);
            progressStart = Clock::now();
        }
        setPhase("indexing");
        uint64_t bytesRead = 0;
        auto [shards, names] = buildShards(generation, 0, kept, keptStamps, bytesRead);
        if (abort) return;
        auto base = makeBase(generation, std::move(shards), std::move(names), !readOnly);
        setPhase("persisting");
        writeManifest(*base);
        lastBuild = {{"milliseconds", milliseconds(started)}, {"enumerationMilliseconds", enumerated},
                     {"files", kept.size()}, {"bytesRead", bytesRead}, {"shards", base->shards.size()},
                     {"indexBytes", base->indexBytes}, {"symbols", base->symbols}, {"generation", generation},
                     {"threads", threads()}};
        auto snapshot = std::make_shared<Snapshot>();
        snapshot->base = std::move(base);
        snapshot->overlay = std::make_shared<const std::map<std::string, std::shared_ptr<const FileData>>>();
        // Changes made while the build ran are applied by the first sync after `revision`.
        snapshot->syncedRevision = revision;
        publish(std::move(snapshot));
    }
    void startup() {
        try {
            const auto monitorRevision = monitor.snapshot().revision;
            setPhase("loading");
            const auto started = Clock::now();
            auto base = loadPersisted();
            if (base) {
                lastLoad = {{"milliseconds", milliseconds(started)}, {"shards", base->shards.size()},
                            {"files", base->locations.size()}, {"generation", base->generation}};
                setPhase("reconciling");
                Json report;
                auto overlay = reconcile(*base, {}, report);
                lastReconcile = report;
                auto snapshot = std::make_shared<Snapshot>();
                snapshot->base = base;
                snapshot->overlay = std::move(overlay);
                snapshot->syncedRevision = monitorRevision;
                const bool compact = snapshot->overlay->size() > overlayCompaction;
                publish(std::move(snapshot));
                if (compact && !readOnly) compactNow();
            } else {
                fullBuild(1, monitorRevision);
            }
            std::lock_guard lock(mutex);
            phase = abort ? "stopped" : "ready";
        } catch (const std::exception &e) {
            std::lock_guard lock(mutex);
            error = e.what();
            phase = "failed";
        }
        {
            std::lock_guard lock(mutex);
            working = false;
        }
        changed.notify_all();
    }
    void launch(bool rebuild) {
        std::lock_guard lock(mutex);
        if (working || stopping) return;
        if (worker.joinable()) worker.join();
        working = true;
        error.clear();
        abort = false;
        worker = std::thread([this, rebuild] {
            if (rebuild) {
                try {
                    const auto revision = monitor.snapshot().revision;
                    const auto generation = (current() && current()->base ? current()->base->generation : 0) + 1;
                    fullBuild(generation, revision);
                    std::lock_guard guard(mutex);
                    phase = abort ? "stopped" : "ready";
                } catch (const std::exception &e) {
                    std::lock_guard guard(mutex);
                    error = e.what();
                    phase = "failed";
                }
                {
                    std::lock_guard guard(mutex);
                    working = false;
                }
                changed.notify_all();
            } else
                startup();
        });
    }
    // Rebuilds shards touched by the overlay into a new generation.
    void compactNow() {
        const auto started = Clock::now();
        auto snapshot = current();
        if (!snapshot || readOnly || snapshot->overlay->empty()) return;
        const auto &base = *snapshot->base;
        const auto &overlay = *snapshot->overlay;
        std::set<uint32_t> affected;
        std::vector<std::string> fresh;
        for (const auto &[path, data] : overlay) {
            if (const auto location = base.locations.find(path); location != base.locations.end())
                affected.insert(location->second.first);
            else if (!data->deleted)
                fresh.push_back(path);
        }
        const uint64_t generation = base.generation + 1;
        std::vector<std::shared_ptr<const Shard>> shards;
        std::vector<std::string> names;
        uint64_t bytesRead = 0;
        size_t number = 0;
        setPhase("compacting");
        for (uint32_t s = 0; s < base.shards.size(); ++s) {
            if (!affected.contains(s)) {
                shards.push_back(base.shards[s]);
                names.push_back(base.shardNames[s]);
                continue;
            }
            std::vector<std::string> paths;
            const auto &shard = *base.shards[s];
            for (uint32_t id = 0; id < shard.files(); ++id) {
                auto path = std::string(shard.path(id));
                const auto found = overlay.find(path);
                if (found != overlay.end() && found->second->deleted) continue;
                paths.push_back(std::move(path));
            }
            const auto stamps = stampMany(paths);
            std::vector<std::string> kept;
            std::vector<Stamp> keptStamps;
            for (size_t i = 0; i < paths.size(); ++i)
                if (stamps[i].file) {
                    kept.push_back(paths[i]);
                    keptStamps.push_back(stamps[i]);
                }
            auto [built, builtNames] = buildShards(generation, 1000000 + number++, kept, keptStamps, bytesRead);
            for (size_t i = 0; i < built.size(); ++i) {
                shards.push_back(built[i]);
                names.push_back(builtNames[i]);
            }
        }
        if (!fresh.empty()) {
            const auto stamps = stampMany(fresh);
            std::vector<std::string> kept;
            std::vector<Stamp> keptStamps;
            for (size_t i = 0; i < fresh.size(); ++i)
                if (stamps[i].file) {
                    kept.push_back(fresh[i]);
                    keptStamps.push_back(stamps[i]);
                }
            auto [built, builtNames] = buildShards(generation, 2000000, kept, keptStamps, bytesRead);
            for (size_t i = 0; i < built.size(); ++i) {
                shards.push_back(built[i]);
                names.push_back(builtNames[i]);
            }
        }
        if (abort) return;
        auto next = makeBase(generation, std::move(shards), std::move(names), true);
        writeManifest(*next);
        std::lock_guard sync(syncMutex);
        auto latest = current();
        auto remaining = std::make_shared<std::map<std::string, std::shared_ptr<const FileData>>>();
        for (const auto &[path, data] : *latest->overlay) {
            const auto old = overlay.find(path);
            if (old != overlay.end() && old->second == data) continue; // incorporated
            remaining->emplace(path, data);
        }
        auto published = std::make_shared<Snapshot>();
        published->base = std::move(next);
        published->overlay = std::move(remaining);
        published->syncedRevision = latest->syncedRevision;
        publish(std::move(published));
        lastCompaction = {{"milliseconds", milliseconds(started)}, {"rebuiltShards", affected.size()},
                          {"newFiles", fresh.size()}, {"bytesRead", bytesRead}, {"generation", generation}};
    }
    // Applies monitor changes since the snapshot's revision. Returns timing/report.
    Json sync(const std::atomic<bool> *cancellation) {
        std::lock_guard sync(syncMutex);
        auto snapshot = current();
        if (!snapshot) return Json::object();
        const auto changes = monitor.changesSince(snapshot->syncedRevision);
        if (changes.revision == snapshot->syncedRevision) return Json::object();
        const auto started = Clock::now();
        std::shared_ptr<std::map<std::string, std::shared_ptr<const FileData>>> overlay;
        Json report;
        if (!changes.complete) {
            overlay = reconcile(*snapshot->base, *snapshot->overlay, report);
            report["reason"] = "change log incomplete; full reconciliation";
            lastReconcile = report;
        } else {
            overlay = std::make_shared<std::map<std::string, std::shared_ptr<const FileData>>>(*snapshot->overlay);
            const auto &base = *snapshot->base;
            std::set<std::string> paths(changes.paths.begin(), changes.paths.end());
            std::vector<std::string> files, directories;
            size_t removed = 0;
            auto known = [&](const std::string &path) {
                const auto found = overlay->find(path);
                if (found != overlay->end()) return !found->second->deleted;
                return base.locations.contains(path);
            };
            auto removeUnder = [&](const std::string &prefix) {
                const auto mark = [&](const std::string &path) {
                    auto data = std::make_shared<FileData>();
                    data->path = path;
                    data->deleted = true;
                    (*overlay)[path] = std::move(data);
                    ++removed;
                };
                if (known(prefix)) mark(prefix);
                const auto head = prefix + "/";
                if (base.directories.contains(prefix))
                    for (const auto &[path, location] : base.locations)
                        if (path.starts_with(head) && known(path)) mark(path);
                std::vector<std::string> overlayPaths;
                for (const auto &[path, data] : *overlay)
                    if (path.starts_with(head) && !data->deleted) overlayPaths.push_back(path);
                for (const auto &path : overlayPaths) mark(path);
            };
            for (const auto &path : paths) {
                if (cancellation && cancellation->load()) break;
                if (path.empty() || ignoredPath(path)) continue;
                const auto stamp = stampOf(root / utf8Path(path));
                if (stamp.file) {
                    // Accept files the enumeration would list: already indexed, or in an indexed
                    // directory (files under ignored build outputs never had indexed siblings).
                    const auto slash = path.rfind('/');
                    const auto parent = slash == std::string::npos ? std::string{} : path.substr(0, slash);
                    bool parentKnown = base.directories.contains(parent);
                    if (!parentKnown)
                        for (const auto &[other, data] : *overlay)
                            if (!data->deleted && other.size() > parent.size() && other.starts_with(parent + "/")) {
                                parentKnown = true;
                                break;
                            }
                    if (known(path) || parentKnown) files.push_back(path);
                } else if (stamp.directory)
                    directories.push_back(path);
                else
                    removeUnder(path);
            }
            for (const auto &directory : directories) {
                std::string error;
                std::atomic<bool> abortEnumeration{false};
                const auto listed = enumerateFiles(root, {directory}, abortEnumeration, error);
                if (!error.empty()) continue;
                std::unordered_set<std::string> present(listed.begin(), listed.end());
                const auto head = directory + "/";
                std::vector<std::string> stale;
                for (const auto &[path, location] : base.locations)
                    if (path.starts_with(head) && !present.contains(path) && known(path)) stale.push_back(path);
                for (const auto &[path, data] : *overlay)
                    if (path.starts_with(head) && !data->deleted && !present.contains(path)) stale.push_back(path);
                for (const auto &path : stale) {
                    auto data = std::make_shared<FileData>();
                    data->path = path;
                    data->deleted = true;
                    (*overlay)[path] = std::move(data);
                    ++removed;
                }
                files.insert(files.end(), listed.begin(), listed.end());
            }
            std::sort(files.begin(), files.end());
            files.erase(std::unique(files.begin(), files.end()), files.end());
            const auto stamps = stampMany(files);
            std::vector<std::string> reread;
            std::vector<Stamp> rereadStamps;
            for (size_t i = 0; i < files.size(); ++i) {
                if (!stamps[i].file) continue;
                // Skip unchanged stamps (e.g. a directory event listing untouched files).
                if (const auto found = overlay->find(files[i]); found != overlay->end() && !found->second->deleted &&
                                                               found->second->size == stamps[i].size &&
                                                               found->second->time == stamps[i].time)
                    continue;
                if (overlay->find(files[i]) == overlay->end())
                    if (const auto location = base.locations.find(files[i]); location != base.locations.end()) {
                        const auto &record = base.shards[location->second.first]->file(location->second.second);
                        if (record.size == stamps[i].size && record.time == stamps[i].time) continue;
                    }
                reread.push_back(files[i]);
                rereadStamps.push_back(stamps[i]);
            }
            uint64_t bytes = 0;
            auto loaded = loadMany(reread, rereadStamps, bytes);
            for (auto &file : loaded) {
                auto path = file.path;
                (*overlay)[path] = std::make_shared<FileData>(std::move(file));
            }
            report = {{"milliseconds", milliseconds(started)}, {"changedPaths", changes.paths.size()},
                      {"reindexed", reread.size()}, {"removed", removed}, {"bytesRead", bytes}};
        }
        auto next = std::make_shared<Snapshot>();
        next->base = snapshot->base;
        next->overlay = std::move(overlay);
        next->syncedRevision = changes.revision;
        const bool compact = next->overlay->size() > overlayCompaction;
        publish(std::move(next));
        if (compact && !readOnly) {
            std::lock_guard lock(mutex);
            if (!working && !stopping) {
                if (worker.joinable()) worker.join();
                working = true;
                worker = std::thread([this] {
                    try {
                        compactNow();
                    } catch (const std::exception &e) {
                        std::lock_guard guard(mutex);
                        error = std::string("Compaction failed: ") + e.what();
                    }
                    {
                        std::lock_guard guard(mutex);
                        working = false;
                        phase = "ready";
                    }
                    changed.notify_all();
                });
            }
        }
        return report;
    }

    struct Candidate {
        std::string path;
        int32_t shard = -1;   // >= 0: base shard file
        uint32_t local = 0;
        std::shared_ptr<const FileData> overlay; // overlay entry
        const std::string *unsaved = nullptr;    // unsaved buffer text
    };
    std::vector<SymbolView> symbolsOf(const Candidate &candidate, const Snapshot &snapshot, std::string_view text) const {
        std::vector<SymbolView> result;
        if (candidate.unsaved) {
            for (auto &symbol : scanSymbols(text, sourceLanguageForPath(candidate.path)))
                result.push_back({std::move(symbol.name), std::move(symbol.scope), symbol.kind, symbol.line, symbol.endLine});
        } else if (candidate.overlay) {
            for (const auto &symbol : candidate.overlay->symbols)
                result.push_back({symbol.name, symbol.scope, symbol.kind, symbol.line, symbol.endLine});
        } else if (candidate.shard >= 0) {
            const auto &shard = *snapshot.base->shards[size_t(candidate.shard)];
            const auto &record = shard.file(candidate.local);
            for (uint32_t i = 0; i < record.symbolCount; ++i) {
                const auto &symbol = shard.symbol(record.symbolBegin + i);
                result.push_back({std::string(shard.string(symbol.nameOffset, symbol.nameLength)),
                                  std::string(shard.string(symbol.scopeOffset, symbol.scopeLength)),
                                  SymbolKind(symbol.kind), symbol.line, symbol.endLine});
            }
        }
        std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.line < b.line; });
        return result;
    }
};

CodeIndex::CodeIndex(fs::path root, WorkspaceMonitor &monitor) : state_(std::make_shared<State>(std::move(root), monitor)) {
    auto &state = *state_;
    std::string configured;
#ifdef _WIN32
    if (const wchar_t *value = _wgetenv(L"CYCODE_INDEX_DIR")) configured = pathUtf8(fs::path(value));
    if (const wchar_t *off = _wgetenv(L"CYCODE_INDEX"); off && std::wstring(off) == L"off")
        state.disabledReason = "Disabled by CYCODE_INDEX=off";
#else
    if (const char *value = std::getenv("CYCODE_INDEX_DIR")) configured = value;
    if (const char *off = std::getenv("CYCODE_INDEX"); off && std::string(off) == "off")
        state.disabledReason = "Disabled by CYCODE_INDEX=off";
#endif
    try {
        if (configured.empty()) {
            state.directory = projectCodeIndexDirectory(state.root);
            state.fallbackDirectory = defaultCodeIndexDirectory(state.root);
            state.storage = "project";
            // A project that could not hold its index (read-only) keeps reusing the cached one.
            std::error_code ec;
            if (!fs::is_regular_file(state.directory / "manifest.json", ec) &&
                fs::is_regular_file(state.fallbackDirectory / "manifest.json", ec)) {
                state.directory = state.fallbackDirectory;
                state.storage = "user-cache";
                state.storageNote = "reusing the index cached for this project in the user cache";
            }
        } else {
            const auto key = defaultCodeIndexDirectory(state.root).filename();
            state.directory = fs::absolute(utf8Path(configured)) / key;
        }
        const auto canonicalRoot = fs::weakly_canonical(state.root);
        const auto canonicalDirectory = fs::weakly_canonical(state.directory);
        const auto relative = canonicalDirectory.lexically_relative(canonicalRoot);
        if (!relative.empty() && *relative.begin() != ".." && !relative.is_absolute()) {
            const auto first = pathUtf8(*relative.begin());
            if (first != ".cycode" && first != ".git")
                state.disabledReason = "Index directory must not be inside the indexed workspace (except .cycode)";
        }
    } catch (const std::exception &e) {
        state.disabledReason = std::string("No usable index directory: ") + e.what();
    }
}
CodeIndex::~CodeIndex() { stop(); }
void CodeIndex::stop() noexcept {
    if (state_) state_->stopWorker();
}
bool CodeIndex::disabled() const { return !state_->disabledReason.empty(); }
bool CodeIndex::persisted() const {
    std::error_code ec;
    return !disabled() && fs::is_regular_file(state_->lockedManifestPath(), ec);
}
void CodeIndex::foregroundBegin() noexcept { ++state_->foreground; }
void CodeIndex::foregroundEnd() noexcept { --state_->foreground; }

bool CodeIndex::startsAtLaunch() {
#ifdef _WIN32
    const wchar_t *value = _wgetenv(L"CYCODE_INDEX_AT_START");
    return value && std::wstring(value) == L"1";
#else
    const char *value = std::getenv("CYCODE_INDEX_AT_START");
    return value && std::string(value) == "1";
#endif
}

void CodeIndex::warmStart() {
    if (disabled()) return;
    if (persisted() || startsAtLaunch()) (void)ready(std::chrono::milliseconds(0));
}

fs::path projectCodeIndexDirectory(const fs::path &root) { return root / ".cycode" / "index"; }

fs::path defaultCodeIndexDirectory(const fs::path &root) {
    auto key = pathUtf8(fs::weakly_canonical(root));
#ifdef _WIN32
    for (auto &c : key) c = char(std::tolower(static_cast<unsigned char>(c)));
#endif
    std::array<uint8_t, 16> hash{};
    crypto_blake2b(hash.data(), hash.size(), reinterpret_cast<const uint8_t *>(key.data()), key.size());
    auto name = pathUtf8(root.filename());
    std::string safe;
    for (const char c : name)
        safe += std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ? c : '_';
    if (safe.size() > 40) safe.resize(40);
    fs::path base;
#ifdef _WIN32
    if (const wchar_t *local = _wgetenv(L"LOCALAPPDATA"); local && *local) base = fs::path(local) / "CyCode" / "index";
    else base = fs::temp_directory_path() / "CyCode" / "index";
#else
    if (const char *cache = std::getenv("XDG_CACHE_HOME"); cache && *cache) base = fs::path(cache) / "cycode" / "index";
    else if (const char *home = std::getenv("HOME"); home && *home) base = fs::path(home) / ".cache" / "cycode" / "index";
    else base = fs::temp_directory_path() / "cycode-index";
#endif
    return base / utf8Path(safe + "-" + hex(hash.data(), hash.size()));
}

bool CodeIndex::ready(std::chrono::milliseconds wait, const std::atomic<bool> *cancellation) {
    auto &state = *state_;
    if (disabled()) return false;
    {
        std::unique_lock lock(state.mutex);
        if (!state.snapshot && !state.working && state.phase != "failed") {
            std::error_code ec;
            fs::create_directories(state.directory, ec);
            if (state.storage == "project") {
                if (!ec) {
                    // Keep the index out of version control without touching the project's own ignores.
                    const auto ignore = state.directory / ".gitignore";
                    if (!fs::exists(ignore, ec)) std::ofstream(ignore, std::ios::binary) << "*\n";
                    ec.clear();
                } else if (!state.fallbackDirectory.empty() && !state.snapshot) {
                    state.storageNote = "project .cycode/index unavailable (" + ec.message() + "); using the user cache";
                    state.directory = state.fallbackDirectory;
                    state.storage = "user-cache";
                    ec.clear();
                    fs::create_directories(state.directory, ec);
                }
            }
            lock.unlock();
            if (ec) {
                std::lock_guard guard(state.mutex);
                state.phase = "failed";
                state.error = "Cannot create index directory: " + ec.message();
                return false;
            }
            if (!state.writer) {
                state.writer = std::make_unique<NativeFileLease>(state.directory / "writer.lock");
                state.readOnly = !state.writer->acquired();
            }
            state.launch(false);
            lock.lock();
        }
        const auto deadline = Clock::now() + wait;
        while (!(state.snapshot && !(state.working && state.phase != "ready" && state.phase != "compacting"))) {
            if (state.phase == "failed" || (cancellation && cancellation->load())) return false;
            if (state.changed.wait_until(lock, std::min(deadline, Clock::now() + std::chrono::milliseconds(100))) ==
                    std::cv_status::timeout &&
                Clock::now() >= deadline)
                break;
        }
        if (!state.snapshot) return false;
        if (state.working && state.phase != "compacting" && state.phase != "ready") return false;
    }
    return true;
}

Json CodeIndex::status() const {
    const auto &state = *state_;
    std::lock_guard lock(state.mutex);
    Json result = {{"engine", "cycode-code-index"}, {"format", indexFormat}, {"scanner", scannerRevision},
                   {"directory", pathUtf8(state.directory)}, {"phase", disabled() ? "disabled" : state.phase},
                   {"ready", state.snapshot != nullptr && (state.phase == "ready" || state.phase == "compacting")},
                   {"readOnly", state.readOnly}, {"storage", state.storage}};
    if (!state.storageNote.empty()) result["storageNote"] = state.storageNote;
    if (disabled()) result["disabledReason"] = state.disabledReason;
    if (!state.error.empty()) result["error"] = state.error;
    if (state.working) {
        const auto done = state.progressDone.load(), total = state.progressTotal.load();
        Json progress = {{"done", done}, {"total", total}};
        const double elapsed = std::chrono::duration<double>(Clock::now() - state.progressStart).count();
        // Files indexed per second so far; an estimate, not a promise.
        if (done >= 256 && total > done && elapsed > 0.5)
            progress["etaSeconds"] = std::round((double(total - done) / (double(done) / elapsed)) * 10) / 10;
        result["progress"] = progress;
    }
    if (state.snapshot) {
        const auto &base = *state.snapshot->base;
        size_t overlayFiles = 0, overlayDeleted = 0;
        for (const auto &[path, data] : *state.snapshot->overlay) (data->deleted ? overlayDeleted : overlayFiles) += 1;
        result["generation"] = base.generation;
        result["persisted"] = base.persisted;
        result["files"] = base.locations.size() + overlayFiles;
        result["shards"] = base.shards.size();
        result["indexBytes"] = base.indexBytes;
        result["textBytes"] = base.textBytes;
        result["symbols"] = base.symbols;
        result["skipped"] = {{"binary", base.binary}, {"oversize", base.oversize}, {"unreadable", base.unreadable}};
        result["overlay"] = {{"files", overlayFiles}, {"deleted", overlayDeleted}};
        result["syncedRevision"] = state.snapshot->syncedRevision;
    }
    const auto monitor = state.monitor.snapshot();
    result["monitor"] = {{"backend", monitor.backend}, {"revision", monitor.revision}};
    if (!state.lastBuild.is_null()) result["lastBuild"] = state.lastBuild;
    if (!state.lastLoad.is_null()) result["lastLoad"] = state.lastLoad;
    if (!state.lastReconcile.is_null()) result["lastReconcile"] = state.lastReconcile;
    if (!state.lastCompaction.is_null()) result["lastCompaction"] = state.lastCompaction;
    return result;
}

Json CodeIndex::prepare(const Json &params, const std::atomic<bool> *cancellation) {
    auto &state = *state_;
    if (disabled()) throw NativeError(ErrorCode::BackendUnavailable, state.disabledReason);
    const auto wait = std::chrono::milliseconds(std::clamp<int64_t>(params.value("waitMs", int64_t(600000)), 0, 3600000));
    const bool rebuild = params.value("rebuild", false);
    const auto started = Clock::now();
    if (rebuild) {
        (void)ready(std::chrono::milliseconds(0), cancellation);
        {
            std::unique_lock lock(state.mutex);
            state.changed.wait(lock, [&] { return !state.working || (cancellation && cancellation->load()); });
        }
        if (state.readOnly) throw NativeError(ErrorCode::ReadOnly, "Another CyCode process owns this index; rebuild there");
        state.launch(true);
    }
    const bool usable = ready(wait, cancellation);
    Json sync = usable ? state.sync(cancellation) : Json::object();
    Json result = status();
    result["waitedMilliseconds"] = milliseconds(started);
    if (!sync.empty()) result["sync"] = sync;
    if (cancellation && cancellation->load()) result["cancelled"] = true;
    return result;
}

Json CodeIndex::searchText(const Json &params, const std::map<std::string, std::string> &unsaved,
                           const std::atomic<bool> *cancellation) {
    auto &state = *state_;
    const auto started = Clock::now();
    const auto query = params.at("query").get<std::string>();
    const bool sensitive = params.value("caseSensitive", false), regex = params.value("regex", false);
    const auto limit = size_t(std::clamp(params.value("limit", 500), 1, 5000));
    const auto maxPerFile = size_t(std::clamp(params.value("maxPerFile", 1000), 1, 5000));
    const auto contextLines = uint32_t(std::clamp(params.value("context", 0), 0, 10));
    const auto output = params.value("output", std::string("matches"));
    if (output != "matches" && output != "files")
        throw NativeError(ErrorCode::InvalidParams, "output must be matches or files");
    const auto prefixes = pathFilters(params);
    const auto extensions = extensionFilters(params);
    const auto cursor = parseCursor(params);
    const Matcher matcher(query, regex, sensitive);
    const auto syncReport = state.sync(cancellation);
    const auto snapshot = state.current();
    if (!snapshot) throw NativeError(ErrorCode::BackendUnavailable, "Index is not ready");
    const auto plan = trigramPlan(queryPlan(query, regex), sensitive);
    const auto planned = Clock::now();

    // Candidate selection.
    std::vector<State::Candidate> candidates;
    size_t indexedConsidered = 0, unindexedScanned = 0;
    const auto &base = *snapshot->base;
    const auto &overlay = *snapshot->overlay;
    auto masked = [&](std::string_view path) {
        const std::string key(path);
        return overlay.contains(key) || unsaved.contains(key);
    };
    for (uint32_t s = 0; s < base.shards.size(); ++s) {
        const auto &shard = *base.shards[s];
        std::vector<uint16_t> ids;
        if (plan) {
            std::set<uint16_t> union_;
            for (const auto &alternative : *plan) {
                std::vector<uint32_t> keys = alternative;
                std::sort(keys.begin(), keys.end(), [&](uint32_t a, uint32_t b) { return shard.postingLength(a) < shard.postingLength(b); });
                std::vector<uint16_t> current = shard.postings(keys.front());
                for (size_t k = 1; k < keys.size() && !current.empty(); ++k) {
                    const auto next = shard.postings(keys[k]);
                    std::vector<uint16_t> both;
                    std::set_intersection(current.begin(), current.end(), next.begin(), next.end(), std::back_inserter(both));
                    current = std::move(both);
                }
                union_.insert(current.begin(), current.end());
            }
            ids.assign(union_.begin(), union_.end());
        } else {
            ids.resize(shard.files());
            for (uint32_t i = 0; i < shard.files(); ++i) ids[i] = uint16_t(i);
        }
        indexedConsidered += shard.files();
        for (const auto id : ids) {
            const auto path = shard.path(id);
            if (!pathSelected(path, prefixes, extensions) || masked(path)) continue;
            const auto &record = shard.file(id);
            if (record.flags & (flagBinary | flagUnreadable)) continue;
            candidates.push_back({std::string(path), int32_t(s), id, nullptr, nullptr});
        }
        // Oversize files are not in posting lists: always verify them directly.
        if (plan)
            for (uint32_t id = 0; id < shard.files(); ++id)
                if ((shard.file(id).flags & flagOversize) && pathSelected(shard.path(id), prefixes, extensions) &&
                    !masked(shard.path(id))) {
                    candidates.push_back({std::string(shard.path(id)), int32_t(s), id, nullptr, nullptr});
                    ++unindexedScanned;
                }
    }
    for (const auto &[path, data] : overlay) {
        if (data->deleted || unsaved.contains(path) || !pathSelected(path, prefixes, extensions)) continue;
        if (data->flags & (flagBinary | flagUnreadable)) continue;
        if ((data->flags & flagOversize) || !plan || fileMatchesPlan(data->trigrams, *plan)) {
            candidates.push_back({path, -1, 0, data, nullptr});
            if (data->flags & flagOversize) ++unindexedScanned;
        }
    }
    for (const auto &[path, text] : unsaved)
        if (pathSelected(path, prefixes, extensions)) candidates.push_back({path, -1, 0, nullptr, &text});
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) { return a.path < b.path; });
    candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) { return a.path == b.path; }),
                     candidates.end());
    if (cursor.valid)
        candidates.erase(candidates.begin(), std::lower_bound(candidates.begin(), candidates.end(), cursor.path,
                                                              [](const auto &c, const std::string &p) { return c.path < p; }));
    const auto selected = Clock::now();

    // Verification, in parallel batches, merged in path order.
    struct Hit {
        uint32_t line, column;
        std::string text;
        Json symbol;
        std::vector<std::string> context;
    };
    struct FileResult {
        std::vector<Hit> hits;
        bool limitError = false, unreadable = false, more = false;
    };
    Json matches = Json::array(), files = Json::array();
    size_t emitted = 0, verified = 0, limitErrors = 0, unreadable = 0;
    bool truncated = false, budgetReached = false;
    std::string nextCursor;
    // maxBytes bounds the serialized rows of one response (0: unbounded). A page always holds at
    // least one row; the rest is reachable with nextCursor instead of being cut by the client.
    const auto maxBytes = size_t(std::clamp<int64_t>(params.value("maxBytes", int64_t(0)), 0, int64_t(64) << 20));
    size_t bytes = 0;
    const auto fits = [&](const Json &row, bool first) {
        if (!maxBytes) return true;
        const auto size = row.dump().size() + 1;
        if (!first && bytes + size > maxBytes) return false;
        bytes += size;
        return true;
    };
    for (size_t begin = 0, batch = 64; begin < candidates.size() && !truncated; begin += batch, batch = 512) {
        if (cancellation && cancellation->load())
            return {{"matches", Json::array()}, {"truncated", false}, {"cancelled", true}, {"engine", "index"}};
        const size_t end = std::min(candidates.size(), begin + batch);
        std::vector<FileResult> results(end - begin);
        std::atomic<size_t> next{begin};
        auto work = [&] {
            pcre2_match_data *data = matcher.data();
            for (size_t i; (i = next.fetch_add(1)) < end;) try {
                auto &candidate = candidates[i];
                auto &result = results[i - begin];
                std::string owned;
                std::string_view text;
                if (candidate.unsaved) text = *candidate.unsaved;
                else {
                    const auto stamp = stampOf(state.root / utf8Path(candidate.path));
                    if (!stamp.file || !readFile(state.root / utf8Path(candidate.path), stamp.size, owned)) {
                        result.unreadable = true;
                        continue;
                    }
                    if (std::memchr(owned.data(), 0, std::min<size_t>(owned.size(), 8192))) continue;
                    text = owned;
                }
                if (!matcher.mayMatch(text)) continue;
                std::vector<SymbolView> symbols;
                bool symbolsLoaded = false;
                uint32_t number = 0;
                size_t at = 0;
                while (at <= text.size()) {
                    const auto newline = text.find('\n', at);
                    const auto stop = newline == std::string_view::npos ? text.size() : newline;
                    auto line = text.substr(at, stop - at);
                    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                    ++number;
                    const bool ok = matcher.line(line, [&](size_t from, size_t) {
                        const auto column = uint32_t(from + 1);
                        if (cursor.valid && candidate.path == cursor.path &&
                            (number < cursor.line || (number == cursor.line && column <= cursor.column)))
                            return true;
                        if (result.hits.size() >= maxPerFile) {
                            result.more = true;
                            return false;
                        }
                        if (!symbolsLoaded) {
                            symbols = state.symbolsOf(candidate, *snapshot, text);
                            symbolsLoaded = true;
                        }
                        Hit hit{number, column, output == "matches" ? preview(line, from) : std::string{}, nullptr};
                        if (const auto in = enclosing(symbols, number))
                            hit.symbol = {{"name", utf8(qualified(in->scope, in->name))}, {"kind", scannedSymbolKindName(in->kind)},
                                          {"line", in->line}};
                        result.hits.push_back(std::move(hit));
                        return true;
                    }, data);
                    if (!ok) result.limitError = true;
                    if (result.more || newline == std::string_view::npos) break;
                    at = newline + 1;
                }
                if (contextLines && output == "matches" && !result.hits.empty()) {
                    // Surrounding lines as "N: text", so a caller does not re-read the file.
                    std::vector<size_t> starts{0};
                    for (size_t p = text.find('\n'); p != std::string_view::npos; p = text.find('\n', p + 1))
                        starts.push_back(p + 1);
                    auto lineAt = [&](uint32_t n) {
                        const auto begin = starts[n - 1];
                        const auto stop = n < starts.size() ? starts[n] - 1 : text.size();
                        auto value = text.substr(begin, stop - begin);
                        if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
                        if (value.size() > 240) value = value.substr(0, 240);
                        return std::to_string(n) + ": " + utf8(value);
                    };
                    const auto last = uint32_t(starts.size() - (text.empty() || text.back() == '\n' ? 1 : 0));
                    for (auto &hit : result.hits) {
                        const auto first = hit.line > contextLines ? hit.line - contextLines : 1;
                        const auto final = std::min(last, hit.line + contextLines);
                        for (uint32_t n = first; n <= final; ++n)
                            if (n != hit.line && n >= 1 && n <= starts.size()) hit.context.push_back(lineAt(n));
                    }
                }
            } catch (const std::exception &) {
                results[i - begin].unreadable = true;
            }
            if (data) pcre2_match_data_free(data);
        };
        std::vector<std::thread> pool;
        const auto threads = std::min<size_t>(state.threads(), end - begin);
        for (size_t t = 1; t < threads; ++t) pool.emplace_back(work);
        work();
        for (auto &thread : pool) thread.join();
        for (size_t i = begin; i < end && !truncated; ++i) {
            const auto &result = results[i - begin];
            ++verified;
            limitErrors += result.limitError;
            unreadable += result.unreadable;
            if (result.hits.empty()) continue;
            const auto &path = candidates[i].path;
            if (output == "files") {
                if (files.size() >= limit) {
                    truncated = true;
                    nextCursor = makeCursor(path, 0, 0);
                    break;
                }
                Json symbolNames = Json::array();
                std::set<std::string> seen;
                for (const auto &hit : result.hits)
                    if (hit.symbol.is_object() && symbolNames.size() < 8 &&
                        seen.insert(hit.symbol.at("name").get<std::string>()).second)
                        symbolNames.push_back(hit.symbol.at("name"));
                Json row = {{"path", utf8(path)}, {"matches", result.hits.size()}, {"moreInFile", result.more},
                            {"firstLine", result.hits.front().line}, {"symbols", symbolNames}};
                if (!fits(row, files.empty())) {
                    truncated = budgetReached = true;
                    nextCursor = makeCursor(path, 0, 0);
                    break;
                }
                files.push_back(std::move(row));
                continue;
            }
            // The cursor names the last returned match (its own path): when a page ends on a file
            // boundary, the next page resumes at the start of the following file, not at a line of
            // the previous one.
            const auto resumeAfterLast = [&] {
                nextCursor = makeCursor(matches.back().at("path").get<std::string>(), matches.back().at("line").get<uint64_t>(),
                                        matches.back().at("column").get<uint64_t>());
            };
            for (const auto &hit : result.hits) {
                if (emitted >= limit) {
                    truncated = true;
                    resumeAfterLast();
                    break;
                }
                Json row = {{"path", utf8(path)}, {"line", hit.line}, {"column", hit.column}, {"text", hit.text}};
                if (hit.symbol.is_object()) row["symbol"] = hit.symbol;
                if (!hit.context.empty()) row["context"] = hit.context;
                if (!fits(row, matches.empty())) {
                    truncated = budgetReached = true;
                    resumeAfterLast();
                    break;
                }
                matches.push_back(std::move(row));
                ++emitted;
            }
            if (!truncated && result.more && output == "matches") {
                // Per-file cap reached: remaining matches of this file are skipped, not paged.
            }
        }
    }
    if (!truncated && output == "matches" && emitted >= limit && verified < candidates.size()) {
        truncated = true;
        nextCursor = makeCursor(matches.back().at("path").get<std::string>(), matches.back().at("line").get<uint64_t>(),
                                matches.back().at("column").get<uint64_t>());
    }
    Json coverage = {{"complete", limitErrors == 0 && unreadable == 0},
                     {"candidateFiles", candidates.size()},
                     {"verifiedFiles", verified},
                     {"indexedFiles", indexedConsidered},
                     {"filteredByIndex", bool(plan)},
                     {"unindexedFilesScanned", unindexedScanned},
                     {"unsavedBuffers", unsaved.size()},
                     {"skippedBinaryFiles", base.binary},
                     {"matcherLimitErrors", limitErrors},
                     {"unreadableFiles", unreadable}};
    if (!plan) coverage["scanReason"] = regex ? "regex has an alternative without a required literal of 3+ bytes"
                                              : "query shorter than 3 bytes";
    Json result = {{"engine", "index"},
                   {"truncated", truncated},
                   {"timedOut", false},
                   {"unsavedRegexExcluded", false},
                   {"coverage", coverage},
                   {"index", {{"generation", base.generation}, {"syncedRevision", snapshot->syncedRevision},
                              {"overlayFiles", overlay.size()}}},
                   {"timing", {{"totalMilliseconds", milliseconds(started)},
                               {"planMilliseconds", std::chrono::duration<double, std::milli>(planned - started).count()},
                               {"candidateMilliseconds", std::chrono::duration<double, std::milli>(selected - planned).count()}}}};
    if (!syncReport.empty()) result["index"]["sync"] = syncReport;
    if (maxBytes) result["budget"] = {{"maxBytes", maxBytes}, {"usedBytes", bytes}, {"reached", budgetReached}};
    if (output == "files") {
        result["files"] = files;
        result["matches"] = Json::array();
    } else
        result["matches"] = matches;
    result["nextCursor"] = truncated ? Json(nextCursor) : Json(nullptr);
    return result;
}

Json CodeIndex::symbols(const Json &params, const std::atomic<bool> *cancellation) {
    auto &state = *state_;
    const auto started = Clock::now();
    const auto query = params.value("query", std::string{});
    if (query.size() > 256) throw NativeError(ErrorCode::InvalidParams, "query exceeds 256 bytes");
    const auto limit = size_t(std::clamp(params.value("limit", 50), 1, 500));
    const auto prefixes = pathFilters(params);
    const auto extensions = extensionFilters(params);
    std::vector<SymbolKind> kinds;
    for (const auto &name : stringList(params, "kinds", 16, 32)) {
        const auto kind = symbolKindFromName(name);
        if (!kind) throw NativeError(ErrorCode::InvalidParams, "Unknown symbol kind: " + name);
        kinds.push_back(*kind);
    }
    const bool declarations = params.value("includeDeclarations", true);
    if (query.empty() && prefixes.empty())
        throw NativeError(ErrorCode::InvalidParams, "Provide a query, or paths to list the symbols of a folder");
    if (query.empty() && kinds.empty())
        kinds = {SymbolKind::Class, SymbolKind::Struct, SymbolKind::Union, SymbolKind::Enum,
                 SymbolKind::Interface, SymbolKind::Function};
    const auto syncReport = state.sync(cancellation);
    const auto snapshot = state.current();
    if (!snapshot) throw NativeError(ErrorCode::BackendUnavailable, "Index is not ready");
    // "Scope::Name" or "Scope.Name" queries: the last component ranks names, earlier parts must appear in the scope.
    std::string nameQuery = query, scopeQuery;
    for (const auto *separator : {"::", "."}) {
        const auto split = query.rfind(separator);
        if (split != std::string::npos && split > 0) {
            nameQuery = query.substr(split + std::strlen(separator));
            scopeQuery = folded(query.substr(0, split));
            break;
        }
    }
    const auto foldedName = folded(nameQuery);
    struct Found {
        int score;
        std::string name, scope, path, detail;
        SymbolKind kind;
        uint32_t line, column, endLine;
        bool declaration;
    };
    std::vector<Found> found;
    std::atomic<size_t> scanned{0};
    // Returns the ranking score, or 0 when the symbol is filtered out.
    auto accept = [&](std::string_view name, std::string_view scope, SymbolKind kind, bool declaration) {
        if (!kinds.empty() && std::find(kinds.begin(), kinds.end(), kind) == kinds.end()) return 0;
        if (declaration && !declarations) return 0;
        const int score = symbolScore(name, nameQuery, foldedName);
        if (score <= 0) return 0;
        if (!scopeQuery.empty() && folded(scope).find(scopeQuery) == std::string::npos) return 0;
        return score * 2 + (declaration ? 0 : 1); // definitions rank before declarations
    };
    const auto &base = *snapshot->base;
    const auto &overlay = *snapshot->overlay;
    // Shards are scanned in parallel; each worker keeps its own matches.
    std::vector<std::vector<Found>> partial(base.shards.size());
    std::atomic<size_t> nextShard{0};
    auto work = [&] {
        size_t local = 0;
        for (size_t s; (s = nextShard.fetch_add(1)) < base.shards.size();) {
            if (cancellation && cancellation->load()) break;
            const auto &shard = base.shards[s];
            auto &out = partial[s];
            for (uint32_t id = 0; id < shard->files(); ++id) {
                const auto path = shard->path(id);
                if (!pathSelected(path, prefixes, extensions) || overlay.contains(std::string(path))) continue;
                const auto &record = shard->file(id);
                local += record.symbolCount;
                for (uint32_t i = 0; i < record.symbolCount; ++i) {
                    const auto &symbol = shard->symbol(record.symbolBegin + i);
                    const auto name = shard->string(symbol.nameOffset, symbol.nameLength);
                    const auto scope = shard->string(symbol.scopeOffset, symbol.scopeLength);
                    const int score = accept(name, scope, SymbolKind(symbol.kind), symbol.flags & 1);
                    if (!score) continue;
                    out.push_back(Found{score, std::string(name), std::string(scope), std::string(path),
                                        std::string(shard->string(symbol.detailOffset, symbol.detailLength)),
                                        SymbolKind(symbol.kind), symbol.line, symbol.column, symbol.endLine,
                                        bool(symbol.flags & 1)});
                }
            }
        }
        scanned += local;
    };
    {
        std::vector<std::thread> pool;
        const auto threads = std::min<size_t>(state.threads(), std::max<size_t>(1, base.shards.size()));
        for (size_t t = 1; t < threads; ++t) pool.emplace_back(work);
        work();
        for (auto &thread : pool) thread.join();
    }
    if (cancellation && cancellation->load()) return {{"symbols", Json::array()}, {"cancelled", true}};
    for (auto &part : partial)
        for (auto &item : part) found.push_back(std::move(item));
    for (const auto &[path, data] : overlay) {
        if (data->deleted || !pathSelected(path, prefixes, extensions)) continue;
        for (const auto &symbol : data->symbols) {
            ++scanned;
            const int score = accept(symbol.name, symbol.scope, symbol.kind, symbol.declaration);
            if (score)
                found.push_back(Found{score, symbol.name, symbol.scope, path, symbol.detail, symbol.kind,
                                      symbol.line, symbol.column, symbol.endLine, symbol.declaration});
        }
    }
    const auto total = found.size();
    const auto count = std::min(limit, found.size());
    std::partial_sort(found.begin(), found.begin() + long(count), found.end(), [](const Found &a, const Found &b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.path != b.path) return a.path < b.path;
        return a.line < b.line;
    });
    Json rows = Json::array();
    for (size_t i = 0; i < count; ++i) {
        const auto &f = found[i];
        Json row = {{"name", utf8(f.name)}, {"kind", scannedSymbolKindName(f.kind)}, {"path", utf8(f.path)}, {"line", f.line},
                    {"column", f.column}, {"endLine", f.endLine}};
        if (!f.scope.empty()) row["scope"] = utf8(f.scope);
        if (!f.detail.empty()) row["detail"] = utf8(f.detail);
        if (f.declaration) row["declaration"] = true;
        rows.push_back(std::move(row));
    }
    Json result = {{"symbols", rows},
                   {"total", total},
                   {"truncated", total > count},
                   {"engine", "index"},
                   {"source", "native-structural-scanner"},
                   {"scannedSymbols", scanned.load()},
                   {"index", {{"generation", base.generation}, {"syncedRevision", snapshot->syncedRevision}}},
                   {"timing", {{"totalMilliseconds", milliseconds(started)}}}};
    if (!syncReport.empty()) result["index"]["sync"] = syncReport;
    return result;
}

Json readNumberedLines(const std::string &path, std::string_view text, size_t first, size_t count) {
    std::vector<size_t> starts{0};
    for (size_t p = text.find('\n'); p != std::string_view::npos; p = text.find('\n', p + 1)) starts.push_back(p + 1);
    const size_t total = starts.size() - (!text.empty() && text.back() == '\n' ? 1 : 0);
    first = std::max<size_t>(1, first);
    if (first > std::max<size_t>(total, 1))
        throw NativeError(ErrorCode::InvalidParams, "line is past the end of the file (" + std::to_string(total) + " lines)");
    const size_t last = std::min(total, first + count - 1);
    std::string numbered;
    for (size_t n = first; n <= last; ++n) {
        const auto begin = starts[n - 1];
        const auto stop = n < starts.size() ? starts[n] - 1 : text.size();
        auto value = text.substr(begin, stop - begin);
        if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
        const bool cut = value.size() > 2000;
        if (cut) value = value.substr(0, 2000);
        numbered += std::to_string(n) + ": " + utf8(value) + (cut ? "..." : "") + "\n";
    }
    // Declarations that contain or start inside the range: where these lines live.
    Json declarations = Json::array();
    for (const auto &symbol : scanSymbols(text, sourceLanguageForPath(path))) {
        const bool body = symbolKindIsContainer(symbol.kind) || symbol.kind == SymbolKind::Function ||
                          symbol.kind == SymbolKind::Method || symbol.kind == SymbolKind::Constructor ||
                          symbol.kind == SymbolKind::Destructor;
        if (!body || symbol.endLine < first || symbol.line > last || declarations.size() >= 64) continue;
        declarations.push_back({{"name", utf8(qualified(symbol.scope, symbol.name))}, {"kind", scannedSymbolKindName(symbol.kind)},
                                {"line", symbol.line}, {"endLine", symbol.endLine}});
    }
    return {{"path", path}, {"startLine", first}, {"endLine", last}, {"totalLines", total},
            {"text", numbered}, {"declarations", declarations},
            {"nextLine", last < total ? Json(last + 1) : Json(nullptr)}};
}
Json attributeScanLines(const std::vector<std::string> &queries, bool regex, bool caseSensitive,
                        const std::vector<std::tuple<std::string, uint32_t, std::string>> &lines, size_t limitPerQuery) {
    Json results = Json::array();
    for (const auto &query : queries) {
        const Matcher matcher(query, regex, caseSensitive);
        pcre2_match_data *data = matcher.data();
        Json matches = Json::array();
        bool truncated = false;
        for (const auto &[path, number, text] : lines) {
            std::string_view line = text;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.remove_suffix(1);
            matcher.line(line, [&](size_t from, size_t) {
                if (matches.size() >= limitPerQuery) {
                    truncated = true;
                    return false;
                }
                matches.push_back({{"path", utf8(path)}, {"line", number}, {"column", from + 1}, {"text", preview(line, from)}});
                return true;
            }, data);
            if (truncated) break;
        }
        if (data) pcre2_match_data_free(data);
        results.push_back({{"query", query}, {"matches", matches}, {"truncated", truncated}});
    }
    return results;
}
Json outlineText(const std::string &path, std::string_view text, size_t limit) {
    const auto language = sourceLanguageForPath(path);
    auto symbols = scanSymbols(text, language);
    Json rows = Json::array(), counts = Json::object();
    for (size_t i = 0; i < symbols.size(); ++i) {
        const auto &symbol = symbols[i];
        const std::string kind = scannedSymbolKindName(symbol.kind);
        counts[kind] = counts.value(kind, 0) + 1;
        if (rows.size() >= limit) continue;
        Json row = {{"id", i}, {"name", utf8(symbol.name)}, {"kind", kind}, {"line", symbol.line}, {"column", symbol.column},
                    {"endLine", symbol.endLine}};
        if (symbol.parent >= 0) row["parent"] = symbol.parent;
        if (!symbol.scope.empty()) row["scope"] = utf8(symbol.scope);
        if (!symbol.detail.empty()) row["detail"] = utf8(symbol.detail);
        if (symbol.declaration) row["declaration"] = true;
        rows.push_back(std::move(row));
    }
    Json includes = Json::array();
    for (const auto &[line, target] : scanIncludes(text, language)) {
        if (includes.size() >= 256) break;
        includes.push_back({{"line", line}, {"target", utf8(target)}});
    }
    const auto lines = std::count(text.begin(), text.end(), '\n') + (text.empty() || text.back() == '\n' ? 0 : 1);
    const char *names[] = {"none", "c-family", "csharp", "python"};
    return {{"path", path},
            {"language", names[size_t(language)]},
            {"lines", lines},
            {"bytes", text.size()},
            {"symbols", rows},
            {"symbolCount", symbols.size()},
            {"truncated", symbols.size() > rows.size()},
            {"kinds", counts},
            {"includes", includes},
            {"source", "native-structural-scanner"}};
}
} // namespace cycode

#include "cycode/Recovery.h"
#include "cycode/NativeTrace.h"
#include <string_view>
#include "cycode/DocumentReference.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace cycode {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
uint64_t nowMs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
}
bool samePath(const fs::path &a, const fs::path &b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
bool contains(const fs::path &root, const fs::path &file) {
    auto b = file.begin();
    for (auto a = root.begin(); a != root.end(); ++a, ++b)
        if (b == file.end() || !samePath(*a, *b))
            return false;
    return true;
}
void ordinary(const fs::path &path) {
    if (!samePath(fs::absolute(path).lexically_normal(), fs::weakly_canonical(path)))
        throw std::runtime_error("Recovery paths cannot be links or redirected directories");
}
bool sessionId(const std::string &text) {
    return text.starts_with("session-") && text.size() == 40 &&
           std::all_of(text.begin() + 8, text.end(),
                       [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool digits(const std::string &text) {
    return !text.empty() && text.size() <= 20 &&
           std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}
bool bufferId(const std::string &text) {
    return text.starts_with("buffer-") && digits(text.substr(7));
}
std::string uniqueSession() {
    std::ostringstream value;
    value << "session-" << std::hex << std::setfill('0');
    std::random_device random;
    for (int i = 0; i < 4; ++i)
        value << std::setw(8) << uint32_t(random());
    return value.str();
}
class Lease {
  public:
    explicit Lease(const fs::path &file, bool create = false) {
        ordinary(file);
#ifdef _WIN32
        handle_ = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              create ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
#else
        handle_ = ::open(file.c_str(), O_RDWR | (create ? (O_CREAT | O_EXCL) : 0), 0600);
        if (handle_ >= 0 && ::flock(handle_, LOCK_EX | LOCK_NB) != 0) {
            ::close(handle_);
            handle_ = -1;
        }
#endif
    }
    ~Lease() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
#else
        if (handle_ >= 0)
            ::close(handle_);
#endif
    }
    bool acquired() const {
#ifdef _WIN32
        return handle_ != INVALID_HANDLE_VALUE;
#else
        return handle_ >= 0;
#endif
    }

  private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int handle_ = -1;
#endif
};
fs::path payload(const fs::path &folder, const std::string &name, const std::string &id, const char *kind) {
    const std::string prefix = std::string(kind) + "-" + id + "-";
    if (!name.starts_with(prefix) || !name.ends_with(".utf8") ||
        !digits(name.substr(prefix.size(), name.size() - prefix.size() - 5)))
        throw std::runtime_error("Invalid recovery payload reference");
    const auto file = folder / utf8Path(name);
    ordinary(file);
    return file;
}
Json loadIndex(const fs::path &folder, std::string *baseline = nullptr) {
    ordinary(folder);
    ordinary(folder / "session.json");
    const auto raw = readTextFileBytes(folder / "session.json");
    if (raw.size() > 1024 * 1024)
        throw std::runtime_error("Recovery index exceeds 1 MiB");
    if (baseline)
        *baseline = raw;
    auto index = Json::parse(raw);
    if (index.at("schemaVersion") != 1 || index.at("session") != pathUtf8(folder.filename()) ||
        !index.at("buffers").is_array() || index.at("buffers").size() > RecoveryStore::maxDocuments)
        throw std::runtime_error("Invalid recovery index");
    std::set<std::string> ids, paths;
    for (const auto &record : index.at("buffers")) {
        const std::string id = record.at("id"), path = record.at("path"),
                          checkpoint = record.at("checkpoint");
        if (!bufferId(id) || !ids.insert(id).second || path.empty() || path.size() > 4096 ||
            !paths.insert(path).second || !checkpoint.starts_with(id + "-") ||
            !digits(checkpoint.substr(id.size() + 1)) || checkpoint.size() > 64 ||
            !record.at("revision").is_number_unsigned() || record.at("revision").get<uint64_t>() < 1 ||
            !record.at("updatedAt").is_number_unsigned() || !record.at("bytes").is_number_unsigned() ||
            record.at("bytes").get<size_t>() > maxTextFileBytes ||
            !record.at("baselineBytes").is_number_unsigned() ||
            record.at("baselineBytes").get<size_t>() > maxTextFileBytes)
            throw std::runtime_error("Invalid recovery document metadata");
        for (const auto *flag : {"bom", "crlf", "crOnly", "existed"})
            if (!record.at(flag).is_boolean())
                throw std::runtime_error("Invalid recovery encoding flags");
        payload(folder, record.at("textFile"), id, "text");
        payload(folder, record.at("baselineFile"), id, "base");
        if (!record.at("textHash").is_string() || !record.at("baselineHash").is_string())
            throw std::runtime_error("Invalid recovery checksum");
    }
    return index;
}
Json summary(const Json &record, const std::string &session, bool active) {
    Json value = record;
    for (const auto *field :
         {"textFile", "baselineFile", "textHash", "baselineHash", "bom", "crlf", "crOnly", "existed"})
        value.erase(field);
    value["session"] = session;
    value["active"] = active;
    return value;
}
void removePayload(const fs::path &folder, const Json &record) {
    for (const auto &[field, kind] : {std::pair{"textFile", "text"}, std::pair{"baselineFile", "base"}}) {
        const auto file = payload(folder, record.at(field), record.at("id"), kind);
        NativeTraceScope trace(std::string_view(kind)=="text" ? NativeTraceEvent::RecoveryRemoveText : NativeTraceEvent::RecoveryRemoveBaseline);
        std::error_code ec;
        const bool removed=fs::remove(file, ec);
        trace.finish(ec ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                     static_cast<std::uint64_t>(ec.value()));
    }
}
} // namespace
struct RecoveryStore::State {
    const fs::path root, base;
    const std::string session = uniqueSession();
    std::unique_ptr<Lease> lease;
    Json index = {
        {"schemaVersion", 1}, {"session", session}, {"createdAt", nowMs()}, {"buffers", Json::array()}};
    std::optional<std::string> baseline;
    uint64_t generation = 0, nextId = 1;
    mutable std::mutex mutex;
    explicit State(fs::path path) : root(fs::canonical(path)), base(root / ".cycode/recovery") {}
    fs::path safeBase(bool create = false) const {
        if (!fs::is_directory(root))
            throw std::runtime_error("Recovery workspace is unavailable");
        ordinary(root);
        ordinary(root / ".cycode");
        ordinary(base);
        if (create) {
            fs::create_directory(root / ".cycode");
            fs::create_directory(base);
            ordinary(base);
        }
        return base;
    }
    fs::path path(const std::string &name) const {
        if (name.starts_with("untitled:")) {
            if (!isUntitledDocument(name)) throw std::runtime_error("Invalid untitled recovery reference");
            return {}; // Validation only: drafts have no project file.
        }
        const auto input = utf8Path(name);
        if (input.is_absolute())
            throw std::runtime_error("Recovery documents must be workspace-relative");
        const auto full = fs::weakly_canonical(root / input);
        if (!contains(root, full) || contains(base, full) || full == root)
            throw std::runtime_error("Invalid recovery document path");
        return full;
    }
    fs::path folder() {
        if (!lease) {
            const auto directory = safeBase(true) / session;
            if (!fs::create_directory(directory))
                throw std::runtime_error("Recovery session already exists");
            ordinary(directory);
            auto next = std::make_unique<Lease>(directory / "session.lock", true);
            if (!next->acquired())
                throw std::runtime_error("Cannot lock recovery session");
            lease = std::move(next);
        }
        const auto directory = safeBase() / session;
        ordinary(directory);
        return directory;
    }
    void commit(const Json &next) {
        NativeTraceScope trace(NativeTraceEvent::RecoveryIndexCommit);
        const auto file = folder() / "session.json";
        ordinary(file);
        const auto bytes = next.dump() + "\n";
        if (bytes.size() > 1024 * 1024)
            throw std::runtime_error("Recovery index quota exceeded");
        writeTextFileAtomic(file, bytes, baseline);
        baseline = bytes;
        index = next;
    }
    ~State() {
        NativeTraceScope trace(NativeTraceEvent::RecoveryStoreShutdown);
        if (lease && index.at("buffers").empty()) {
            try {
                const auto directory = folder();
                std::error_code ec;
                ordinary(directory / "session.json");
                fs::remove(directory / "session.json", ec);
                lease.reset();
                ordinary(directory / "session.lock");
                fs::remove(directory / "session.lock", ec);
                fs::remove(directory, ec);
            } catch (...) {
            }
        } else {
            // Observe the existing no-cleanup branch; dirty checkpoints stay owned.
            trace.finish(NativeTraceResult::Skipped);
        }
    }
};
RecoveryStore::RecoveryStore(fs::path root) : state_(std::make_unique<State>(std::move(root))) {}
RecoveryStore::~RecoveryStore() = default;
std::string RecoveryStore::fingerprint(const std::string &value) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ostringstream result;
    result << "fnv1a64-" << std::hex << std::setw(16) << std::setfill('0') << hash << '-' << std::dec
           << value.size();
    return result.str();
}
uint64_t RecoveryStore::savedRevision(const std::string &path) const {
    std::lock_guard lock(state_->mutex);
    for (const auto &record : state_->index.at("buffers"))
        if (record.at("path") == path)
            return record.at("revision");
    return 0;
}
void RecoveryStore::checkpoint(const Snapshot &snapshot) {
    NativeTraceScope trace(NativeTraceEvent::RecoveryCheckpoint);
    std::lock_guard lock(state_->mutex);
    state_->path(snapshot.path);
    if (isUntitledDocument(snapshot.path) && (snapshot.existed || !snapshot.disk.empty()))
        throw std::runtime_error("Untitled recovery cannot contain a disk baseline");
    validateEditorText(snapshot.text);
    if (snapshot.disk.size() > maxTextFileBytes || !snapshot.revision)
        throw std::runtime_error("Invalid recovery snapshot");
    auto disk = snapshot.disk;
    if (snapshot.bom && disk.starts_with("\xef\xbb\xbf"))
        disk.erase(0, 3);
    validateEditorText(disk);
    auto next = state_->index;
    auto &buffers = next.at("buffers");
    Json old;
    size_t total = 0;
    for (const auto &record : buffers)
        if (record.at("path") == snapshot.path)
            old = record;
        else
            total += record.at("bytes").get<size_t>() + record.at("baselineBytes").get<size_t>();
    if ((old.is_null() && buffers.size() >= maxDocuments) ||
        total + snapshot.text.size() + snapshot.disk.size() > maxBytes)
        throw std::runtime_error("Recovery quota exceeded (128 documents / 128 MiB)");
    const auto directory = state_->folder();
    const auto id =
        old.is_null() ? "buffer-" + std::to_string(state_->nextId++) : old.at("id").get<std::string>();
    const auto generation = std::to_string(++state_->generation);
    const auto textFile = "text-" + id + "-" + generation + ".utf8";
    const auto baselineHash = fingerprint(snapshot.disk);
    const bool reuse = !old.is_null() && old.at("baselineHash") == baselineHash;
    const auto baselineFile =
        reuse ? old.at("baselineFile").get<std::string>() : "base-" + id + "-" + generation + ".utf8";
    std::vector<fs::path> created;
    try {
        const auto text = payload(directory, textFile, id, "text");
        writeTextFileAtomic(text, snapshot.text, std::nullopt);
        created.push_back(text);
        if (!reuse) {
            const auto base = payload(directory, baselineFile, id, "base");
            writeTextFileAtomic(base, snapshot.disk, std::nullopt);
            created.push_back(base);
        }
        Json record = {{"id", id},
                       {"path", snapshot.path},
                       {"checkpoint", id + "-" + generation},
                       {"revision", snapshot.revision},
                       {"updatedAt", nowMs()},
                       {"bytes", snapshot.text.size()},
                       {"baselineBytes", snapshot.disk.size()},
                       {"textFile", textFile},
                       {"baselineFile", baselineFile},
                       {"textHash", fingerprint(snapshot.text)},
                       {"baselineHash", baselineHash},
                       {"bom", snapshot.bom},
                       {"crlf", snapshot.crlf},
                       {"crOnly", snapshot.crOnly},
                       {"existed", snapshot.existed}};
        for (auto it = buffers.begin(); it != buffers.end();)
            if (it->at("path") == snapshot.path)
                it = buffers.erase(it);
            else
                ++it;
        buffers.push_back(record);
        state_->commit(next);
    } catch (...) {
        for (const auto &file : created) {
            std::error_code ec;
            fs::remove(file, ec);
        }
        throw;
    }
    if (!old.is_null()) {
        // Diagnostic one-shot boundary; normal execution returns immediately.
        // The new checkpoint is committed, and all old payloads still exist here.
        pauseNativeRecoveryCleanupForDiagnosis();
        std::error_code ec;
        {
            NativeTraceScope trace(NativeTraceEvent::RecoveryRemoveText);
            const bool removed=fs::remove(payload(directory, old.at("textFile"), id, "text"), ec);
            trace.finish(ec ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                         static_cast<std::uint64_t>(ec.value()));
        }
        if (!reuse) {
            NativeTraceScope trace(NativeTraceEvent::RecoveryRemoveBaseline);
            const bool removed=fs::remove(payload(directory, old.at("baselineFile"), id, "base"), ec);
            trace.finish(ec ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                         static_cast<std::uint64_t>(ec.value()));
        }
    }
}
void RecoveryStore::erase(const std::string &path) {
    NativeTraceScope trace(NativeTraceEvent::RecoveryErase);
    std::lock_guard lock(state_->mutex);
    auto next = state_->index;
    auto &buffers = next.at("buffers");
    for (auto it = buffers.begin(); it != buffers.end(); ++it)
        if (it->at("path") == path) {
            const auto old = *it;
            buffers.erase(it);
            state_->commit(next);
            removePayload(state_->folder(), old);
            return;
        }
}
void RecoveryStore::retain(const std::set<std::string> &paths) {
    std::lock_guard lock(state_->mutex);
    auto next = state_->index;
    std::vector<Json> removed;
    for (auto it = next.at("buffers").begin(); it != next.at("buffers").end();) {
        if (!paths.contains(it->at("path"))) {
            removed.push_back(*it);
            it = next.at("buffers").erase(it);
        } else
            ++it;
    }
    if (removed.empty())
        return;
    NativeTraceScope trace(NativeTraceEvent::RecoveryRetain);
    state_->commit(next);
    for (const auto &record : removed)
        removePayload(state_->folder(), record);
}
RecoveryStore::Json RecoveryStore::status() const {
    std::lock_guard lock(state_->mutex);
    size_t bytes = 0;
    uint64_t updated = 0;
    for (const auto &record : state_->index.at("buffers")) {
        bytes += record.at("bytes").get<size_t>() + record.at("baselineBytes").get<size_t>();
        updated = std::max(updated, record.at("updatedAt").get<uint64_t>());
    }
    return {{"session", state_->session},
            {"documents", state_->index.at("buffers").size()},
            {"bytes", bytes},
            {"lastCheckpoint", updated},
            {"maxDocuments", maxDocuments},
            {"maxBytes", maxBytes}};
}
RecoveryStore::Json RecoveryStore::list(size_t limit) const {
    if (limit < 1 || limit > 1024)
        throw std::runtime_error("Invalid recovery list limit");
    Json items = Json::array(), errors = Json::array();
    bool truncated = false;
    try {
        const auto base = state_->safeBase();
        if (!fs::is_directory(base))
            return {{"items", items}, {"errors", errors}, {"truncated", false}};
        size_t count = 0;
        for (const auto &entry : fs::directory_iterator(base)) {
            if (++count > 512) {
                truncated = true;
                break;
            }
            const auto session = pathUtf8(entry.path().filename());
            if (!sessionId(session))
                continue;
            try {
                ordinary(entry.path());
                if (!fs::is_directory(entry.path()) || !fs::exists(entry.path() / "session.json"))
                    continue;
                Lease lease(entry.path() / "session.lock");
                const auto index = loadIndex(entry.path());
                for (const auto &record : index.at("buffers")) {
                    state_->path(record.at("path"));
                    items.push_back(summary(record, session, !lease.acquired()));
                    const auto newer = [](const auto &a, const auto &b) {
                        return a.at("updatedAt") > b.at("updatedAt");
                    };
                    std::push_heap(items.begin(), items.end(), newer);
                    if (items.size() > limit) {
                        truncated = true;
                        std::pop_heap(items.begin(), items.end(), newer);
                        items.erase(std::prev(items.end()));
                    }
                }
            } catch (const std::exception &e) {
                errors.push_back({{"session", session}, {"error", e.what()}});
            }
        }
    } catch (const std::exception &e) {
        errors.push_back({{"error", e.what()}});
    }
    std::sort(items.begin(), items.end(),
              [](const auto &a, const auto &b) { return a.at("updatedAt") > b.at("updatedAt"); });
    return {{"items", items}, {"errors", errors}, {"truncated", truncated}};
}
RecoveryStore::Json RecoveryStore::inspect(const std::string &session, const std::string &id,
                                           const std::string &checkpoint,
                                           const std::function<Json(const Entry &)> &action, bool consume) {
    if (!sessionId(session) || !bufferId(id))
        throw std::runtime_error("Invalid recovery entry reference");
    const auto directory = state_->safeBase() / session;
    ordinary(directory);
    Lease lease(directory / "session.lock");
    if (!lease.acquired())
        throw std::runtime_error("Recovery session is active or inaccessible");
    std::string baseline;
    auto index = loadIndex(directory, &baseline);
    for (auto it = index.at("buffers").begin(); it != index.at("buffers").end(); ++it)
        if (it->at("id") == id) {
            const auto record = *it;
            if (record.at("checkpoint") != checkpoint)
                throw std::runtime_error("RevisionConflict: recovery checkpoint changed");
            state_->path(record.at("path"));
            Snapshot snapshot;
            snapshot.path = record.at("path");
            snapshot.revision = record.at("revision");
            snapshot.text = readTextFileBytes(payload(directory, record.at("textFile"), id, "text"));
            snapshot.disk = readTextFileBytes(payload(directory, record.at("baselineFile"), id, "base"));
            if (snapshot.text.size() != record.at("bytes").get<size_t>() ||
                snapshot.disk.size() != record.at("baselineBytes").get<size_t>() ||
                fingerprint(snapshot.text) != record.at("textHash").get<std::string>() ||
                fingerprint(snapshot.disk) != record.at("baselineHash").get<std::string>())
                throw std::runtime_error("Recovery payload checksum mismatch");
            validateEditorText(snapshot.text);
            if (normalizeEditorText(snapshot.text) != snapshot.text)
                throw std::runtime_error("Recovery buffer is not normalized UTF-8");
            snapshot.bom = record.at("bom");
            snapshot.crlf = record.at("crlf");
            snapshot.crOnly = record.at("crOnly");
            snapshot.existed = record.at("existed");
            if (isUntitledDocument(snapshot.path) && (snapshot.existed || !snapshot.disk.empty()))
                throw std::runtime_error("Invalid untitled recovery baseline");
            auto disk = snapshot.disk;
            if (snapshot.bom && disk.starts_with("\xef\xbb\xbf"))
                disk.erase(0, 3);
            validateEditorText(disk);
            auto result = action({snapshot, summary(record, session, false)});
            if (consume && result.value("checkpointProtected", false)) {
                try {
                    index.at("buffers").erase(it);
                    ordinary(directory / "session.json");
                    {
                        NativeTraceScope trace(NativeTraceEvent::RecoveryIndexCommit);
                        writeTextFileAtomic(directory / "session.json", index.dump() + "\n", baseline);
                    }
                    removePayload(directory, record);
                    result["sourceRetained"] = false;
                } catch (const std::exception &e) {
                    result["sourceRetained"] = true;
                    result["warning"] = e.what();
                }
            } else if (consume)
                result["sourceRetained"] = true;
            return result;
        }
    throw std::runtime_error("Recovery checkpoint is unavailable");
}
RecoveryStore::Json RecoveryStore::discard(const std::string &session, const std::string &id,
                                           const std::string &checkpoint) {
    if (!sessionId(session) || !bufferId(id))
        throw std::runtime_error("Invalid recovery entry reference");
    const auto directory = state_->safeBase() / session;
    ordinary(directory);
    Lease lease(directory / "session.lock");
    if (!lease.acquired())
        throw std::runtime_error("Recovery session is active or inaccessible");
    std::string baseline;
    auto index = loadIndex(directory, &baseline);
    for (auto it = index.at("buffers").begin(); it != index.at("buffers").end(); ++it)
        if (it->at("id") == id) {
            const auto record = *it;
            if (record.at("checkpoint") != checkpoint)
                throw std::runtime_error("RevisionConflict: recovery checkpoint changed");
            state_->path(record.at("path"));
            index.at("buffers").erase(it);
            {
                NativeTraceScope trace(NativeTraceEvent::RecoveryIndexCommit);
                writeTextFileAtomic(directory / "session.json", index.dump() + "\n", baseline);
            }
            removePayload(directory, record);
            return {{"discarded", true}, {"sourceRetained", false}};
        }
    throw std::runtime_error("Recovery checkpoint is unavailable");
}
} // namespace cycode

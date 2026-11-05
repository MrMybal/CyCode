#include "cycode/WorkspaceMonitor.h"
#include "cycode/Paths.h"
#include <array>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cycode {
namespace {
bool ignoreFile(const std::filesystem::path &path) {
    auto name = pathUtf8(path.filename());
#ifdef _WIN32
    for (auto &c : name)
        c = char(std::tolower(static_cast<unsigned char>(c)));
#endif
    return name == ".gitignore" || name == ".ignore";
}
bool ignored(const std::filesystem::path &path) {
    for (const auto &part : path) {
        auto name = pathUtf8(part);
#ifdef _WIN32
        for (auto &c : name)
            c = char(std::tolower(static_cast<unsigned char>(c)));
#endif
        if (name == ".git" || name == "runtime" || name == ".cycode" || name == "data")
            return true;
    }
    return false;
}
} // namespace
struct WorkspaceMonitor::State {
    const std::filesystem::path root;
    mutable std::mutex mutex;
    std::condition_variable initialized;
    uint64_t revision = 1, indexRevision = 1;
    std::string backend = "polling", error;
    bool started = false;
    std::jthread worker;
    explicit State(std::filesystem::path path) : root(std::move(path)) {
        worker = std::jthread([this](std::stop_token stop) {
            try {
#ifdef _WIN32
                if (native(stop))
                    return;
#endif
                polling(stop);
            } catch (const std::exception &e) {
                std::lock_guard lock(mutex);
                error = e.what();
                started = true;
                initialized.notify_all();
            }
        });
        std::unique_lock lock(mutex);
        initialized.wait(lock, [&] { return started; });
    }
    ~State() {
        worker.request_stop();
        if (worker.joinable())
            worker.join();
    }
    void signal(bool structural) {
        std::lock_guard lock(mutex);
        ++revision;
        if (structural)
            ++indexRevision;
    }
    void ready(const char *name) {
        std::lock_guard lock(mutex);
        backend = name;
        started = true;
        initialized.notify_all();
    }
#ifdef _WIN32
    bool native(std::stop_token stop) {
        HANDLE directory = CreateFileW(
            root.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (directory == INVALID_HANDLE_VALUE) {
            std::lock_guard lock(mutex);
            error = "Directory notifications unavailable; using polling";
            return false;
        }
        std::unique_ptr<void, decltype(&CloseHandle)> ownedDirectory(directory, CloseHandle);
        HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!event) {
            return false;
        }
        std::unique_ptr<void, decltype(&CloseHandle)> ownedEvent(event, CloseHandle);
        alignas(DWORD) std::array<unsigned char, 60 * 1024> buffer{};
        bool failed = false;
        {
            std::stop_callback stopped(stop, [event] { SetEvent(event); });
            while (!stop.stop_requested()) {
                OVERLAPPED operation{};
                operation.hEvent = event;
                ResetEvent(event);
                if (!ReadDirectoryChangesW(directory, buffer.data(), DWORD(buffer.size()), TRUE,
                                           FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                               FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                           nullptr, &operation, nullptr)) {
                    failed = true;
                    break;
                }
                ready("ReadDirectoryChangesW");
                DWORD wait = WAIT_TIMEOUT;
                while (!stop.stop_requested() && (wait = WaitForSingleObject(event, 100)) == WAIT_TIMEOUT) {
                }
                if (stop.stop_requested() || wait != WAIT_OBJECT_0) {
                    CancelIoEx(directory, &operation);
                    DWORD ignoredBytes = 0;
                    GetOverlappedResult(directory, &operation, &ignoredBytes, TRUE);
                    failed = !stop.stop_requested();
                    break;
                }
                DWORD bytes = 0;
                if (!GetOverlappedResult(directory, &operation, &bytes, FALSE)) {
                    failed = true;
                    break;
                }
                if (!bytes) {
                    signal(true);
                    continue;
                } // Overflow: force a full reconciliation.
                bool changed = false, structural = false;
                for (size_t offset = 0; offset + sizeof(FILE_NOTIFY_INFORMATION) <= bytes;) {
                    const auto *entry =
                        reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(buffer.data() + offset);
                    constexpr auto header = offsetof(FILE_NOTIFY_INFORMATION, FileName);
                    if (entry->FileNameLength % sizeof(wchar_t) ||
                        offset + header + entry->FileNameLength > bytes) {
                        changed = structural = true;
                        break;
                    }
                    const auto path = std::filesystem::path(
                        std::wstring(entry->FileName, entry->FileNameLength / sizeof(wchar_t)));
                    if (!ignored(path)) {
                        changed = true;
                        structural |= entry->Action != FILE_ACTION_MODIFIED || ignoreFile(path);
                    }
                    if (!entry->NextEntryOffset)
                        break;
                    if (entry->NextEntryOffset < header || offset + entry->NextEntryOffset >= bytes) {
                        changed = structural = true;
                        break;
                    }
                    offset += entry->NextEntryOffset;
                }
                if (changed)
                    signal(structural);
            }
        }
        if (failed) {
            std::lock_guard lock(mutex);
            error = "Directory notifications interrupted; using polling";
        }
        return !failed;
    }
#endif
    struct Stamp {
        std::filesystem::file_time_type time;
        uintmax_t size = 0;
        bool directory = false;
        bool operator==(const Stamp &) const = default;
    };
    std::map<std::string, Stamp> scan(std::stop_token stop) {
        std::map<std::string, Stamp> result;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator
                 it(root, std::filesystem::directory_options::skip_permission_denied, ec),
             end;
             it != end && !stop.stop_requested(); it.increment(ec)) {
            const auto relative = it->path().lexically_relative(root);
            if (ignored(relative) || it->is_symlink(ec)) {
                if (it->is_directory(ec))
                    it.disable_recursion_pending();
                continue;
            }
            if (result.size() >= 200000 || it.depth() >= 64) {
                it.disable_recursion_pending();
                continue;
            }
            const bool directory = it->is_directory(ec);
            const auto time = it->last_write_time(ec);
            if (ec) {
                ec.clear();
                continue;
            }
            uintmax_t size = 0;
            if (!directory && it->is_regular_file(ec))
                size = it->file_size(ec);
            if (!ec)
                result.emplace(pathUtf8(relative), Stamp{time, size, directory});
            else
                ec.clear();
        }
        return result;
    }
    void polling(std::stop_token stop) {
        auto previous = scan(stop);
        ready("polling");
        while (!stop.stop_requested()) {
            for (int i = 0; i < 10 && !stop.stop_requested(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (stop.stop_requested())
                break;
            auto next = scan(stop);
            bool structural = next.size() != previous.size();
            if (!structural) {
                auto a = previous.begin(), b = next.begin();
                for (; a != previous.end(); ++a, ++b)
                    if (a->first != b->first || (a->second != b->second && ignoreFile(utf8Path(a->first)))) {
                        structural = true;
                        break;
                    }
            }
            if (next != previous)
                signal(structural);
            previous = std::move(next);
        }
    }
};
WorkspaceMonitor::WorkspaceMonitor(std::filesystem::path root)
    : state_(std::make_unique<State>(std::move(root))) {}
WorkspaceMonitor::~WorkspaceMonitor() = default;
WorkspaceMonitor::Snapshot WorkspaceMonitor::snapshot() const {
    std::lock_guard lock(state_->mutex);
    return {state_->revision, state_->indexRevision, state_->backend, state_->error};
}
void WorkspaceMonitor::invalidate() { state_->signal(true); }
} // namespace cycode

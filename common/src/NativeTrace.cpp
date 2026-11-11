#include "cycode/NativeTrace.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <mutex>
#include <thread>
namespace cycode {
namespace {
std::atomic<bool> enabled{false};
enum class CleanupPauseState { Idle, Armed, Consumed };
std::atomic<CleanupPauseState> recoveryCleanupPauseState{CleanupPauseState::Idle};
std::atomic<std::uint64_t> nextSpan{1};
thread_local std::uint64_t currentSpan = 0;
std::mutex outputMutex;
std::size_t writtenBytes = 0;
std::uint64_t sequence = 0;
bool limitWritten = false;
const char *label(NativeTraceEvent event) noexcept {
    switch (event) {
    case NativeTraceEvent::DocumentSave: return "document.save";
    case NativeTraceEvent::DocumentReplace: return "document.replace";
    case NativeTraceEvent::RecoveryRetire: return "recovery.retire";
    case NativeTraceEvent::RecoveryCheckpoint: return "recovery.checkpoint";
    case NativeTraceEvent::RecoveryErase: return "recovery.erase";
    case NativeTraceEvent::RecoveryRetain: return "recovery.retain";
    case NativeTraceEvent::RecoveryIndexCommit: return "recovery.index.commit";
    case NativeTraceEvent::RecoveryRemoveText: return "recovery.payload.text.remove";
    case NativeTraceEvent::RecoveryRemoveBaseline: return "recovery.payload.baseline.remove";
    case NativeTraceEvent::AtomicWrite: return "file.atomic.write";
    case NativeTraceEvent::TemporaryCreate: return "file.temporary.create";
    case NativeTraceEvent::TemporaryWrite: return "file.temporary.write";
    case NativeTraceEvent::TemporaryFlush: return "file.temporary.flush";
    case NativeTraceEvent::TemporaryClose: return "file.temporary.close";
    case NativeTraceEvent::ReplaceExisting: return "file.replace.existing";
    case NativeTraceEvent::PublishNew: return "file.publish.new";
    case NativeTraceEvent::TemporaryCleanup: return "file.temporary.cleanup";
    case NativeTraceEvent::TemporaryRetained: return "file.temporary.retained";
    case NativeTraceEvent::ServiceDrain: return "service.drain";
    case NativeTraceEvent::EngineShutdown: return "engine.shutdown";
    case NativeTraceEvent::RecoveryScan: return "recovery.scan";
    case NativeTraceEvent::RecoveryStoreShutdown: return "recovery.store.shutdown";
    case NativeTraceEvent::RecoveryCleanupPause: return "recovery.cleanup.pause";
    }
    return "trace.unknown";
}
const char *label(NativeTraceResult result) noexcept {
    switch (result) {
    case NativeTraceResult::Returned: return "returned";
    case NativeTraceResult::Success: return "success";
    case NativeTraceResult::Failure: return "failure";
    case NativeTraceResult::Missing: return "missing";
    case NativeTraceResult::Skipped: return "skipped";
    }
    return "returned";
}
void emit(const char *event, const char *phase, const char *outcome,
          std::uint64_t id, std::uint64_t parent, std::uint64_t error) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    try {
        std::lock_guard lock(outputMutex);
        if (!enabled.load(std::memory_order_relaxed)) return;
        const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        char line[512];
        auto render = [&] {
            return std::snprintf(line, sizeof(line),
                "{\"format\":\"cycode-native-trace/1\",\"seq\":%llu,\"utcMicros\":%llu,"
                "\"span\":%llu,\"parent\":%llu,\"event\":\"%s\",\"phase\":\"%s\","
                "\"outcome\":\"%s\",\"systemError\":%llu}\n",
                static_cast<unsigned long long>(sequence + 1),
                static_cast<unsigned long long>(timestamp), static_cast<unsigned long long>(id),
                static_cast<unsigned long long>(parent), event, phase, outcome,
                static_cast<unsigned long long>(error));
        };
        int count = render();
        if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(line)) {
            enabled.store(false, std::memory_order_relaxed); return;
        }
        // Leave enough space for one final fixed limit record, never a partial JSON line.
        if (writtenBytes + static_cast<std::size_t>(count) > maximumNativeTraceBytes - sizeof(line)) {
            if (limitWritten) return;
            limitWritten = true;
            event = "trace.limit"; phase = "limit"; outcome = "truncated";
            id = 0; parent = 0; error = 0;
            count = render();
        }
        ++sequence;
        const bool okay = count > 0 && std::fwrite(line, 1, static_cast<std::size_t>(count), stderr) ==
            static_cast<std::size_t>(count) && std::fflush(stderr) == 0;
        if (count > 0) writtenBytes += static_cast<std::size_t>(count);
        if (!okay || limitWritten) enabled.store(false, std::memory_order_relaxed);
    } catch (...) {
        // Diagnostic failure cannot unwind or change a document/recovery transaction.
        enabled.store(false, std::memory_order_relaxed);
    }
}
} // namespace
void enableNativeDiagnosticTrace() noexcept {
    // Do not reset counters/caps if the host enables tracing more than once.
    try {
        std::lock_guard lock(outputMutex);
        if (!limitWritten && writtenBytes < maximumNativeTraceBytes)
            enabled.store(true, std::memory_order_relaxed);
    } catch (...) {}
}
void armNativeRecoveryCleanupPause() noexcept {
    if (enabled.load(std::memory_order_relaxed)) {
        auto expected = CleanupPauseState::Idle;
        recoveryCleanupPauseState.compare_exchange_strong(expected, CleanupPauseState::Armed,
                                                         std::memory_order_relaxed);
    }
}
void pauseNativeRecoveryCleanupForDiagnosis() noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    auto expected = CleanupPauseState::Armed;
    if (!recoveryCleanupPauseState.compare_exchange_strong(expected, CleanupPauseState::Consumed,
                                                         std::memory_order_relaxed))
        return;
    NativeTraceScope trace(NativeTraceEvent::RecoveryCleanupPause);
    try {
        std::this_thread::sleep_for(std::chrono::seconds(nativeRecoveryCleanupPauseSeconds));
        trace.finish(NativeTraceResult::Success);
    } catch (...) {
        // Diagnostic failure must not cancel or change the committed transaction.
        trace.finish(NativeTraceResult::Failure);
    }
}
NativeTraceScope::NativeTraceScope(NativeTraceEvent event) noexcept : event_(event) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    exceptions_ = std::uncaught_exceptions();
    id_ = nextSpan.fetch_add(1, std::memory_order_relaxed);
    parent_ = currentSpan;
    currentSpan = id_;
    emit(label(event_), "begin", "begin", id_, parent_, 0);
}
NativeTraceScope::~NativeTraceScope() {
    if (!id_) return;
    emit(label(event_), "end", std::uncaught_exceptions() > exceptions_ ? "exception" : label(result_),
        id_, parent_, error_);
    currentSpan = parent_;
}
} // namespace cycode

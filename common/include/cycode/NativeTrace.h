#pragma once
#include <cstddef>
#include <cstdint>
namespace cycode {
// Explicit host opt-in. Fixed event labels only: no paths, text or credentials.
// stderr is separate from the native/MCP stdout protocols. One bounded sink per process.
enum class NativeTraceEvent {
    DocumentSave, DocumentReplace, RecoveryRetire, RecoveryCheckpoint,
    RecoveryErase, RecoveryRetain, RecoveryIndexCommit, RecoveryRemoveText,
    RecoveryRemoveBaseline, AtomicWrite, TemporaryCreate, TemporaryWrite,
    TemporaryFlush, TemporaryClose, ReplaceExisting, PublishNew, TemporaryCleanup,
    TemporaryRetained, ServiceDrain, EngineShutdown, RecoveryScan, RecoveryStoreShutdown, RecoveryCleanupPause
};
enum class NativeTraceResult { Returned, Success, Failure, Missing, Skipped };
inline constexpr std::size_t maximumNativeTraceBytes = 64 * 1024;
void enableNativeDiagnosticTrace() noexcept;
// Explicit CLI diagnostic only: once per process, after a refreshed index is
// committed and before obsolete payload cleanup. Does not disable cleanup.
inline constexpr unsigned nativeRecoveryCleanupPauseSeconds = 15;
void armNativeRecoveryCleanupPause() noexcept;
void pauseNativeRecoveryCleanupForDiagnosis() noexcept;
class NativeTraceScope {
  public:
    explicit NativeTraceScope(NativeTraceEvent) noexcept;
    ~NativeTraceScope();
    NativeTraceScope(const NativeTraceScope &) = delete;
    NativeTraceScope &operator=(const NativeTraceScope &) = delete;
    void finish(NativeTraceResult result, std::uint64_t systemError = 0) noexcept {
        result_ = result;
        error_ = systemError;
    }
  private:
    NativeTraceEvent event_;
    NativeTraceResult result_ = NativeTraceResult::Returned;
    std::uint64_t id_ = 0, parent_ = 0, error_ = 0;
    int exceptions_ = 0;
};
} // namespace cycode

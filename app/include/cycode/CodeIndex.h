#pragma once
#include "cycode/SymbolScanner.h"
#include "cycode/WorkspaceMonitor.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace cycode {
// Persistent project index shared by text and symbol searches.
//
// * Storage: CYCODE_INDEX_DIR (one folder per canonical workspace root) when set; otherwise
//   the project's own .cycode/index, created on the first build with a .gitignore ignoring
//   everything in it. When that folder cannot be created, the per-user cache is used
//   (%LOCALAPPDATA%\CyCode\index, $XDG_CACHE_HOME/cycode/index, ~/.cache/cycode/index).
//   .cycode is excluded from search, enumeration and monitoring; opening a project writes nothing.
// * Content: the ripgrep file list (same ignore rules as search.text), one trigram
//   posting list per immutable shard for candidate selection, and the declarations
//   found by SymbolScanner. Matches are always verified against current file bytes.
// * Freshness: WorkspaceMonitor change paths are applied before every query as an
//   in-memory overlay; a full stat reconciliation runs after a restart or when the
//   change log overflowed. Large overlays are compacted into new shard generations.
class CodeIndex {
  public:
    CodeIndex(std::filesystem::path root, WorkspaceMonitor &monitor);
    ~CodeIndex();
    CodeIndex(const CodeIndex &) = delete;
    CodeIndex &operator=(const CodeIndex &) = delete;
    void stop() noexcept;

    // Starts loading/building in the background if needed, then waits up to `wait`.
    // Returns true when a complete index generation is usable.
    bool ready(std::chrono::milliseconds wait, const std::atomic<bool> *cancellation = nullptr);
    bool disabled() const;
    // A generation was persisted for this workspace (reload is cheap compared to a build).
    bool persisted() const;
    // Starts reloading a persisted generation in the background. A first build starts too
    // when CYCODE_INDEX_AT_START=1 (long-lived agent services); otherwise it stays lazy.
    void warmStart();
    // CYCODE_INDEX_AT_START=1: this process is a long-lived agent service that builds its index
    // at launch. Interactive hosts (the IDE) leave it unset and never wait for a first build.
    static bool startsAtLaunch();
    // Brackets an interactive full scan: a running build pauses between files meanwhile.
    void foregroundBegin() noexcept;
    void foregroundEnd() noexcept;
    nlohmann::json status() const;
    nlohmann::json prepare(const nlohmann::json &params, const std::atomic<bool> *cancellation);

    // Unsaved buffers (path -> text) supersede disk content for these paths.
    nlohmann::json searchText(const nlohmann::json &params, const std::map<std::string, std::string> &unsaved,
                              const std::atomic<bool> *cancellation);
    nlohmann::json symbols(const nlohmann::json &params, const std::atomic<bool> *cancellation);

    struct State;

  private:
    std::shared_ptr<State> state_;
};

// Lexical outline of one text (live buffer or disk), with include list and kind counts.
nlohmann::json outlineText(const std::string &path, std::string_view text, size_t limit);
// Attributes lines found by one multi-pattern scan to each query with the index matcher.
// lines: (path, 1-based line, line text). Returns [{query, matches, truncated}] in query order.
nlohmann::json attributeScanLines(const std::vector<std::string> &queries, bool regex, bool caseSensitive,
                                  const std::vector<std::tuple<std::string, uint32_t, std::string>> &lines,
                                  size_t limitPerQuery);
// 1-based line range as "N: text" lines, with the declarations it crosses.
nlohmann::json readNumberedLines(const std::string &path, std::string_view text, size_t first, size_t count);
// Default storage folder for a workspace root (does not create it).
std::filesystem::path defaultCodeIndexDirectory(const std::filesystem::path &root);
// Default storage: the project's own .cycode/index (excluded from search, enumeration and monitoring).
std::filesystem::path projectCodeIndexDirectory(const std::filesystem::path &root);
} // namespace cycode

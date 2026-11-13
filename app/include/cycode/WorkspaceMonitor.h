#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace cycode {
// Worker-owned notifications. Consumers read counters; no UI/document callback runs here.
class WorkspaceMonitor {
  public:
    struct Snapshot {
        uint64_t revision = 1, indexRevision = 1;
        std::string backend, error;
    };
    explicit WorkspaceMonitor(std::filesystem::path);
    ~WorkspaceMonitor();
    WorkspaceMonitor(const WorkspaceMonitor &) = delete;
    WorkspaceMonitor &operator=(const WorkspaceMonitor &) = delete;
    Snapshot snapshot() const;
    void invalidate();
    // Workspace-relative paths ('/' separators) reported after `revision`. complete=false
    // when the bounded log cannot cover that range (overflow, polling restart, invalidate()):
    // consumers must then reconcile from a full enumeration.
    struct Changes {
        uint64_t revision = 0;
        bool complete = false;
        std::vector<std::string> paths;
    };
    Changes changesSince(uint64_t revision) const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace cycode

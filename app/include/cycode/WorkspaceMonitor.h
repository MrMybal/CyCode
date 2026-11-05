#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
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

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace cycode

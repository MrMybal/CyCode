#pragma once
#include "cycode/RelatedFiles.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace cycode {
struct WorkspaceFileSnapshot {
    nlohmann::json files=nlohmann::json::array();
    RelatedFileIndex related;
    uint64_t revision=0,generation=0;
    bool indexing=true,complete=false;
    std::string error;
};
// One owned ripgrep stream per Engine, preserving its real ignore semantics.
// Published sorted batches are immutable; generations prevent mixed page views.
class WorkspaceFileIndex {
  public:
    explicit WorkspaceFileIndex(std::filesystem::path root):root_(std::move(root)) {}
    ~WorkspaceFileIndex();
    void stop() noexcept;
    std::shared_ptr<const WorkspaceFileSnapshot> snapshot(uint64_t revision,bool refresh);
  private:
    void scan(uint64_t revision) noexcept;
    void publish(const nlohmann::json &,uint64_t,bool,const std::string &error={});
    std::filesystem::path root_;
    std::mutex lifecycle_,mutex_;
    std::condition_variable changed_;
    std::shared_ptr<const WorkspaceFileSnapshot> current_;
    uint64_t generation_=0;
    std::atomic<bool> stopped_{false},abortProcess_{false};
    std::thread worker_;
};
} // namespace cycode

#pragma once
#include <cycode/Automation.h>
#include <cycode/AtomicFile.h>
#include <cycode/NativeFileLease.h>
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>
#include <cymcp/changes.hpp>
#include <condition_variable>
#include <fstream>
#include <map>
#include <regex>

namespace cycode::automation {
Json readJson(const std::filesystem::path&, std::size_t maximum = 32 * 1024 * 1024);
void saveJson(const std::filesystem::path&, const Json&);
double timestamp();
bool terminal(const std::string&);
Json sdkSchema(const Json&);
void validateContract(const Json&, const Json&);
struct OperationError : cymcp::Error {
    Json details, native;
    explicit OperationError(const Json& e)
        : cymcp::Error(e.value("code", "OperationFailed"), e.dump()),
          details(e.value("details", Json::object())), native(e) {}
};
using DomainInvoker = std::function<Json(const std::string&, const Json&, const cymcp::CallContext&)>;
class Jobs {
    struct Entry {
        Json record;
        cymcp::CallContext context;
        bool cancellable = true;
    };
    std::filesystem::path root_;
    std::unique_ptr<NativeFileLease> lease_;
    std::map<std::string, std::shared_ptr<Entry>> jobs_;
    std::map<std::string, Json> sessions_, clients_;
    std::vector<Json> events_;
    std::uint64_t sequence_ = 0;
    std::mutex mutex_;
    std::condition_variable changed_;
    DomainInvoker invoke_;
    std::function<Json(const std::string&)> describe_;
    std::size_t workers_, queueCapacity_, diskLimit_, ramLimit_, running_ = 0, reservedRam_ = 0;
    std::filesystem::path importRoot_, exportRoot_;
    bool stopping_ = false;
    std::unique_ptr<cymcp::Executor> executor_;
    std::jthread timer_;
    void persist(const Entry&);
    void persistIdentities();
    void schedule(); // caller owns mutex_
    void execute(std::shared_ptr<Entry>);
    void emit(const std::string&, const std::string&, Json);
    std::shared_ptr<Entry> owned(const std::string&, const std::string&);
  public:
    Jobs(const Options&, DomainInvoker, std::function<Json(const std::string&)>);
    ~Jobs();
    Json submit(const std::string&, Json, const cymcp::CallContext&, Json options = Json::object());
    Json wait(const std::string&, const cymcp::CallContext&);
    Json call(const std::string&, const Json&, const cymcp::CallContext&);
    void stop();
};
// Suppresses nested durability wrappers while the SDK executes an already-issued job.
extern thread_local bool withinJob;
} // namespace cycode::automation

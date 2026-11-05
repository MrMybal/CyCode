#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
// A temporary source-breakpoint lease, never part of the saved configuration.
class RunToCursor {
  public:
    using Json = nlohmann::json;
    static Json plan(const Json &parameters, const Json &state, const Json &original);
    static Json verification(const Json &reply, size_t count);
    void begin(const Json &plan);
    void armed(const Json &reply);
    void continuing();
    void continued(const Json &result);
    void stopped(const Json &body);
    void resumed();
    void breakpointEvent(const Json &point, bool removed);
    void restoring(uint64_t expectedRevision);
    void restored(const Json &reply);
    void fail(const std::string &message);
    void ended();
    void reset();
    bool active() const;
    bool needsRestore() const;
    bool cleanupDue() const;
    bool stopConfirmed() const;
    Json original() const;
    std::string path() const;
    Json snapshot() const;

  private:
    Json plan_ = Json::object(), target_ = nullptr, targetHit_ = nullptr;
    Json continueResult_ = Json::object();
    Json deferred_ = Json::array();
    std::string status_ = "idle", message_, stopReason_;
    uint64_t revision_ = 0;
    bool active_ = false, restore_ = false, cleanup_ = false;
    bool stopped_ = false;
    size_t deferredBytes_ = 0;
};
} // namespace cycode

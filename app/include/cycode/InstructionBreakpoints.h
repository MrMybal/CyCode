#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>

namespace cycode {
// Session-only configuration; DAP I/O and synchronization belong to Debugger.
class InstructionBreakpoints {
  public:
    using Json = nlohmann::json;
    static Json plan(const Json &points, const Json &capabilities);
    static Json draft(const Json &active);
    void beginReplace(uint64_t expectedRevision);
    void complete(const Json &plan, const Json &reply);
    void fail();
    bool breakpointEvent(const Json &point, bool removed = false);
    void reset();
    Json snapshot() const;

  private:
    Json active_ = Json::array(), deferred_ = Json::array();
    uint64_t revision_ = 0;
    size_t deferredBytes_ = 0;
    bool synchronized_ = true, pending_ = false, deferredLost_ = false;
};
} // namespace cycode

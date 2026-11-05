#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace cycode {
// Worker-owned, UI-free DAP data breakpoint identities. Raw dataId never leaves
// this service's prepared wire plan. Armed breakpoints outlive inspection handles.
class DataBreakpoints {
  public:
    using Json = nlohmann::json;
    static Json arguments(const Json &parameters, const Json &capabilities, int frame);
    Json capture(const Json &arguments, const Json &reply, uint64_t session, uint64_t stop);
    Json prepare(const Json &points, const Json &capabilities, uint64_t session, uint64_t stop) const;
    void beginReplace(uint64_t expectedRevision);
    void complete(const Json &plan, const Json &reply);
    bool breakpointEvent(const Json &point, bool removed = false);
    void expire();
    void reset();
    Json snapshot() const;

  private:
    struct Candidate { Json publicInfo; std::string dataId; uint64_t session, stop; };
    std::map<std::string, Candidate> candidates_;
    Json active_ = Json::array();
    uint64_t serial_ = 0, revision_ = 0;
    size_t bytes_ = 0;
    bool synchronized_ = true;
};
} // namespace cycode

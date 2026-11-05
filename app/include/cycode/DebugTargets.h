#pragma once
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
// One bounded catalogue per family, scoped to the inspected pause/frame.
class DebugTargets {
  public:
    using Json = nlohmann::json;
    enum class Kind { stepIn, goTo };
    explicit DebugTargets(Kind kind = Kind::stepIn) : kind_(kind) {}
    static Json context(const Json &state, Kind kind = Kind::stepIn);
    static Json source(const Json &state, bool adapterData = false);
    static Json query(const Json &parameters, const Json &state, Kind kind = Kind::stepIn);
    static bool available(const Json &state, Kind kind = Kind::stepIn);
    static bool current(const Json &catalogue, const Json &state, Kind kind = Kind::stepIn);
    Json complete(const Json &parameters, const Json &reply, const Json &state,
                  const Json &queriedArguments = nullptr);
    Json arguments(const Json &parameters, const Json &state) const;
    void expire();
    Json snapshot() const;

  private:
    Kind kind_;
    uint64_t serial_ = 0;
    Json catalogue_ = Json::object(), sourceContext_ = Json::object();
    std::map<std::string, int> targets_;
};
} // namespace cycode

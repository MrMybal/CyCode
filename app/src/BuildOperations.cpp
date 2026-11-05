#include "cycode/BuildOperations.h"
#include <algorithm>
#include <set>

namespace cycode {
namespace {
std::string operationId(const nlohmann::json &parameters, bool required) {
    auto id = parameters.value("operationId", std::string{});
    if ((!id.empty() && (id.size() > 128 || !std::all_of(id.begin(), id.end(),
                                                         [](unsigned char c) {
                                                             return (c >= 'a' && c <= 'z') ||
                                                                    (c >= 'A' && c <= 'Z') ||
                                                                    (c >= '0' && c <= '9') || c == '-' ||
                                                                    c == '_' || c == '.';
                                                         }))) ||
        (required && id.empty()))
        throw std::runtime_error("Invalid build operation ID");
    return id;
}
} // namespace
bool BuildOperations::supports(const std::string &method) {
    static const std::set<std::string> methods = {
        "build.configure", "build.run",       "build.presetRun",  "msbuild.build",       "msbuild.rebuild",
        "msbuild.clean",   "msbuild.restore", "msbuild.evaluate", "msbuild.prepare",     "unreal.generate",
        "unreal.database", "unreal.build",    "unreal.prepare",   "unreal.automationRun"};
    return methods.contains(method);
}
bool BuildOperations::finish() {
    std::lock_guard state(stateMutex_);
    operationId_.clear();
    method_.clear();
    output_.finish();
    return cancellation_.load();
}
BuildOperations::Json BuildOperations::execute(const std::string &method, const Json &parameters,
                                               const Work &work) {
    if (!supports(method))
        throw std::runtime_error("Unsupported cancellable build operation");
    auto id = operationId(parameters, false);
    if (id.empty()) {
        static std::atomic<uint64_t> sequence{0};
        id = "native-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
             std::to_string(++sequence);
    }
    std::unique_lock operation(operationMutex_, std::try_to_lock);
    if (!operation.owns_lock())
        throw std::runtime_error("A native build is already running; wait or cancel it first");
    auto currentMethod = method;
    std::string stream;
    {
        std::lock_guard state(stateMutex_);
        cancellation_ = false;
        stream = output_.begin(method, id);
        operationId_.swap(id);
        method_.swap(currentMethod);
    }
    try {
        ProcessOutputScope outputScope(&cancellation_, output_.observer(stream));
        auto result = work(&cancellation_);
        const bool cancelled = finish();
        result["cancelled"] = result.value("cancelled", false) || cancelled;
        if ((result.at("cancelled").get<bool>() || result.value("truncated", false) ||
             result.value("timedOut", false)) &&
            result.value("exitCode", -1) == 0)
            result["exitCode"] = -1;
        return result;
    } catch (...) {
        const bool cancelled = finish();
        if (cancelled)
            throw std::runtime_error("BuildCancelled: build operation cancelled");
        throw;
    }
}
BuildOperations::Json BuildOperations::cancel(const Json &parameters) {
    const auto id = operationId(parameters, false);
    std::lock_guard state(stateMutex_);
    const bool requested = !operationId_.empty() && (id.empty() || id == operationId_);
    if (requested && !cancellation_.load()) {
        cancellation_ = true;
        output_.cancelling();
    }
    return {{"requested", requested}};
}
BuildOperations::Json BuildOperations::status() const {
    std::lock_guard state(stateMutex_);
    return {{"running", !operationId_.empty()},
            {"operationId", operationId_},
            {"method", method_},
            {"cancelling", !operationId_.empty() && cancellation_.load()}};
}
BuildOperations::Json BuildOperations::output(const Json &parameters) const {
    return output_.read(parameters);
}
} // namespace cycode

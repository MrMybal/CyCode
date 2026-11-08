#include "cycode/CoreService.h"
#include <random>
#include <sstream>

namespace cycode {
namespace {
std::string contextId() {
    std::random_device random;
    std::ostringstream out;
    for (unsigned i=0; i<4; ++i) out << std::hex << random();
    return out.str();
}
bool validId(const Json &id) {
    return id.is_null() || id.is_number_integer() || (id.is_string() && id.get_ref<const std::string &>().size() <= 256);
}
}
CoreService::CoreService(Engine &engine) : engine_(engine), context_(contextId()) {}
Json CoreService::status() const {
    return {{"protocol",protocol},{"version",CYCODE_VERSION},
        {"service",{{"state",draining() ? "draining" : "ready"},
                    {"uptimeMilliseconds",std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started_).count()}}},
        {"engine",{{"context",context_},{"state","available"},{"workspace",pathUtf8(engine_.workspace())}}},
        // Initialization is not semantic coverage. Preserve the backend's own
        // epochs, diagnostics/revisions and capabilities through language.status.
        {"analysis",{{"statusMethod","language.status"},{"projectIndexCompleteness","unknown"},
                     {"readyMeaning","backend-initialized-only"}}},
        {"limits",{{"requestBytes",maximumRequestBytes}}}};
}
Json CoreService::error(const std::string &code,const std::string &message,const Json &id) {
    return {{"protocol",protocol},{"id",id},{"error",{{"code",code},{"message",message}}}};
}
Json CoreService::respond(const Json &input) noexcept {
    Json id;
    try {
        if (!input.is_object()) return error("InvalidRequest","Expected a request object");
        if (input.contains("id")) {
            if (!validId(input.at("id"))) return error("InvalidRequest","Expected a scalar request id of at most 256 bytes");
            id=input.at("id");
        }
        if (input.contains("protocol") && input.at("protocol") != protocol)
            return error("UnsupportedProtocol","Expected cycode-core/1",id);
        if (!input.contains("method") || !input.at("method").is_string())
            return error("InvalidRequest","Expected a string method",id);
        const auto method=input.at("method").get<std::string>();
        if (method.empty() || method.size()>128) return error("InvalidRequest","Invalid method length",id);
        const auto params=input.value("params",Json::object());
        if (!params.is_object()) return error("InvalidParams","Expected an object for params",id);
        if (method=="service.status") return {{"protocol",protocol},{"id",id},{"result",status()}};
        if (draining() && method!="project.cancel" && method!="build.cancel" && method!="tests.cancel")
            return error("ServiceStopping","The project context is draining",id);
        return {{"protocol",protocol},{"id",id},{"result",engine_.dispatch(method,params)}};
    } catch (const Json::exception &e) { return error("InvalidParams",e.what(),id); }
      catch (const std::filesystem::filesystem_error &e) { return error("FileSystemError",e.what(),id); }
      catch (const std::invalid_argument &e) { return error("InvalidParams",e.what(),id); }
      // Existing Engine operations use runtime_error for revision conflicts,
      // backend failures and unsupported methods. Do not guess a semantic code
      // by matching prose; callers retain the original detail until typed owners migrate.
      catch (const std::exception &e) { return error("OperationFailed",e.what(),id); }
      catch (...) { return error("InternalError","Unexpected native failure",id); }
}
void CoreService::beginShutdown() noexcept {
    if (draining_.exchange(true)) return;
    for (const auto *method : {"build.cancel","tests.cancel","project.cancel"})
        try { engine_.dispatch(method); } catch (...) {}
}
} // namespace cycode

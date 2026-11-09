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
        {"limits",{{"requestBytes",maximumRequestBytes},{"responseBytes",maximumNativeResponseBytes}}}};
}
Json CoreService::error(const std::string &code,const std::string &message,const Json &id) {
    return nativeErrorResponse(code,message,id);
}
Json CoreService::respond(const Json &input) noexcept {
    return respondNative(input,[this](const std::string &method,const Json &params)->Json {
        if(method=="service.status")return status();
        if(draining() && method!="project.cancel" && method!="build.cancel" && method!="tests.cancel" && method!="analysis.cancel")
            throw NativeError(ErrorCode::ServiceStopping,"The project context is draining");
        return engine_.dispatch(method,params);
    });
}
void CoreService::beginShutdown() noexcept {
    if (draining_.exchange(true)) return;
    for (const auto *method : {"build.cancel","tests.cancel","project.cancel","analysis.cancel"})
        try { engine_.dispatch(method); } catch (...) {}
}
} // namespace cycode

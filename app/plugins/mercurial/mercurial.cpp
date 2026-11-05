#include "MercurialClient.h"
#include "cycode/PluginAPI.h"
#include "cycode/Tools.h"
#include <cstddef>
#include <memory>

namespace {
struct Instance {
    std::string output;
};
const char *dispatch(void *instance, const char *input) noexcept {
    auto &self = *static_cast<Instance *>(instance);
    using cycode::mercurial::Json;
    try {
        const auto request = Json::parse(input);
        cycode::mercurial::Client client(cycode::toolPath("hg"), [](const auto &argv, const auto &cwd,
                                                                    int seconds, const auto &environment) {
            return cycode::runProcess(argv, cwd, std::chrono::seconds(seconds), 512 * 1024, nullptr,
                                      environment);
        });
        self.output = client.run(request.at("action"), request.at("parameters"))
                          .dump(-1, ' ', false, Json::error_handler_t::replace);
        if (self.output.size() > 4 * 1024 * 1024)
            throw std::runtime_error("Mercurial response exceeds 4 MiB");
    } catch (const std::exception &e) {
        self.output = Json({{"error", e.what()}}).dump(-1, ' ', false, Json::error_handler_t::replace);
    } catch (...) {
        self.output = "{\"error\":\"Mercurial extension failed\"}";
    }
    return self.output.c_str();
}
} // namespace
extern "C" CYCODE_PLUGIN_EXPORT int cycode_plugin_init_v2(const CyCodeHostV2 *host,
                                                          void **instance) noexcept {
    if (!host || !instance || host->abi_version != CYCODE_PLUGIN_ABI_V2 ||
        host->struct_size < offsetof(CyCodeHostV2, features) || !host->contribute)
        return 1;
    *instance = nullptr;
    try {
        auto self = std::make_unique<Instance>();
        if (host->contribute(host->context, "vcs", cycode::mercurial::descriptor().dump().c_str(), dispatch))
            return 1;
        *instance = self.release();
        return 0;
    } catch (...) {
        return 1;
    }
}
extern "C" CYCODE_PLUGIN_EXPORT void cycode_plugin_shutdown_v2(void *instance) noexcept {
    delete static_cast<Instance *>(instance);
}

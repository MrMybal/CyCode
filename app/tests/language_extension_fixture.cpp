#include "cycode/PluginAPI.h"
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
#include <nlohmann/json.hpp>
extern "C" CYCODE_PLUGIN_EXPORT int cycode_plugin_init_v2(const CyCodeHostV2 *host, void **instance) {
    *instance = nullptr;
    std::string executable;
#ifdef _WIN32
    wchar_t value[32768]{};
    const auto length = GetEnvironmentVariableW(L"CYCODE_TEST_LANGUAGE_SERVER", value, 32768);
    if (length && length < 32768) {
        const auto bytes = WideCharToMultiByte(CP_UTF8, 0, value, int(length), nullptr, 0, nullptr, nullptr);
        executable.resize(bytes);
        WideCharToMultiByte(CP_UTF8, 0, value, int(length), executable.data(), bytes, nullptr, nullptr);
    }
#else
    if (const auto value = std::getenv("CYCODE_TEST_LANGUAGE_SERVER"))
        executable = value;
#endif
    if (executable.empty())
        return 1;
    const nlohmann::json descriptor = {
        {"id", "fixture.language.cpp"},
        {"title", "Plugin C++ dialect"},
        {"arguments", {executable, "--background-index", "--log=error"}},
        {"languages",
         nlohmann::json::array({{{"id", "cpp"}, {"extensions", {".cycpp"}}, {"filenames", {"CyDialect"}}}})}};
    return host->contribute(host->context, "languageServer", descriptor.dump().c_str(), nullptr);
}

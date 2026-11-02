#pragma once
#include <stdint.h>
#ifdef _WIN32
#define CYCODE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define CYCODE_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define CYCODE_PLUGIN_ABI 1u
/* UTF-8 JSON. Results are owned by the plugin and copied before the next call.
 * Callbacks must not throw across this C boundary. Host invokes serially.
 * Plugins are trusted native code, not sandboxed. Never pass STL/ImGui objects. */
typedef const char *(*CyCodeCommand)(const char *input_json);
typedef struct CyCodeHostV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    void (*add_command)(void *, const char *id, const char *title, CyCodeCommand);
    void (*add_vcs_provider)(void *, const char *id, const char *title, const char *capabilities_json,
                             CyCodeCommand dispatch);
} CyCodeHostV1;
typedef int (*CyCodePluginInit)(const CyCodeHostV1 *);
/* Required export: int cycode_plugin_init(const CyCodeHostV1* host). */
#ifdef __cplusplus
}
#endif

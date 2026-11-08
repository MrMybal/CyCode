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
#define CYCODE_PLUGIN_ABI_V2 2u
#define CYCODE_HOST_FEATURE_ACTIVATION_V2 UINT64_C(1)
#define CYCODE_HOST_FEATURE_EVENTS_V2 UINT64_C(2)
#define CYCODE_HOST_FEATURE_EVENT_SUBSCRIPTIONS_V2 UINT64_C(4)
#define CYCODE_HOST_FEATURE_DEBUG_ADAPTERS_V2 UINT64_C(8)
#define CYCODE_HOST_FEATURE_SNIPPETS_V2 UINT64_C(16)
/* EVENTS exposes the read-only events.read journal. EVENT_SUBSCRIPTIONS permits
 * contribute("events", {id, types:["document.*"], maxBatch:32}, callback).
 * Host-owned serial callbacks receive subscribed/gap/events envelopes; the first
 * two are read-only snapshot handshakes. Normal delivery uses manifest permissions,
 * excludes the plugin's own host mutations and never passes ImGui/STL objects. */
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

/* ABI 2 preserves ABI 1. UTF-8 JSON only: no STL, ImGui objects or allocators.
 * contribute registers command/menu/panel/vcs/project/syntax/template/languageServer
 * descriptors during init. DEBUG_ADAPTERS_V2 also permits a declarative debugAdapter
 * (id/title/type/arguments/requests, optional launch/attach/environment/pidField),
 * no callback. Requires process.execute; the host owns stdio-DAP and all children.
 * template descriptors contain bounded text files/options and have no callback.
 * SNIPPETS_V2 permits contribute("snippets", {id,title,definitions,order}, NULL).
 * definitions uses the saved project snippet grammar; no file path or callback.
 * The host owns scopes, issuance, selection, revisions and edit transactions.
 * request runs during a callback on that same thread and returns host-owned
 * {ok,result,error} JSON, valid until the next request. Check ok before result. */
typedef const char *(*CyCodeCommandV2)(void *plugin_instance, const char *input_json);
typedef struct CyCodeHostV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    int (*contribute)(void *, const char *kind, const char *descriptor_json, CyCodeCommandV2);
    const char *(*request)(void *, const char *method, const char *parameters_json);
    /* Optional ABI2 tail. Check struct_size before reading. Older plugins keep
     * using the original prefix; older hosts do not supply these feature bits. */
    uint64_t features;
} CyCodeHostV2;
typedef int (*CyCodePluginInitV2)(const CyCodeHostV2 *, void **plugin_instance);
typedef void (*CyCodePluginShutdown)(void);
typedef void (*CyCodePluginShutdownV2)(void *plugin_instance);
/* Optional export: void cycode_plugin_active_v2(void*, uint32_t active).
 * With CYCODE_HOST_FEATURE_ACTIVATION_V2, init registers dormant state. The host
 * notifies active=1 after activation, active=0 after disable and before shutdown.
 * Notifications are serial, cannot veto activation, must not throw or call the
 * host, and must stop/join background work before returning from active=0.
 * Disabled libraries remain pinned. This is not a native-code sandbox. */
typedef void (*CyCodePluginActiveV2)(void *plugin_instance, uint32_t active);
/* ABI 2 exports cycode_plugin_init_v2; optional shutdown callbacks must
 * stop/join plugin-owned work without host calls. Libraries remain pinned until
 * host destruction, including while disabled; disabling removes contributions.
 * ABI 2 exports cycode_plugin_shutdown_v2(instance) to release per-host state.
 * Each host gets its own instance, including multiple workspaces in one process. */
#ifdef __cplusplus
}
#endif

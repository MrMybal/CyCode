---
id: plugin-api
title: Develop a native plugin
description: Use public C entry points and UTF-8 JSON contributions.
section: Work in CyCode
order: 280
parent: plugins
revision: 2
---

## Include and entry points {#contract}

Include `cycode/PluginAPI.h` from the installed SDK/include directory or the corresponding source distribution. Compile a compatible shared library. `CYCODE_PLUGIN_ABI` is 1; `CYCODE_PLUGIN_ABI_V2` is 2. ABI 2 adds instance ownership and richer contributions without changing ABI 1.

```cpp
int cycode_plugin_init_v2(const CyCodeHostV2* host, void** plugin_instance);
void cycode_plugin_shutdown_v2(void* plugin_instance);
void cycode_plugin_active_v2(void* plugin_instance, uint32_t active);
```

The init export is required for ABI 2; shutdown and activation callbacks are optional according to their contract. Check `abi_version`, `struct_size` and feature bits before reading optional fields.

`host` points to the current host contract. Write your per-host state to `*plugin_instance`; return 0 for successful initialisation and a nonzero value for failure. The host passes that instance to ABI 2 command, activation and shutdown callbacks. `active` is 1 after activation and 0 after disabling; activation and shutdown callbacks must not call the host.

ABI 1 uses `int cycode_plugin_init(const CyCodeHostV1* host)` and the optional `void cycode_plugin_shutdown(void)`. Its command callback is `const char* callback(const char* input_json)`; ABI 2 adds the instance parameter: `const char* callback(void* plugin_instance, const char* input_json)`. The host copies the plugin-owned result before the next callback.

## Contributions and calls {#contributions}

`host->contribute(context, kind, descriptor_json, callback)` registers supported descriptors during initialisation. `host->request(context, method, parameters_json)` returns host-owned `{ok,result,error}` JSON during an authorised callback; check `ok` before reading `result`.

`contribute` returns 0 on success and nonzero on failure. `kind` selects the descriptor family; `descriptor_json` contains that family's fields and `callback` handles applicable commands. Pass `host->context` as the first argument. `request` takes the advertised host method name and its JSON parameters; the result string is valid until the next host request.

JSON is UTF-8. Keep callback/state lifetimes valid and never throw across the C boundary. Copy returned strings before the next call invalidates them. Do not pass STL objects, ImGui objects or allocators through the ABI. Stop/join plugin work before deactivation/shutdown returns. Native libraries remain pinned until host destruction.

The host supports contributions for commands, menus, panels, version control, projects, syntax, templates and language servers. Optional event subscriptions and declarative debug adapters require their corresponding feature bits and permissions. A template or debug-adapter descriptor has no command callback. A menu command must refer to a command contributed by the same plugin.

```cpp
#include <cycode/PluginAPI.h>
#include <stddef.h>
static const char* hello(void*, const char*) {
    return "{\"message\":\"Hello\"}";
}
extern "C" CYCODE_PLUGIN_EXPORT
int cycode_plugin_init_v2(const CyCodeHostV2* host, void** instance) {
    if (!host || !instance || host->abi_version != CYCODE_PLUGIN_ABI_V2 ||
        host->struct_size < offsetof(CyCodeHostV2, features))
        return 1;
    *instance = nullptr;
    return host->contribute(host->context, "command",
        "{\"id\":\"hello\",\"title\":\"Hello\"}", hello);
}
```

This entry-point example also needs an application plugin manifest and a compatible shared-library build. See [extension management](doc:plugins#development). Native plugins are trusted code running inside the application.

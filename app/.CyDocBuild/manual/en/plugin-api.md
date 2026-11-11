---
id: plugin-api
title: Develop a native plugin
description: Use public C entry points and UTF-8 JSON contributions.
section: Work in CyCode
order: 280
parent: plugins
revision: 6
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

## Declarative snippet catalogues {#snippet-catalogues}

After the ordinary ABI2 guards, check `struct_size >= sizeof(CyCodeHostV2)` and
`host->features & CYCODE_HOST_FEATURE_SNIPPETS_V2`. During initialization, pass
kind `snippets`, the following UTF-8 JSON and `nullptr` as callback to contribute.
This example requires the manifest ID `example.code`:

```json
{
  "id": "example.code.snippets",
  "title": "Example snippets",
  "order": 20,
  "definitions": {
    "Wrap": { "prefix": "wrap", "scope": "cpp,hlsl,glsl", "body": "(${1:$TM_SELECTED_TEXT})$0" }
  }
}
```

The ID must use the plugin namespace. Optional title is one line / 160 bytes;
order defaults to 0 and ranges from -10000 to 10000. Only id/title/order/definitions
are accepted. Definitions are nonempty and use the saved snippet grammar; callbacks,
paths, owner overrides and when/context conditions are refused. Per catalogue:
256 KiB, 256 definitions and 8 KiB bodies. Registry: 32 catalogues / 1 MiB including
inactive registrations and host owner metadata. No additional contribution/query
permission is needed; applying a host plan still requires workspace.write.

The active snapshot is sorted by order/ID after workspace items. Source provenance
is retained, with 512 combined proposals and ordinary payload limits. Registry
changes invalidate pending issuance/preparation and desktop acceptance. Already
returned concrete plans retain normal apply guards. This is declarative insertion,
without provider callbacks or LSP queries, not isolation for other native code.
See [using snippets](doc:refactoring#plugin-snippets).

## Personal snippet preferences {#user-snippets}

The JSON host exports snippets.userRead and snippets.userSave for the fixed
embedding-configured personal store. Read never creates it. Save requires
settings.write; workspace.write alone is insufficient, and read-only callbacks
cannot save even with the setting permission. No path argument or C ABI layout
change is introduced. Use the revision from read, preserve errors, and request
new snippets after a change. A default Engine returns available=false.
See [personal snippets](doc:refactoring#user-snippets) and the generated
[read](doc:operation-snippets-user-read-snippets-user-read) /
[save](doc:operation-snippets-user-save-snippets-user-save) contracts.

## Capture selected source {#capture-snippet}

snippets.userDraft / SDK [snippets_user_draft](doc:operation-snippets-user-draft-snippets-user-draft)
exports one current workspace selection as a personal JSON draft. Pass path,
expectedRevision and ordered UTF-16 selection. It is a read-only host service,
including in read-only callbacks, with no file/lock creation or ABI layout change.
The result includes the original catalogue snapshot, draft, unique name and source
path/revision. Source/escaping/catalogue bounds are checked natively. Review the
draft and save separately using catalogue.revision and settings.write. See
[selected-code capture](doc:refactoring#capture-snippet).

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
        "{\"id\":\"example.plugin.hello\",\"title\":\"Hello\"}", hello);
}
```

This entry-point example needs a plugin manifest with ID `example.plugin` and a compatible shared-library build. Contribution IDs must start with that manifest ID and a dot. See [extension management](doc:plugins#development). Native plugins are trusted code running inside the application.

A [custom revision-control provider](doc:custom-vcs#provider) can expose backend-specific menus and actions.

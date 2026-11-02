# Native plugin SDK, ABI 1

Public contract: include/cycode/PluginAPI.h. The host loads manifests from its own
runtime/plugins directory at startup, never from an arbitrary opened project.
Plugin binaries are trusted native code and must be rebuilt per OS/architecture.
There is no marketplace, sandbox, live reload or dependency resolver yet.

The sample in plugins/sample is built with the IDE and contributes a working
Text statistics command. The Extensions panel and MCP plugins_invoke can run it.

Each manifest provides id, name, version, abi=1, library (local basename without
extension). Export cycode_plugin_init(const CyCodeHostV1*) and verify abi_version
and struct_size before using host callbacks. Return zero for success.

- add_command registers an ID, title and UTF-8 JSON callback.
- add_vcs_provider registers a provider ID, title, supported action names and callback.
- No std::string, exceptions, allocators or ImGui objects cross the C ABI.
- Callback output is plugin-owned and copied by the host before the next invocation.
- Host calls plugin functions serially and keeps the library loaded while callbacks exist.

VCS actions currently understood by the native service: status, diff, log, stage,
unstage and commit. Dispatch input is {"action":"status","parameters":{...}}.
All paths belong to the current workspace; providers must respect that boundary.
The VCS panel selects a registered provider and disables unsupported actions. MCP
vcs_* operations accept an optional provider ID, with Git as the default.
status returns {provider,repository,branch,files:[{status,path}],message}.
Other actions return {exitCode,output,timedOut,truncated}. Providers advertise only
the actions actually implemented. Unsupported actions fail explicitly.

Language packs, custom panels, project adapters and specialized shader/web editors
are planned extension points. ABI 1 currently implements command and VCS callbacks;
it does not pretend to ship those other extension APIs.

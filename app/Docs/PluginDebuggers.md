# Plugin-owned adapter definitions, host-owned DAP sessions

The independent `builtin.java` provider now uses JDT LS and a bounded host-owned
loopback DAP connection. It does not change the ABI2 stdio contribution contract;
plugins cannot register either reserved built-in ID. The adapter catalogue now
contains two built-ins plus up to 32 active contributions. Java launch setup,
runtime prerequisites and pending acceptance are in [Java debugging](JavaDebugging.md).

`builtin.java` now advertises launch and attach. Its host-owned `javaAttach`
profile field stores the JDWP endpoint independently of vendor options; other
adapters retain PID attach and do not receive this Java field in DAP payloads.
Saving keeps the existing workspace.write/process.execute checks; attaching keeps
process.execute. The C ABI and descriptor transports are unchanged. The Java
adapter can connect to explicit remote targets while its CyCode DAP socket stays
local; no plugin callback or Python service owns the target JVM.

ABI2 plugins can register stdio Debug Adapter Protocol adapters. The actual
Debugger service launches and owns their processes, handles DAP framing and
configuration, and supplies the existing breakpoints, stepping, stack, variables,
watches, source views and program terminals. GUI, native/plugin host requests and
MCP use this same service. An installed descriptor alone does not supply a missing
debugger executable or implement a language's debug engine.

## Registration

Check the optional ABI2 tail size and
`CYCODE_HOST_FEATURE_DEBUG_ADAPTERS_V2` before contributing during init:

```cpp
if (host->struct_size >= sizeof(CyCodeHostV2) &&
    (host->features & CYCODE_HOST_FEATURE_DEBUG_ADAPTERS_V2)) {
    host->contribute(host->context, "debugAdapter", R"({
        "id":"example.tools.debugger",
        "title":"Example debugger",
        "type":"example-dap",
        "arguments":["${pluginDir}/adapter", "--stdio"],
        "requests":["launch","attach"],
        "pidField":"processId",
        "environment":{"ADAPTER_MODE":"stdio"},
        "launch":{"console":"integratedTerminal"}
    })", nullptr);
}
```

The contribution is unconditional data: no callback, `when` or GUI context. Its
ID must use the plugin namespace. The manifest needs `process.execute`. Missing
permissions, unknown descriptor fields, duplicate IDs and malformed declarations
are rejected during registration. A failed init removes its partial contributions.
The original ABI2 prefix/layout and ABI1 remain compatible; the new feature is an
additional bit in the existing optional tail.

| Field | Contract |
| --- | --- |
| id/title/type | Required text. `type` is the adapter's DAP ID, also used during initialize; up to 128 ASCII letters/digits/underscore/dot/hyphen |
| arguments | Required argv array, 1–128 entries, each at most 8 KiB. First entry is an executable path/tool name; it is resolved when explicitly launching, without a shell |
| requests | Nonempty unique subset of launch/attach; defaults to launch only |
| pidField | Numeric attach PID key: processId (default) or pid |
| environment | Optional adapter-process environment overrides/removals (null); at most 64 entries / 16 KiB |
| launch/attach | Optional adapter-specific request defaults, only for declared requests; objects of at most 16 KiB |

The host adds the actual `plugin` owner to its internal snapshot. There are at most
32 contributed adapters and 64 KiB per descriptor. Options/defaults are bounded to
16 levels, 128 entries per collection, 8 KiB strings and 128-byte keys. Native
validation occurs before serialization of nested options.

Only argv and environment string values expand `${pluginDir}` and `${workspace}`.
Expansion is a single pass over the original text: inserted directory names are
literal, including spaces and any variable-like characters. Unknown variables in
those fields are errors. Launch/attach JSON strings remain literal. No project
configuration is evaluated and no adapter starts during registration/enumeration.
Adapters remain platform-specific tools; the manifest's existing OS/architecture
and host compatibility checks still apply.

## Selection, profiles and payloads

The Debugger's Launch configuration tab selects an **Adapter provider** and edits
its **Adapter options (JSON)**. IDs persist in the existing `.cycode/launch.json`
profile as `adapterId`; old profiles default to `builtin`, preserving the native
and .NET adapter selection. There is one saved profile per workspace, not a new
per-adapter catalogue. Profile edits affect the next launch; a running session
stays bound to its original adapter definition.

`debug.adapters` / MCP `debug_adapters` returns the registry revision and summaries
with id/title/type/plugin/requests. It includes `builtin`. It is read-only, starts
no process and reveals no adapter argv, environment or option defaults. It lists
active contributions, not proof that each executable is installed. `plugins.list`
also contains contributed summaries and advertises the `debugAdapters` feature.

`debug.configure`, `debug.launch` and `debug.attach` accept `adapterId` and
`adapterOptions`. Defaults from the selected launch/attach declaration are shallow
merged with saved/per-call options. The host supplies common fields last:
request/type/cwd, program/args/env on launch, stopOnEntry and the explicit attach
PID. `noDebug` is false. Options cannot override those fields or internal adapter
selection/transport fields. Nested option objects replace the corresponding
defaults rather than recursively merging them.

The common program/script field, when nonempty, is resolved relative to the
workspace and must name a real file; explicitly selected absolute programs remain
allowed. The working directory must remain inside the workspace. A contributed
adapter may use an empty program with its own options, such as a module name.
User argv boundaries, Unicode and environment removals are preserved. The DAP
request is capped at 64 KiB. The adapter's optional default exception filters are
configured before configurationDone; richer exception filter UI remains pending.
Native profile validation rejects unknown fields and validates nested options
and scalar collections before serialization; vendor-specific parameters belong
in `adapterOptions`.

The native worker uses initialize → launch/attach → initialized/configuration →
configurationDone through the existing DAP transport. Unsupported launch/attach
requests are rejected before process creation; unavailable providers disable the
corresponding GUI commands and show a warning. An absent executable produces an
installation/selection error. The host does not silently switch to a different
provider. Native/.NET runtime and custom executable controls apply to `builtin`;
contributed argv/environment belong to the registered definition.

Run without debugging still uses the existing native/.NET or project-task Run
services. Selecting a DAP provider does not replace those runners. Language-server
configuration and debugger selection remain independent.

## Permissions and lifetime

The Engine publishes one atomic active-plugin service snapshot for LSP and DAP.
The DAP owner stores immutable active definitions and its own registry epoch.
Identical registry refreshes do not invalidate queued launches. A queued contributed
launch is rejected if the registry changed before execution, including a rapid
disable/re-enable. Changing unrelated adapters does not revoke an already active
session whose own definition remains identical.

Removing/disabling/changing the active adapter marks it revoked and wakes its
owner, including when its channel is already idle/exited. Revocation interrupts
request/configuration waits. The worker disconnects and stops the adapter, closes
owned program terminals, invalidates frame/variable inspection and reports failure.
Launched debuggees are terminated; explicitly attached processes are detached and
remain alive. `adapterRevoked` exposes teardown in progress. Completion/failure of
launch requests is reported after owned cleanup; the service retains ownership
through termination. Unexpected adapter exit also cleans up launched targets.
Workspace shutdown stops DAP work before waiting for plugin event callbacks.

The selected profile ID is retained after disable, so re-enabling restores an
explicit choice rather than rewriting settings. Existing plugin libraries remain
pinned until their normal reverse-order shutdown. DAP transport/child ownership
belongs to the host, not the plugin's activation or shutdown callback.

Exported ABI2 host services now include:

- `debug.adapters`: read-only summaries, no extra manifest permission.
- `debug.profile/status`: require `process.execute`; allowed during read-only
  callbacks since they inspect cached data, which can include sensitive values.
- `debug.configure/breakpoints`: require both `process.execute` and
  `workspace.write`; denied during read-only callbacks/snapshot handshakes.
- `debug.launch/attach/stop/pause/continue/next/stepIn/stepOut/evaluate/variables/
  frame/thread/source`: require `process.execute` and a normal mutating callback.

MCP retains the actual SDK permission checks for these operations. Native plugins
and selected debug engines are trusted code with OS access; descriptor validation
is not process sandboxing. Expression evaluation and adapter-specific options may
execute target/debugger code. No arbitrary external filesystem writes are granted
by a catalogue read. Reverse runInTerminal requests use bounded existing terminal
services and a workspace-checked working directory, and are denied during teardown.

## Bundled example, boundaries and validation

The Unreal extension contributes `cycode.unreal.lldb` using the installed CodeLLDB
tool and the same native DAP worker, with numeric `pid` attachment and C++ source
language metadata. This replaces no UBT service and does not add Blueprint
visualizers, Live Coding or verified UnrealEditor debugging. Existing installed
CodeLLDB selection remains available through `builtin` too.

This addition supports stdio, one session per workspace and the implemented
native DAP request set. TCP-only engines need a real stdio bridge supplied by their
integration. Nested/child sessions, arbitrary reverse requests, custom DAP methods,
remote attach setup, exception-filter controls, advanced memory/data breakpoints
and adapter-specific UI are still separate work. Java/JS/browser/Python engines
are not automatically installed or validated by registering this API.

Windows x64/MSVC production application/core/CLI and bundled plugins compile with
tests disabled. Source assertions cover descriptor/option limits, variable
expansion, metadata privacy, PID mapping, GUI parameter validation, native registry
activation and official MCP permissions. Extended actual-LLDB acceptance covers
contributed launch/attach and revoke/terminate/detach behavior **when run**; it has
not been executed after the Kaspersky interruption. No new test executable or app
window was launched. Linux/macOS and real third-party-adapter acceptance remain
pending; see [Validation](Validation.md) and [Testing](Testing.md).

Protocol reference: [Microsoft's DAP schema](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

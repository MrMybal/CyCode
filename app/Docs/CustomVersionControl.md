# Adding a revision-control provider

CyCode's provider boundary is independent of Git, SVN, Perforce, Mercurial and
Diversion. The owner's future revision-control system can use that same boundary
once its protocol is defined; no implementation or protocol for it is invented
here. A provider does not have to imitate Git commands, staging or remotes.

Implement a native ABI2 plugin against [PluginAPI.h](../include/cycode/PluginAPI.h).
The bundled [Diversion adapter](../plugins/diversion/diversion.cpp) is a concrete
example of registering a `vcs` contribution and calling an existing CLI through
the shared native process owner. Its client/descriptor remains free of CyUI,
ImGui and Engine. An adapter can instead use its own SDK/service, retaining
application path, data, persistence and process ownership rules.

## Descriptor and result contract

Use an independent stable plugin ID, provider ID, contribution/action IDs and local
library basename. Declare only implemented capabilities. For example, a system
that publishes changes and manages file leases can describe its own actions:

```json
{
  "id":"example.revisions.provider", "title":"Example revisions",
  "capabilities":["status","diff","log","publish","lease.acquire"],
  "actions":[
    {"id":"diff","title":"Working differences","scope":"repository","fields":[]},
    {"id":"log","title":"Revision history","scope":"repository","fields":[]},
    {"id":"publish","title":"Publish changes","scope":"repository",
     "fields":[{"id":"message","title":"Description","required":true}]},
    {"id":"lease.acquire","title":"Acquire file lease","scope":"file","fields":[]}
  ]
}
```

This is a documentation example, not a registered provider or simulated backend.
The host generates provider menus/forms and palette actions from the descriptor.
Labels require English/French translations; IDs remain stable and untranslated.
Fields are bounded text inputs with optional default/required. File actions receive
the active document path. Providers can contribute separate native model panels,
commands and event subscriptions for advanced workflows beyond these forms; see
[Plugins](Plugins.md) and [Events](Events.md) for the implemented limits.

Callbacks receive `{action, parameters}`. The host owns the injected absolute
`workspace` and workspace-relative `root`; validated file `path` stays within that
mapping. Validate again against the backend's own repository/client/workspace
identity. An independent ID/protocol must never cause an unimplemented operation
to fall back to Git. Return a nonempty JSON `error` on failure, or preserve the
native operation's `exitCode/output/timedOut/truncated/cancelled` metadata.

`status` returns at least:

```json
{"provider":"example.revisions.provider","repository":true,"branch":"main",
 "files":[{"status":"M","path":"Game/Source/Main.cpp"}],"message":""}
```

`path` values are workspace-relative, not backend IDs, URLs or temporary artifacts.
Additional backend metadata can be returned without inventing Git state.
`diff` returns a unified patch in `output`, consumed by the common CyUI Diff view;
`log` and `blame` return read-only Review text. Unsupported binary/conflict/history
representations require an explicit panel/contract rather than fabricated text.

## Routing, permissions and lifetime

Installed plugins are loaded from application extension locations. A project
selects a provider using revision-checked `.cycode/vcs.json` root mappings; the
deepest matching root drives native GUI/CLI/MCP routes. Explicit selection is
also available. The built-in default stays Git. An unavailable mapped provider
remains unavailable until reenabled/reinstalled or explicitly remapped.

Generic MCP access uses `vcs_provider_action`; its conservative permissions include
`plugins.execute`, `process.execute`, `workspace.write` and `vcs.write`. Typed Git
SDK methods keep their existing Git-only schemas and permission boundaries.
Recognized read actions are `status/inspect/diff/log/history/show/blame`. Other
action IDs are conservatively treated as mutations, including custom read-like
actions until a versioned access contract exists. They therefore also receive
dirty-buffer/disk-baseline guards and reconciliation. This limitation is explicit.

The versioned C ABI transports UTF-8 JSON and callbacks, without STL/ImGui objects
or cross-library allocators. Calls are serial, outputs are plugin-owned until the
next invocation and exceptions must be caught before returning across the ABI.
Activation/dependency/version/platform checks and disable/shutdown ownership apply
to VCS providers just as to other extensions. Native plugins are trusted code;
the ABI is not a process sandbox. Process-wide application data belongs to the
host, and backend history/Undo/persistence semantics belong to the backend.

## Acceptance for a future adapter

Specify read/write operations, repository identity, exact path encoding, conflict
states, authentication, background work and cancellation before exporting actions.
Use argv with bounded owned children for CLI integration; never shell-concatenate
project input. Validate complete metadata before a destructive or broad operation.
Document which commands change disk, publish remotely or schedule asynchronous
work. Do not imply that stopping a waiter cancels accepted server work.

Runner/ABI regressions should cover actual backend contracts, path identities,
partial/error responses, dirty-buffer protection, routing and lifecycle; real-client
and GUI acceptance must be recorded separately. Current local validation follows
[Testing](Testing.md): source checks and production builds only during the antivirus
interruption. Windows production compilation does not validate Linux/macOS.

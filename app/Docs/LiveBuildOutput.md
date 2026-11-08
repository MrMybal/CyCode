# Live native build output

Recognized source rows can now open existing workspace files through the shared
native build.location parser. Live project logs supply their recorded task
working directory; other/final logs use the workspace root. This is a guarded
navigation hint, not a current diagnostic revision. See
[Compiler locations](CompilerLocations.md) for formats and limits.

The Build panel has a **Live build output** section using CyUI Diagnostics and
the shared log severity rules. It polls at most four times per second while the
panel is visible, supports filtering/follow/copy, and shows the latest operation's
ID, method and running/finished state. Final GUI task output and diagnostics keep
their existing path; MCP returns the final result to its caller. Live messages
do not imply success or completion.

## Shared service and scope

`build.output` / SDK `build_output` / ABI2 read the same Engine-owned memory tail
without acquiring adapter, document or plugin execution locks. The operation
requires `process.execute`, because output may contain private project data.
It is read-only and has no workspace mutation group. Authorized SDK clients can
inspect output while another job builds; read-only plugin callbacks can read it
with this manifest permission. Trusted native plugin code is not OS-sandboxed.

Explicit CMake configure/build/test/package/workflow execution, MSBuild build/
rebuild/clean/restore and Unreal generation/database/build/prepare/UAT execution
opt in at their main process calls. Sequential phases share one tail in actual
pipe-read order. Stdout/stderr are merged by the existing pipe; their separate
identity and original interleaving cannot be reconstructed.

Tool discovery and structured MSBuild evaluation never opt in. Background Unreal
analysis has a separate cancellation owner and does not publish here. Cargo/npm/
Java/shader tasks now share the same journal implementation through the separate
project.output service and a Build panel source selector; see
[Project output](ProjectTaskOutput.md). Debugger terminals and arbitrary plugin
processes retain their services. Process ownership, deadlines and cancellation remain unchanged.

The observer is scoped to the synchronous build thread and exact cancellation
owner. Other threads/owners receive no observer. Callbacks check their captured
stream ID and active reservation, so late callbacks cannot enter the next build.
Journal failures do not cancel the build or release its reservation. Output from
internally buffered tools, real-time scheduling and progress percentages are not
promised. Failed starts may have no pipe output; inspect the final build response.

## Reading the tail

Start with `{}`. Responses contain `streamId`, `revision`, `changed`,
`operationId`, `method`, `running`, `cancelling`, `byteCount`, `droppedBytes`,
`incomplete`, `encoding` and `output`. Each build gets a distinct stream ID even
when a caller reuses an operation ID. The latest tail survives completion/failure
until the next explicit build or workspace destruction. An empty stream ID means
no such build. Observation IDs do not confer authorization.

Send both observed `streamId` and `revision` to avoid repeating an unchanged tail.
Equal observations return `changed:false` with empty output. Otherwise
`changed:true` supplies a **replacement of the whole display tail**, including
when a new build replaces the old one. It is not a chunk to append. Lifecycle and
cancellation also advance the revision. Poll at about 250 ms or slower and check
returned IDs rather than inferring identity from a reused operation ID.

The journal retains at most 256 KiB of raw bytes in bounded 8 KiB chunks.
`byteCount` counts observed original bytes; `droppedBytes` counts bytes removed
from the front. Tail overflow does not truncate or stop the build. `incomplete`
records a journal allocation failure, independently of the process outcome.
Original per-process output retains its existing 8 MiB cap.

Display text repairs malformed encoding and NUL/control bytes, preserving CR/LF/tab;
`encoding` is `utf8-repaired`. Polling/retention can split a character at an edge;
a later replacement refreshes that display. Byte counters are not Unicode
character counts or offsets in returned text. ANSI escapes are not interpreted;
Malformed Windows UTF-8 lines use the system OEM code page; other malformed bytes
get U+FFFD. Valid UTF-8 lines remain unchanged. UTF-16 or arbitrary/mixed encodings
are not inferred. Original ProcessResult/protocol bytes keep their representation;
final user-facing JSON log fields use the same text conversion. No archive, temporary SDK artifact, MCP push, argument
dump, automatic export or clipboard write is added.

## Validation boundary

Windows production compilation, MSVC /Zs and actual installed SDK/schema checks
are separate from runtime acceptance. Existing opt-in build fixtures specify
retention, revisions, scope isolation, late callbacks, text repair, failure/
cancellation and CMake pipe integration. They are syntax-checked only, neither
built nor executed during the Kaspersky suspension. Real compiler/Unreal timing,
concurrent SDK/plugin readers, GUI filtering/follow/themes/DPI and Linux/macOS
execution remain unverified. See [Validation](Validation.md).

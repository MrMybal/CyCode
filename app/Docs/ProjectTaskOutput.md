# Project task status and live output

Recognized source rows can now open existing workspace files through the shared
native build.location parser. Live project logs supply their recorded task
working directory; other/final logs use the workspace root. This is a guarded
navigation hint, not a current diagnostic revision. See
[Compiler locations](CompilerLocations.md) for formats and limits.

The Build panel selects **Native build output** or **Project task output**. The
latter follows Cargo, npm, standalone javac, Maven, Gradle and shader compilation
through their existing ProjectTasks owner. It defaults to the project source for
detected Cargo/npm/JVM projects. Explicit shader tasks in another primary project
can use the selector. Each source keeps its own latest tail and identity.

The shared TaskOutput implementation replaces the previous build-specific
journal. It stores at most 256 KiB in 8 KiB chunks, with distinct stream IDs,
revisioned conditional reads, repaired display text and late-callback guards.
Native builds and project tasks retain their separate existing reservations,
process trees and cancellation tokens. No extra process owner or queue is added.

## Native, SDK and ABI2

`project.status` / `project_status` returns current running/cancelling state,
operation ID and method, without waiting for execution, plugin or document locks.
Its read is permission-free metadata, like build.status. It remains running until
the owner returns, including process cleanup. A cancellation acknowledgement is
not completed termination. Application terminals have a separate lifecycle.

`project.output` / `project_output` uses the same report as build.output, with an
optional `context` containing system and workspace-relative directory. Context is
absent before the first task, empty while its profile is not yet validated, then
identifies the actual selected project. Context changes advance the revision.
The result retains the last task's context after completion or failure.

Read initially with `{}`, then send both observed streamId and revision. An
unchanged read has changed:false and empty output. Every changed read replaces
the entire previous display tail; it is not appended as a delta. Stream IDs
distinguish tasks even if a caller reuses an operation ID. Overflow drops the
oldest display bytes without stopping a compiler. The journal survives until the
next task or workspace destruction and is not a persisted history archive.

The output query requires process.execute because logs can contain private
project data; it has no SDK mutation group and cannot cancel or start a task.
ABI2 exports both reads, including output from read-only callbacks with that
permission. The facade classifies these queries separately from executable
project methods, so it does not inject an operation ID or propagate read-job
cancellation to an executing build. Existing SDK identities, jobs and scheduling
remain authoritative. No C ABI change is needed.

GUI task starts now receive distinct operation IDs. Cancel task captures that ID
and retries while its own asynchronous request is entering the reservation;
remote tasks use their observed ID. A stale ID does not target a different ID.
Caller-supplied reused IDs still need caller coordination and are not credentials.
Working state and workspace switching also observe SDK/plugin-owned project work.
No force-stop or antivirus-setting change is introduced.

## Cargo display framing

Cargo format-version 1 metadata is excluded from the live display. Compilation
protocol lines are framed across arbitrary pipe chunks: compiler-message uses
rendered text (or its plain message fallback), compiler-artifact/build-script/
completion records are hidden, and ordinary stderr or unknown lines remain
visible. No executable path is inferred from this display. Existing raw process
collection and the final Cargo diagnostics/artifact parser remain unchanged.

Each live protocol line is capped at 64 KiB. An oversized line is explicitly
omitted until the next newline, then framing resumes. Unexpected framing failures
also produce an explicit message when observation is available. An unterminated
last line is flushed once. These limits do not change the existing 4 MiB project
process/result bound. A tool that buffers its own output may still appear silent.

Project byteCount/droppedBytes count bytes published to the display journal:
rendered messages for Cargo, pipe text for ordinary commands. They are not Cargo
JSON byte offsets, Unicode counts or a progress measure. UTF-8/control repair and
partial edge-character limitations match [native live output](LiveBuildOutput.md).
No ANSI interpretation is provided. Malformed Windows UTF-8 lines use the system
OEM fallback described in the shared native output guide; raw protocol bytes remain unchanged.

Persistent Run terminals, arbitrary plugin processes, background Unreal analysis
and standalone per-test result trees remain separate. Live Cargo text does not
publish incremental Problems entries; structured diagnostics retain their final
task-result path. [Diagnostic navigation](DiagnosticNavigation.md) adds keyboard
navigation over the already reported Problems/LSP locations.

## Validation

Windows production compilation, MSVC /Zs and actual SDK/schema checks are distinct
from runtime acceptance. Opt-in existing build/project/module fixtures specify
context revisions, ID reuse/late callbacks, fragmented/oversized Cargo records,
plain text repair, status/dirty-buffer reads, cancellation and keyboard filtering.
They are source-checked only during the Kaspersky suspension. Actual tool pipes,
SDK/plugin concurrency, cancellation races, GUI/themes/DPI and Linux/macOS remain
unverified. No fixture, task compiler or application is launched for acceptance.

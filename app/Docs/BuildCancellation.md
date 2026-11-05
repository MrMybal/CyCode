# Cancelling native build operations

Reviewed Unreal cook/package execution now uses this same reservation,
operation-ID matching and owned process tree. Its reviewed deadline can be
1..86400 seconds; partial archives/logs remain persistent on cancellation. No
Unreal acceptance ran during the Kaspersky suspension; see
[Unreal automation](UnrealAutomation.md).

Build > Cancel build, the command palette and the Build panel stop an active
CMake, MSBuild or explicit Unreal operation. A custom binding can be set in
Options > Keyboard shortcuts. Cargo/npm/standalone Java retain their separate
Project tasks > Cancel task service.

The UI tracks the issued operation ID while an asynchronous task enters native
execution. An old request cannot cancel the next operation. Cancel build also
signals the existing background Unreal analysis cancellation token. Closing the
application requests cancellation before waiting for owned task/plugin futures;
trusted native plugin work outside exported services must cooperate with its own
lifecycle contract.

## Shared Engine, MCP and plugins

Reviewed CMake test/package/workflow execution through build.presetRun also uses
this reservation and its existing ID/deadline/cancellation contract. See
[Preset actions](CMakePresetActions.md).

One `BuildOperations` reservation per Engine covers:

- `build.configure`, `build.run`;
- `msbuild.build/rebuild/clean/restore/evaluate/prepare`;
- `unreal.generate/database/build/prepare` when explicitly dispatched.

`build.status` / MCP `build_status` returns `running`, `operationId`, `method` and
`cancelling`. It describes the native reservation, including waiting for an adapter
lock. The separate build.output/build_output service supplies a bounded,
revisioned live tail; see [Live build output](LiveBuildOutput.md). Background GUI
Unreal analysis retains
its independent token/status rather than creating a second reservation. The GUI's
Working indicator includes native builds started by the MCP/plugin host; workspace
switching waits for them to finish or be cancelled.

`build.cancel` / MCP `build_cancel` accepts optional `operationId` and returns
`requested`. Omit the ID to stop the current native build intentionally; supply
it to require an exact match. `requested:true` acknowledges the request, not
completed termination. `running` remains true until native work and cleanup
return. A second explicit native build is rejected while that reservation is held.
Cancellation/status bypass plugin, document and adapter execution locks.

The facade gives each direct build request a fresh internal ID. SDK job
cancellation repeatedly signals only that ID and keeps receiving its native
response before the handler returns. Failed cancellation notifications do not
release the SDK reservation; the client retries while waiting on the original
call. `build_cancel` requires `process.execute` and has no workspace-mutation
exclusive group, so it can run while a build owns that group. `build_status` is
read-only. IDs correlate operations; they are not authorization tokens. An
authorized operator can explicitly cancel the shared workspace's current build.

ABI2 host requests export `build.inspect/status`, `build.configure/run/cancel`,
`msbuild.inspect/build/rebuild/clean/restore/evaluate/prepare` and the existing
Unreal services. Process/mutation services require the manifest's `process.execute`
permission and a mutating callback. Explicit build cancellation can stop these
host-owned builds while a plugin callback is waiting. Generic plugin-job
cancellation does not propagate an outer job ID through arbitrary plugin callbacks;
use `build_cancel` for their active host-owned build. Arbitrary native plugin
work cannot be interrupted by this service.

## Execution and persistence

The token reaches toolchain probes, native execution and MSBuild's per-project
evaluation loop. Waiting for an adapter/detection lock is cancellable. Interrupted
discovery does not publish a partial tool cache. Cancelled, timed-out or truncated
results cannot be reported as successful builds. Profile and analysis publication
checks cancellation; files already written or committed before observing it remain.
No build tree, project source or cache is deleted, and no automatic retry occurs.

Windows children are created suspended, assigned to an owned Job Object and then
resumed. If ownership cannot be established, the child is terminated before tool
code runs and the operation fails. On completion/stop, remaining owned workers are
terminated and the job is checked for an empty process set. Output draining is
bounded even for a process continuously writing output.

Linux/macOS use an owned POSIX process group. The parent observes child exit with
`waitid(..., WNOWAIT)` before stopping remaining group members and reaping it,
avoiding use of an already-reaped/reusable child PID for group cleanup. A process
deliberately escaping its group or using an existing shared daemon is outside
this ownership contract. This is process lifecycle management, not an OS sandbox.
References: [Linux waitid documentation](https://man7.org/linux/man-pages/man2/waitid.2.html),
[Apple's wait API header](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/wait.h).

The main CMake/MSBuild/Unreal build/generation subprocess accepts `timeoutSeconds`
from 1 to 86,400. Native defaults remain 180 seconds for CMake/Unreal and 150 for
MSBuild. Desktop Options remembers a 1,800-second default and supplies it to these
operations, including background Unreal analysis. This preference is included in
the existing configuration save/load. CLI/MCP/plugin callers can set their own
request deadline; it is not stored in project/compiler profiles or index stamps.

The real CyToolsCore facade extends its build response wait to the requested native
deadline plus 60 seconds (minimum 200), retaining correlated cancellation handling.
Other RPC waits remain 200 seconds. Tool probes and 15-second structured MSBuild
evaluations (up to eight projects) retain their independent bounds. The output cap
remains 8 MiB. Explicit build execution now has a separate 256 KiB live tail;
project tasks now share this implementation through project.status/project.output,
with their existing separate owner and captured GUI cancellation IDs. See
[Project output](ProjectTaskOutput.md). Background-analysis streaming and semantic
request cancellation remain future work.

## Validation boundary

Windows application/core/CLI compilation and source checks are the local checks
for this change. New C++ assertions cover ID matching, native ownership after a
cancel acknowledgement, exclusion of a second build, reset for the next build,
exception cleanup and pre-cancelled process requests. A new official MCP test
exercises cancellation inside a CMake configure subprocess and a subsequent job.
These scenarios have not been executed during the Kaspersky interruption;
GUI interaction and Linux/macOS execution are unverified. See [Testing](Testing.md)
and [Validation](Validation.md).

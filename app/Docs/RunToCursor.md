# Run to cursor

CyCode arms a verified source breakpoint at the editor cursor, then sends one
ordinary DAP Continue. Existing breakpoints and exceptions can stop the program
before that destination. On the next actual stop, the native owner replaces that
file's breakpoint list with its original list. The temporary point is never saved
in the project configuration. This executes program code and has no source Undo.

## Desktop

Pause a debug session, select a saved workspace source document and place its
cursor on the destination line. Choose **Run > Run to cursor**, the Find Action
command, or **Debugger > Run to cursor**. Rider uses **Alt+F9**; the Visual Studio
profile uses **Ctrl+F10**. The VS Code profile has no new default binding.
Custom bindings retain priority. These are Windows/Linux profiles; dedicated
macOS schemes and actual keyboard acceptance remain pending.

The document must already be open, unchanged since the captured native revision,
clean and byte-equal to its on-disk baseline. Validation runs before arming and
again immediately before Continue. Other unsaved tabs do not block this command.
The line must exist in that document and the adapter must verify that exact line.
A missing/unverified/relocated destination, different verified source path or
unmatched virtual source reference prevents continuation. This does not prove
that the loaded binary was built from the current file, or lock future disk edits.
Adapter-owned virtual sources and files outside the workspace remain unsupported
for this workflow; [Set next statement](GotoTargets.md) has a separate source model.

An existing enabled, unconditional destination breakpoint is reused. An enabled
conditional breakpoint, hit condition or logpoint at that line rejects the action
instead of changing its behavior. Disabled entries and all original conditions,
hit conditions and log messages remain in the original list.

The panel displays verification/progress, cleanup uncertainty and the actual stop
reason. A destination hit is reported only when the adapter's stopped event names
its actual breakpoint ID. Missing hit IDs remain unknown; another reported ID
does not become a fabricated destination hit. The interface uses CyUI Core roles,
existing style dimensions and English/French labels. No new CyUI module is needed.

## Native owner and recovery

`RunToCursor` owns one bounded, revisioned temporary-breakpoint lease. `Debugger`
owns its transport and runs the whole operation on the existing DAP worker;
`Engine` validates native document identity/revision and saved bytes. The worker
captures the complete original source list and maps enabled entries to DAP's
replacement response order. One temporary unconditional point is appended only
when an existing destination cannot be reused. The persisted breakpoint map is
unchanged; stale verification metadata is removed during the temporary overlay.

Before Continue, the owner checks session/stop/thread, source identity, current
saved bytes, original breakpoint equality and intervening stopped/continued events.
It sends one Continue, with optional capability-gated `singleThread`, and preserves
the actual `allThreadsContinued` response. Stepping granularity and goto target IDs
are not accepted. Existing target catalogues expire on execution submission.

An early stopped event is retained even when it arrives before Continue's reply.
Cleanup restores the original requested list and publishes the adapter's actual
verification for it. Original points can still be unverified or relocated by the
adapter; acknowledging replacement does not prove physical address identity.
After cleanup, normal stack inspection refreshes the current pause.

An arming failure with a settled reply and confirmed pause attempts one original
list restoration. An unanswered request retains its identity and `replyPending`;
late replies are consumed without automatically repeating or continuing an action.
An uncertain Continue requires an actual stopped event before restoration.
Other debugger mutations are blocked while the lease is active; status remains
readable, and Pause or Stop remain available. Restoration itself needs a confirmed
pause, settled requests and the current lease revision. The panel's **Restore
source breakpoints** action supplies these guards. A restoration error keeps the
lease unconfirmed and requires an explicit recovery action. Stopping the session
releases the lease; there is no automatic retry, target restart or process kill.

## SDK/MCP and plugins

`debug_run_to_cursor` maps to `debug.runToCursor`. Capture the actual document ID,
revision, current breakpoint list and debug context rather than inventing them:

```json
{"path":"src/main.cpp","line":40,"sessionGeneration":3,
 "stopGeneration":7,"threadId":11,"expectedDocumentId":2,
 "expectedRevision":9,"expectedBreakpoints":[],"singleThread":false}
```

The path refers to an already-open persistent workspace document, not a CyTools
task artifact. No file is staged or copied by this operation. All inputs except
`singleThread` are required. The response is the common debugger state, including
`runToCursor`: status, monotonic revision, active/restorePending/replyPending,
captured session/path/line, temporary, stopConfirmed, actual verification,
nullable targetHit, stopReason, message and actual continueResult.
`lastExecution` records the actual `debug.continue` acknowledgement separately
from restoration uncertainty.

`debug_restore_run_to_cursor` / `debug.restoreRunToCursor` uses the currently
observed session, pause and lease revision, for example:

```json
{"sessionGeneration":3,"stopGeneration":8,"expectedRevision":12}
```

Both operations use the real CyToolsCore manifest/handler, `process.execute`,
the existing workspace mutation group and noncancellable metadata. ABI2 plugins
use the same methods through `host->request`; no new C ABI or Python editor/debug
owner is introduced. Acknowledgement is not evidence that the destination was hit.

Bounds: 1 MiB serialized native request and original list, 1000 source entries
including disabled points, destination lines 1..10000000, path 8192 UTF-8 bytes,
condition/hit/log text 4096 bytes, replacement reply 512 KiB with exact enabled
entry count. Actual optional breakpoint IDs are signed int32; missing IDs remain
missing. Coordinates retain DAP integer bounds. Pre-ack breakpoint events are
bounded to 256 entries / 1 MiB total / 64 KiB each. Malformed or relocated target
verification refuses continuation; excess events mark the lease unconfirmed.

## Verification and remaining acceptance

Source-only MSVC `/Zs` checks cover the native owner/worker, Engine, UI, command
profiles, plugin exports and regression sources. Assertions specify preservation
and enabled-to-wire mapping, collision/capacity rejection, observed guards,
verification bounds/signed IDs, deferred events, stop ordering, actual hit reports,
revisioned restoration and uncertainty. Constructor compatibility is also checked.
The actual CyToolsCore loader and Draft 2020-12 schema data checks accept all
187 operations, typed outcomes, mandatory fields, permissions, cancellation
metadata, defaults/custom bindings and translated controls.

These are compilation and data checks, not executed native assertions. Real
adapter arming/cleanup, source/binary mismatch, late transport replies, concurrent
events/threads, SDK/plugin Runtime, GUI/keyboard/scrolling/themes/DPI and Linux/macOS
acceptance remain pending during the Kaspersky native-test suspension. See
[Validation](Validation.md) and [Testing](Testing.md) for completed checks.

Primary contracts: [DAP specification](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json),
[Rider stepping](https://www.jetbrains.com/help/rider/Stepping_Through_the_Program.html),
[Visual Studio debugger navigation](https://learn.microsoft.com/en-us/visualstudio/debugger/navigating-through-code-with-the-debugger?view=vs-2022).

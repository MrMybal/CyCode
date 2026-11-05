# Set next statement

CyCode can ask a capable debug adapter for valid destinations in the selected
frame's source, then move that thread's next instruction to one explicit choice.
This is DAP `gotoTargets` / `goto`, guarded by `supportsGotoTargetsRequest`.
The intervening code is skipped, and moving backward can repeat side effects.
This does not implement Run to cursor, resume the program or undo source edits.

## Desktop

Choose **Run > Set next statement**, the same Find Action command, or the action
in **Debugger > Execution controls**. The **Next statement** tab opens a query
at the selected frame's line. It shows that frame's source path/name/reference;
enter another **Destination line** and optional **Destination column** in that
same source, then choose **Read next-statement targets**. Lines/columns are
one-based; column 0 in the form means unspecified and is omitted from the request.
Columns count UTF-16 code units, rather than visual indentation/display columns.

Select an actual returned destination and choose **Set next statement here**.
There is no automatic first selection or execution on row selection. Changing
the form clears its previous choices. A real empty result is displayed explicitly.
The shared target table preserves stable issued row IDs, actual labels/start
positions and delayed full-label/instruction-reference tooltips. It uses CyUI
Core styles, palette roles, DPI-scaled dimensions and English/French controls.
Existing keymaps remain intact; the query command can receive a custom binding.

The source is always the actual selected stack frame's descriptor. Files outside
the workspace, engine frames and adapter-owned virtual sources can be queried
without opening/reading arbitrary files or fabricating a source path. This UI does
not accept an arbitrary second source or infer that another open tab matches the
selected frame. It does not validate that an edited source still matches the
compiled program; actual valid destinations remain the adapter's responsibility.

## Common owner and lifecycle

`DebugTargets` replaces the step-in-only implementation with one common native
catalogue implementation. Separate step-in/goto instances retain independent
counters and markers; `StepInTargets` remains a source-compatible alias selecting
the original default family. The shared table also replaces duplicate row logic.
There is one DAP worker, no new executable/dependency/CyUI module and no C ABI change.

Both query and execution require explicit observed `sessionGeneration`,
`stopGeneration`, `threadId` and `frameId`. They must match the current inspected
pause and actual selected frame. Query inputs add `line` and optional `column`;
caller paths, source descriptors, raw target IDs, arbitrary commands and stepping
options are rejected. The native owner filters the actual frame source descriptor
and passes bounded opaque `adapterData` back to that same adapter when present.
That opaque data remains private and is compared again before issuing a response
and before dispatching a selected target. A changed source cannot silently reuse
a captured response. Public source metadata contains only path/name/reference.

A query replaces the goto catalogue, even within the same pause. Each returned
target has an issued string ID, the actual label and required actual line, optional
range coordinates and `instructionPointerReference`. Signed int32 adapter IDs
stay private. Handles expire on refresh, resume/new stop/invalidation/memory events,
frame/thread/capability/session changes or execution submission. Neither catalogue
nor process instruction references are persisted. `debug_status.gotoTargetCatalog`
is the current issued catalogue ID, or `""`; `stepInTargetCatalog` stays independent.

Executing a choice consumes both families' catalogues before sending exactly one
DAP `goto`. `lastExecution.method` is `debug.gotoTarget`; its public arguments
contain the issued handle and captured frame/thread, never the raw adapter ID.
`singleThread` and `granularity` are not accepted or injected from UI preferences.
The existing applying/accepted/unconfirmed outcomes and execution epoch preserve
a new stopped event that arrives before acknowledgement. There is no automatic
retry, continuation, reverse operation or source Undo.

If acknowledgement arrives first, the old stack is invalidated and `pendingStop`
is set. The public state remains stopped, with empty inspection and zero selected
thread/frame; no resumed program state is fabricated. Debugger operations other
than stopping the session are blocked, while status/profile/adapters remain
readable. Actual stopped/continued/termination events remain authoritative.
Invalidation events preserve the pending wait. A missing stopped event after
15 seconds marks the outcome unconfirmed and leaves the wait blocked; it does not
inspect stale references, resume, retry, kill or disconnect the program automatically.
Wait for the adapter's event or explicitly stop the debugging session.

## SDK/MCP and plugins

`debug_goto_targets` maps to `debug.gotoTargets`. For an observed session 3,
stop 7, selected thread 11 and frame 9:

```json
{"sessionGeneration":3,"stopGeneration":7,"threadId":11,"frameId":9,
 "line":40,"column":5}
```

The result contains that observed context, `catalogId`, filtered `source`,
`requestedLine`, optional `requestedColumn`, and `targets`. Pass an actually issued
target ID to `debug_goto_target` / `debug.gotoTarget` with the same context:

```json
{"sessionGeneration":3,"stopGeneration":7,"threadId":11,"frameId":9,
 "target":"goto-1-0"}
```

This sample ID is illustrative; never reconstruct or persist handles. Both SDK
operations require `process.execute`. The mutation is noncancellable in the real
CyToolsCore manifest and handler. ABI2 plugins use the same methods/guards through
`host->request`. Source references belong to the debug session, not SDK task
artifacts, and are not staged or copied by the SDK.

Limits: 4 KiB public request, coordinates 1..10000000 on input, 64 KiB selected
frame source descriptor including adapter data, 128 destinations / 512 KiB adapter
reply, 4096 UTF-8 bytes per label and 8192 per nonempty instruction reference.
Malformed/duplicate signed IDs, NUL/oversized text, missing destination lines,
invalid or reversed explicit ranges fail without publishing a partial catalogue.
Returned coordinates preserve DAP integer bounds 0..9007199254740991; zero remains
unknown and missing optional fields remain absent. Extra adapter target fields are
ignored. Distinct IDs with duplicate labels remain distinct.

## Verification and remaining acceptance

MSVC `/Zs` checks the common native implementation, worker/UI/SDK plugin exports
and regression sources without linking or running native tests. Assertions specify
frame/source/context changes, opaque-data isolation/comparison, virtual sources,
pending-state rejection, signed/issued identity isolation, bounds, exact goto wire
fields, actual empty results, refresh/expiry and GUI option capture. Existing
step-in and execution-epoch assertions remain in the same debugger target.
Actual CyToolsCore loading and schema data examples validate all 185 operations,
typed goto/pending outcomes, field isolation, permissions and translated controls.
See [Validation](Validation.md) for completed build/source checks.

Actual adapters and VM behavior, unsupported destinations, event/deadline races,
SDK/plugin runtime, keyboard/scrolling/themes/DPI and graphical acceptance remain
unexecuted during the Kaspersky native-test suspension. Linux/macOS compilation
and execution remain pending. [Run to cursor](RunToCursor.md) now has a separate
temporary-breakpoint/Continue/restoration workflow with pending runtime acceptance.
Full per-thread non-stop debugging remains separate work.

Primary contract: [Debug Adapter Protocol specification](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

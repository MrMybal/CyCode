# Targeted step-in

When several calls share a source line, CyCode can query the debug adapter for
the functions it can enter and submit one explicit choice. GUI, SDK/MCP and ABI2
plugins share the existing native DAP worker. Availability requires the actual
`supportsStepInTargetsRequest` capability, a paused selected thread and a frame
present in the inspected stack. No targets are inferred from source syntax.

## Desktop

Use **Run > Choose step-in target**, the same command in Find Action, or
**Debugger > Execution controls > Choose step-in target**. The **Step-in targets**
tab also provides **Read step-in targets**. Read the list, select a function, then
choose **Step into selected target**. Reading never resumes the program; selecting
a row never executes it. No first target is selected automatically. Existing
keymaps remain intact; the new command can receive a custom shortcut.

Rows have stable host-issued identities independent of their labels. The table
shows the actual function label and optional reported starting line/column. A
multiline label is shown in full in its tooltip. The full optional end range is
also retained in the SDK result. A genuine empty result is displayed as such.
Stepping granularity and the selected-thread option from Execution controls apply
only when separately advertised by the adapter. All controls use CyUI Core,
translated English/French labels, theme/style roles and DPI-scaled dimensions.

## Common service

`StepInTargets` is now a compatibility alias for the UI-free `DebugTargets`
implementation, defaulting to the step-in family. Goto uses a separate instance
of that same implementation; see [Set next statement](GotoTargets.md). Each
instance owns a single catalogue and keeps signed int32 DAP
target IDs private. Its monotonic catalogue counter is not reset when a session
ends. Refreshing replaces the previous catalogue, including within the same pause.
`debug_status.stepInTargetCatalog` is the current issued catalogue ID, or `""` when
none is available. It does not contain target descriptions or raw adapter IDs.

References expire on execution submission, continued/stopped/invalidation/memory
events, disconnect, selected frame/thread changes and adapter capability changes.
Both query and execution require explicit observed `sessionGeneration`,
`stopGeneration`, `threadId` and `frameId`. They must still match at native
dispatch; a query response is checked against the live context again before it
is issued. Late responses cannot populate a different stop's catalogue. The GUI
additionally checks the catalogue ID when displaying an asynchronous result, so
a refresh by another client clears its previous choices.

The query sends DAP `stepInTargets` with only the selected `frameId`. Targeted
execution resolves a host handle, validates optional stepping settings and sends
exactly one DAP `stepIn` with the private `targetId`. The entire catalogue is
consumed before submission, including when acknowledgement later fails. Legacy
`debug.stepIn` continues to reject both raw `targetId` and issued `target` fields.
An explicit refresh is required to obtain another choice after an attempt.

`lastExecution.method` is `debug.stepIntoTarget`. Its public arguments contain
the issued `target`, `frameId`, `threadId` and optional stepping settings, never
the raw adapter target ID. The existing applying/accepted/unconfirmed outcomes,
actual stopped-event refresh and rapid-stop preservation apply. Acceptance only
acknowledges the request. There is no automatic retry, source Undo, rollback or
cancellation after execution submission.

## SDK/MCP and plugins

`debug_step_in_targets` maps to `debug.stepInTargets`; `debug_step_into_target`
maps to `debug.stepIntoTarget`. Both require `process.execute`. The execution
operation is noncancellable in the real CyToolsCore manifest and handler.
ABI2 `host->request` uses exactly these native methods and guards. No C ABI version
change, extra executable, external dependency or new CyUI module is required.

For an observed session 3, stop 7, selected thread 11 and frame 9:

```json
{"sessionGeneration":3,"stopGeneration":7,"threadId":11,"frameId":9}
```

A query result contains the same context plus `catalogId` and `targets`. Each
target has an issued string `id`, the actual `label`, and optional `line`, `column`,
`endLine`, `endColumn`. Pass the selected returned ID as `target` with that same
captured context; do not reconstruct or persist handles. For example, if the actual
result issued `step-1-0`:

```json
{"sessionGeneration":3,"stopGeneration":7,"threadId":11,"frameId":9,
 "target":"step-1-0","granularity":"line","singleThread":false}
```

Limits: one catalogue, 128 targets, 4 KiB query/execution request, 512 KiB adapter
reply and 4096 UTF-8 bytes per nonempty label. Unknown public request fields,
invalid/duplicate signed adapter IDs, NUL text, malformed coordinates, oversized
responses and reversed explicit ranges are rejected before publishing a catalogue.
Extra adapter response fields are ignored. Optional positions retain DAP's
0..9007199254740991 integer bounds: initialize requests one-based lines and UTF-16
columns; zero is retained as unknown and omitted from the table. Missing optional
positions remain absent. Duplicate labels with different IDs remain distinct.

## Verification and remaining acceptance

MSVC `/Zs` checks cover the native/UI implementations and regression assertions
for context/capability changes, refresh/expiry, signed IDs, duplicates, actual empty
results, bounds, raw-ID isolation, stepping options and captured GUI parameters.
These assertions have not been linked or executed. Actual CyToolsCore manifest
loading and schema examples check the 183-operation catalogue, typed input/output,
legacy field isolation, permissions, execution cancellation metadata and labels.
See [Validation](Validation.md) for the completed production/source checks.

Real CodeLLDB/NetCoreDbg/java-debug/plugin adapters, nested/inlined calls, late
events/timeouts, SDK/plugin runtime, keyboard/scrolling/themes/DPI and graphical
acceptance remain unexecuted during the Kaspersky native-test suspension.
Linux/macOS builds remain pending. Run-to-position and full non-stop thread UI
are separate work; this does not provide them.

Primary contract: [Debug Adapter Protocol specification](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

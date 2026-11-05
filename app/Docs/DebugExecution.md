# Stepping, reverse execution and frame restart

The Debugger **Execution controls** tab and Run commands share the existing native
DAP worker with SDK/MCP and ABI2 plugin requests. DebugExecution validates plans
and acknowledgements without UI dependencies. No target process owner, recording
engine, CyUI module or external dependency is added.

## Desktop controls

Pause the program and select its thread. **Stepping granularity** offers
**Adapter default**, **Statement**, **Source line** and **Machine instruction**
when supportsSteppingGranularity is advertised. Default omits the wire field.
**Only selected thread** requires supportsSingleThreadExecutionRequests; otherwise
the normal adapter behavior applies. Unavailable options reset on status polling.
These settings are local to this application instance and are not saved in launch.json.

The existing Step over / Step into / Step out shortcuts use those options too.
Continue uses the thread option and never receives a stepping granularity.
Run > **Step back**, **Continue backward** and **Restart selected frame** also appear
in the action palette and can receive user-defined keyboard bindings. They have no
new default shortcut that would replace an existing Rider/VS/VS Code binding.

Step back and Continue backward require supportsStepBack. The external debugger
must actually support the required execution history. CyCode does not emulate
reverse execution through document Undo or repeated launches. Restart selected
frame requires supportsRestartFrame, a current inspected selected frame and
canRestart other than false. It sends one restartFrame request and uses the actual
subsequent stopped event. It does not issue an automatic stepIn or restart the VM.
Re-executing code can repeat its I/O and other effects; it does not undo them.

**Last execution request** distinguishes submission, adapter acknowledgement and an
unconfirmed outcome. When supplied, the actual allThreadsContinued value remains
visible, including false. The current host still uses one selected execution
context: independent simultaneous inspection of every suspended thread while
other threads run is not implemented by these controls.

## Native and SDK contracts

All seven execution operations require process.execute. Their SDK handlers retain
the native result after submission and are noncancellable. Source files and source
Undo/Redo are unaffected.

| SDK operation / native method | Context | Options |
| --- | --- | --- |
| debug_continue / debug.continue | optional sessionGeneration, stopGeneration, threadId | singleThread |
| debug_next / debug.next | optional same context | granularity, singleThread |
| debug_step_in / debug.stepIn | optional same context | granularity, singleThread |
| debug_step_out / debug.stepOut | optional same context | granularity, singleThread |
| debug_step_back / debug.stepBack | required sessionGeneration, stopGeneration, threadId | granularity, singleThread |
| debug_reverse_continue / debug.reverseContinue | required same context | singleThread |
| debug_restart_frame / debug.restartFrame | required sessionGeneration, stopGeneration, frameId | none |

For the four existing forward operations, omitted context is captured under the
native queue lock when submitted. Explicit guards are preserved. New reverse/frame
operations require explicit observed context, including through the plugin host.
The GUI always captures its observed context for all seven commands. A changed
session, pause, selected thread or selected restart frame is rejected before
dispatch. Thread/frame IDs are bounded signed 32-bit references, with positive
threads and nonnegative frames. A restart frame must occur in the inspected stack;
its canRestart hint is respected, defaulting to true only with advertised support.

Options require their own capabilities even when an explicit singleThread=false
or granularity=statement is provided. Continue/reverse Continue reject granularity;
frame restart rejects thread/granularity flags. Unknown fields, arbitrary commands,
targetId overrides, malformed booleans and unadvertised options are rejected.
Selected-target step-in and run-to-position are separate
workflows, rather than unchecked extra fields in this API.

Example after reading actual debug_status:

```json
{
  "sessionGeneration": 3,
  "stopGeneration": 7,
  "threadId": 11,
  "granularity": "instruction",
  "singleThread": true
}
```

That request can be used with debug_step_back only when all three required
capabilities are advertised; it does not enable recording or install a backend.

## Events and uncertain results

Optional debug_status.lastExecution records method, applying/accepted/unconfirmed
status, observed session/stop generations, actual wire arguments, message and a
bounded validated result. A new launch clears it. It is the last submitted attempt,
not a complete execution history. Pre-dispatch validation errors can leave the
previous record unchanged. Failed calls still leave their attempted outcome
available through debug_status; no automatic retry is issued.

accepted means the adapter acknowledged the request. It does not manufacture a
completed step, a restarted frame or target values. After acknowledgement, the
original paused inspection transitions to running while awaiting real events.
An actual stopped event then supplies a new inspection generation and stack.
If that event arrives before the acknowledgement is consumed, it remains stopped;
a late response does not overwrite that new pause. A separate execution epoch
allows same-pause invalidated/memory cache events without mistaking them for a new
stop. A continued or exited event also remains authoritative.

Failed, malformed or timed-out submitted requests may already have changed the
target. They invalidate inspection and record unconfirmed without automatic
rollback or replay. Native error state is still reported. DAP results are bounded
to 64 KiB; absent optional thread-continuation reports are not fabricated. Target
events, not a returned empty acknowledgement, establish the next paused state.

## Verification and remaining acceptance

Source-only MSVC /Zs checks cover native/UI code and regression assertions for
legacy defaults, capabilities, context/frame changes, per-frame restart hints,
granularity/thread options, acknowledgement shapes, empty/partial thread reports
and immediate-stop versus same-pause-invalidation ordering. The existing module
test source specifies GUI context capture and preservation of explicitly supplied
guards. These assertions were compiled as syntax only, not executed or linked.

The actual CyToolsCore manifest loader and all schemas accept 180 operations.
Data examples cover legacy and required contexts, field isolation, bounds, typed
acknowledgement states, permissions/noncancellable handlers, plugin/menu methods
and English/French labels. See [Validation](Validation.md) for production checks.

Actual CodeLLDB/NetCoreDbg/java-debug/contributed adapter capabilities, recording,
frame restart behavior/side effects, mixed thread states, timeouts/late events,
SDK/plugin runtime and keyboard/themes/DPI remain unexecuted during the Kaspersky
native-test suspension. Linux/macOS compilation/execution remains pending.
Instruction breakpoints now have a separate shared service with pending acceptance;
see [Instruction breakpoints](InstructionBreakpoints.md). Targeted step-in now
has a shared guarded catalogue and execution service with pending acceptance;
see [Targeted step-in](StepInTargets.md). Remote/core dumps and
full per-thread non-stop UI remain separate work.

Set next statement now has a separate guarded goto catalogue and pending-stop
flow sharing this owner. It skips code, does not resume or run to the cursor,
and accepts no stepping options. See [Set next statement](GotoTargets.md).

Run to cursor now uses one ordinary Continue after verifying a temporary source
breakpoint and restores the original source list at the next stop. It shares this
worker's execution epoch/actual acknowledgement handling, with separate lease,
pending-reply and restoration guards. Real adapter/GUI acceptance is pending;
see [Run to cursor](RunToCursor.md).

Primary contract: [Debug Adapter Protocol](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

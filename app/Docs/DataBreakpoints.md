# Data breakpoints

Debugger > **Data breakpoints** uses the common native DAP owner, SDK and ABI2
host exports. It requires the active adapter's `supportsDataBreakpoints` capability;
no support is inferred from a language or project type. Actual adapter/hardware/GUI
acceptance remains pending; see [Validation](Validation.md).

## Desktop workflow

Pause the program, select a stack frame, then enter a variable/expression using
**Selected stack frame**, or a child name within a displayed variable scope.
**Inspect data breakpoint target** requests actual `dataBreakpointInfo`. Its
description explains the target or why it is unavailable. Choose an advertised
read/write mode or **Adapter default**, which omits accessType on the wire instead
of inventing a write fallback. Conditions/hit conditions require their capabilities.

Add every desired inspected target to the **Replacement list**, then **Replace
data breakpoints**. This replaces the entire list rather than appending implicitly.
Rows can be removed locally; **New replacement list** discards the draft and
captures the latest configuration revision. **Clear data breakpoints** submits an
empty list. All mutations require a paused target and actual adapter support.

The status table retains the adapter's verified flag/message, including refused
targets and hardware limits. Changed/removed breakpoint events update verification.
A data-breakpoint stop uses the common stack, variables and stepping views.

## Native and SDK contract

Both operations require process.execute and observed sessionGeneration /
stopGeneration from debug_status:

- `debug_data_breakpoint_info` / `debug.dataBreakpointInfo`: name plus optional
  positive variablesReference from a current scope. Without a container, the owner
  supplies the selected frameId. Caller-supplied frame IDs/range flags/raw dataId
  are not accepted. Output: available, issued id, name/description, accessTypes,
  canPersist and both generations. A null adapter dataId returns available:false.
- `debug_data_breakpoints` / `debug.dataBreakpoints`: complete breakpoints array
  (empty clears), expectedRevision from debug_status.dataBreakpointRevision and
  both generations. Entries use an issued id and optional advertised accessType,
  condition and hitCondition. Output includes actual dataBreakpoints,
  dataBreakpointRevision and dataBreakpointsSynchronized.

Example after actual status/inspection calls (replace these illustrative values):

```json
{
  "breakpoints": [{"id": "data-3-1", "accessType": "write"}],
  "sessionGeneration": 3,
  "stopGeneration": 7,
  "expectedRevision": 4
}
```

Raw adapter IDs remain private to the registry/wire plan. Bounds: 128 handles and
1 MiB of stored text payload per pause, 32 replacement entries, 64 KiB info replies,
512 KiB verification replies and bounded names/messages. Duplicate targets,
unadvertised modes and unknown/expired handles are rejected before mutation.

## Lifetime and failure

Resume, another stop, DAP invalidation, termination and disconnect expire inspection
handles. Already installed targets can survive resume and remain visible. Inspect
every desired target again at a later pause before submitting a new full list.
canPersist is adapter metadata: CyCode keeps data targets within this session and
does not save or restore them through launch.json.

An expectedRevision guard rejects an older draft. Before submitting replacement,
the owner marks the list unsynchronized. Timeout, rejection, malformed verification
or a changed pause may leave target-side effects; the old list is retained only as
the last known configuration with dataBreakpointsSynchronized=false. Re-inspect and
replace/clear while paused to establish a new confirmed list. The operation has no
Undo or cancellation once submitted; the SDK retains its native outcome rather
than discarding a mutation due to a late cancellation token. Inspection can involve
target expression evaluation. No target/server is restarted or installed.

See the upstream [DAP contract](https://microsoft.github.io/debug-adapter-protocol/specification#Requests_DataBreakpointInfo).
Address/range/mode controls, automatic re-resolution and persisted targets remain
separate work. New regression assertions were compiled with /Zs, not executed.
Real native/.NET/JVM, MCP/plugin, hardware, keyboard/theme/DPI and Linux/macOS
acceptance remain unexecuted while native tests are suspended after antivirus removals.

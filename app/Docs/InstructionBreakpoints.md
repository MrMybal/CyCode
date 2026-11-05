# Instruction breakpoints

The Debugger **Instruction breakpoints** tab uses the shared native DAP owner,
SDK/MCP and ABI2 plugin service. It requires supportsInstructionBreakpoints from
the active adapter. No dependency, CyUI module, process owner or new test executable
is added. Actual adapter/GUI acceptance remains pending.

## Desktop workflow

Pause the target and read **Disassembly**. Right-click an address and choose
**Add instruction breakpoint**, or **Copy instruction address**. The add action
requires the result's original session/pause and rejects rows marked invalid.
Late disassembly results do not repopulate an expired inspection. Address-scoped
popup IDs prevent a refreshed row from silently becoming another target.

The instruction tab also accepts an explicit adapter instruction/memory reference
with a byte offset; **Use selected frame instruction** copies the actual frame
reference. **New instruction replacement list** copies the entire last known
configuration into an editable draft. Adding the first entry starts such a draft,
so an existing list is not silently dropped. Rows retain stable local IDs while
others are added/removed. Edit references, offsets, enabled flags, conditions and
hit conditions, review the whole list, then **Apply instruction breakpoints**.
Typing or closing the tab does not submit a change.

**Clear instruction breakpoints** explicitly sends an empty list. Resuming,
changing pause/session, inspection invalidation or successful application discards the draft. A competing
configuration revision disables the old draft until a new list is prepared.
These actions affect the debuggee and are separate from source-document Undo.

## Native and SDK contract

`debug_instruction_breakpoints` / `debug.instructionBreakpoints` requires
process.execute and all four fields:

```json
{
  "sessionGeneration": 3,
  "stopGeneration": 7,
  "expectedRevision": 4,
  "breakpoints": [
    {"instructionReference": "adapter-reference", "offset": 0,
     "enabled": true, "condition": "counter > 1", "hitCondition": "3"}
  ]
}
```

Use observed debug_status generations and instructionBreakpointRevision, with
references reviewed for that target. Native/plugin requests also require explicit
context; an omitted stop is not silently filled. The owner rejects stale contexts
and configuration revisions before mutation. Conditions/hit conditions on enabled
entries require their advertised capabilities. Disabled entries remain local with
verification:null and are omitted from the DAP request, rather than inventing a
negative adapter verification for them.

References remain opaque strings. CyCode does not infer a symbol, pointer origin,
byte width or address rebase from them. The manual API validates their shape and
observed context, not that the caller obtained them from a particular adapter reply.
The backend determines whether a reference actually resolves and can be bound.
Disassembly addresses are passed unchanged; offsets are in bytes, not instructions.

Bounds are 128 entries / 1 MiB serialized request data, references of 1–8192 UTF-8
bytes, offsets within ±1 MiB and condition strings up to 4096 bytes, without NUL.
Exact duplicate reference/offset pairs and unknown fields are rejected. Different
strings that alias the same address are not canonicalized locally. Breakpoint
modes and instruction logpoints are not exposed by this contract.

## Verification, events and lifetime

debug_status exposes instructionBreakpoints, instructionBreakpointRevision and
instructionBreakpointsSynchronized. Entries retain requested reference/offset and
options, plus real verification: verified, message, optional signed 32-bit id,
actual instructionReference/offset and pending/failed reason. Requested locations
are not substituted for missing actual locations. Signed IDs and optional reported
offsets preserve their DAP ranges. Verification parsing is shared with data
breakpoints, whose existing ID contract is preserved.

Reply counts must match enabled entries in order, with unique returned IDs. Replies
are limited to 512 KiB. Changed/removed events update actual verification. During a
pending replacement, up to 256 events / 1 MiB are retained and reconciled by ID after
the acknowledgement; individual events are bounded to 64 KiB. Missing/ambiguous
verification, malformed events or exhausted event budgets leave uncertainty visible.
An adapter coalescing alias targets into repeated IDs requires an explicit corrected
list; the host does not guess their identity.

Before submission the revision advances and synchronization becomes false. A
rejected, malformed, timed-out or execution-expired request can already have side
effects. The old list remains only the last known configuration. Inspect status,
then explicitly replace or clear while paused to establish a confirmed list again.
The SDK handler is noncancellable after submission and retains the native outcome.
There is no automatic retry, rollback or replay.

Already installed points can remain across resume. Target termination, disconnect
or a new launch resets this session configuration; no instruction addresses are
written into launch.json or automatically reinstalled in another process. Same-pause
cache invalidation does not falsely classify an acknowledgement as a different
execution stop. A real new pause/resume while waiting leaves the mutation uncertain.

## Verification performed

MSVC /Zs checks the native/UI/regression sources without executing assertions or
producing test objects/executables. Source assertions cover enabled mappings,
conditions, exact references, duplicate/revision rejection, pre-ack events,
relocated/removed/pending verification, signed IDs, uncertainty, clear/reset,
count/byte/event bounds, invalid disassembly markers and captured GUI drafts.
Actual CyToolsCore manifest/schema examples check all 181 operations, typed
disabled/verified/uncertain outputs, guards, bounds, permissions, cancellation
metadata and English/French controls. See [Validation](Validation.md) for delivery.

Real CodeLLDB/NetCoreDbg/java-debug/plugin behavior, protected or unloaded code,
aliases/ID reuse, instruction-byte patching, conditions/hit semantics, late events,
concurrency, MCP/plugin runtime and keyboard/themes/DPI remain unexecuted during the
Kaspersky test suspension. Linux/macOS compilation and execution remain pending.

Primary contract: [Debug Adapter Protocol](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

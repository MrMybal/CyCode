# Debugger value and memory editing

Paused target edits use the existing native DAP worker. Desktop views, SDK/MCP
operations and ABI2 plugin host requests call the same service. Support comes from
the active adapter's capabilities, rather than the file language. No additional
CyUI module, external dependency, executable or debugger implementation is added.

## Desktop workflow

Right-click a value in the Variables tree or Scopes / Registers table and choose
**Change value**. The **Edit value** tab retains the displayed value and lets you
enter an adapter/language expression before **Apply value change**. An empty value
is passed through, since its meaning belongs to the adapter. A readOnly
presentation hint disables this action. Registers can be assigned only when the
adapter exposes them as writable variables; no universal register-write API is
invented.

When supportsSetExpression and evaluateName are available, assignment uses
setExpression in the selected frame, as recommended by DAP. Otherwise a named
child uses setVariable when supported. **Edit watched expression** captures the
current Watch text and frame without automatically evaluating it or treating an
earlier Watch result as its current value. Target expressions can invoke code.
The form is discarded when its pause, frame or session changes.

In **Memory**, first read the target reference and offset. Enter replacement
hexadecimal byte pairs, such as `00 FF 2A`, under **Write target memory**. A preview
shows old and new byte values. **Write these bytes** replaces a prefix of the read
range; it requires enough actually readable bytes and both read/write capabilities.
Changing the reference, offset or execution context requires another read. There
is no implicit write on typing, focus loss or read refresh.

All changes affect the debuggee. They are separate from source-document editing
and its Undo/Redo history; there is no automatic target Undo, rollback or retry.

## Shared contracts

Each operation requires process.execute and explicit positive sessionGeneration
and nonnegative stopGeneration from debug_status. Native host requests require
these too; target setters do not silently pin a missing stop at dispatch time.
The target must still be paused in that session when the queued operation runs.

| SDK operation / native method | Additional input | Advertised capability |
| --- | --- | --- |
| debug_set_variable / debug.setVariable | positive variablesReference, name, value | supportsSetVariable |
| debug_set_expression / debug.setExpression | expression, value, observed selected frameId | supportsSetExpression |
| debug_write_memory / debug.writeMemory | memoryReference, optional offset, bytes, expectedBytes | supportsReadMemoryRequest and supportsWriteMemoryRequest |

Names/expressions are nonempty and bounded to 4096 UTF-8 bytes, values to 8192
bytes, references to 8192 bytes; embedded NUL and unknown parameters are rejected.
Frame/container references are bounded signed 32-bit values. An expression's frame
must match the selected frame, rather than silently using a newly selected one.
Memory accepts 1–4096 integer bytes in 0–255 and offsets within ±1 MiB.
expectedBytes must have exactly the replacement length. Schema bounds validate
individual arrays; native code also checks their matching lengths and live context.

Illustrative memory request, after inspecting actual status and bytes:

```json
{
  "sessionGeneration": 3,
  "stopGeneration": 7,
  "memoryReference": "adapter-issued-reference",
  "offset": 0,
  "expectedBytes": [1, 2, 3],
  "bytes": [0, 255, 42]
}
```

The owner reads the range again and rejects mismatched/unreadable expected bytes
before submitting writeMemory with canonical base64 data and allowPartial=false.
This comparison is not an atomic compare-and-swap: other target threads or actors
may still change memory between requests. A second read checks the resulting
bytes. Only an actual adapter-provided bytesWritten/offset is exposed; a successful
reply without a byte count does not fabricate one. Malformed, oversized or
inconsistent replies are rejected.

## Outcomes and reference lifetime

debug_status.lastMutation is optional until a validated attempt starts. It records
method, sessionGeneration, status, message and result. It remains the last observed
attempt, not a mutation history; a new launch clears it. A pre-dispatch validation
failure may leave that previous record unchanged.

| Status | Meaning |
| --- | --- |
| checking | Memory preflight read is pending; no write submitted yet. |
| applying | The target setter is being submitted or awaited. |
| applied | A valid value/type reply was acknowledged, or memory read-back matches. |
| partial | The adapter explicitly reported fewer bytes written than requested. |
| rejected | Memory preflight/context check prevented submitting the write. |
| unconfirmed | A submitted operation failed, timed out, changed execution context, or could not be verified. Effects may already exist. |

Value results contain actual returned value and optional type. Memory results
contain requestedBytes, optional actual bytesWritten/offset, and readBackVerified
only when a read-back completed. An explicit partial count remains partial even if
the remaining requested bytes already happen to match. A failed call still leaves
its lastMutation outcome available through debug_status.

After every validated attempt, including failure, inspection generations/caches
and data-breakpoint inspection handles expire and the shared owner refreshes the
paused stack/scopes. Returned setter references are validated but omitted from the
published result because they would immediately expire. DAP invalidated/memory
events also expire inspection. A separate execution epoch prevents a resume/new
pause from being mistaken for normal same-pause cache invalidation during a setter.

SDK operations are noncancellable after submission; the bridge retains the native
outcome instead of discarding it on a late cancellation token. Errors never trigger
an automatic second setter, JVM restart or target rollback.

## Verification and remaining acceptance

The new pure native contract regression source belongs to the existing debugger
test target; it is checked with MSVC /Zs without objects or execution. It specifies
capabilities, guards, frame mismatch, readOnly preference, wire bytes/base64,
partial/missing counts, malformed responses, bounds and stale-reference omission.
Actual CyToolsCore manifest/schema examples check all 177 operations, typed mutation
states, required context, permissions and noncancellable handlers. See
[Validation](Validation.md) for production build and delivery checks.

Actual CodeLLDB/NetCoreDbg/java-debug/plugin adapters, target expression evaluation,
memory protection/partial writes, timeouts/late events, concurrent clients and
GUI/keyboard/themes/DPI remain unexecuted during the Kaspersky test suspension.
Linux/macOS compilation and runtime acceptance remain pending. Source/data checks
and successful compilation do not establish those runtime behaviors.

Primary contract: [DAP requests and capabilities](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

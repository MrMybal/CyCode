# Advanced DAP inspection and breakpoints

Instruction breakpoints now share the native worker and disassembly snapshot.
See [Instruction breakpoints](InstructionBreakpoints.md) for session-only full-list
edits, conditions/disabled entries, actual verification and pending acceptance.

Forward stepping options, reverse execution and frame restart now share this DAP
worker. The controls respect advertised capabilities and actual execution events;
see [Execution controls](DebugExecution.md) for context guards and pending acceptance.

Capability-gated data breakpoint inspection/replacement now shares this owner.
Handles expire with the pause; installed targets can remain across resume.
See [Data breakpoints](DataBreakpoints.md) for session-only persistence, full-list
edits, revision/uncertainty guards and pending acceptance.

Paused variable/expression setters and guarded memory writes also use this worker.
See [Value and memory editing](DebugValueEditing.md) for capabilities, explicit
context guards, read-back/uncertain outcomes and pending real-adapter acceptance.

Debugger's Scopes / Registers, Memory, Disassembly and Function / Exception
breakpoints tabs use the existing native DAP owner. GUI, SDK/MCP and native plugin
requests call the same service. No synthetic target values, separate debugger
process implementation or UI dependency enters the native core.

## Inspection

`debug.memory` / `debug_memory` uses `readMemory` only when
`supportsReadMemoryRequest` is advertised. A request has an opaque
`memoryReference`, byte `offset` (±1 MiB) and `count` (1–4096). Native validation
checks canonical base64, response byte limits and unreadable counts. The returned
`body` contains the adapter's address/data plus decoded integer `bytes` and
`unreadableBytes`. The GUI shows a read-only hexadecimal/ASCII view. Evaluate a
pointer expression in Watch, then use its returned memory reference when the
adapter supplies one. The separate explicit write controls require both read/write
support and a current readable snapshot; see [Value and memory editing](DebugValueEditing.md).

`debug.disassemble` / `debug_disassemble` uses a capable adapter's `disassemble`
request with bounded byte/instruction offsets and 1–200 instructions. The GUI can
use the selected stack frame's instruction reference and shows addresses, bytes,
instructions and symbols. Supplied source locations use existing registered-source
navigation; they do not grant arbitrary disk access. Source/instruction breakpoint
editing and reverse debugging are separate work.

`debug.variables` / `debug_variables` accepts `start`, `count` (1–200), optional
`filter` (`named` or `indexed`) and the live `variablesReference`. Child trees have
page controls. Scopes / Registers can explicitly read expensive scopes, including
registers when the adapter supplies such a scope. There is no invented universal
register request. A full page means another page may exist; an empty next page
marks the end when the adapter does not report a total.

`debug.exceptionInfo` / `debug_exception_info` retrieves actual exception details
for the selected stopped thread when `supportsExceptionInfoRequest` is advertised.
All these operations require `process.execute`. Their output is persistent debugger
state/data, not a job artifact or a workspace source document.

## Stop generations and stale references

Read/step operations accept `stopGeneration` from `debug.status`. Omitting it pins
the current generation when the request is queued. Target value/memory setters
instead require explicit session/stop generations. Inspection queued for an older
stop is rejected before dispatch; memory, disassembly, exception, variable,
evaluate and source responses are rejected if execution resumes/stops again while
waiting. Resume clears GUI caches, and scopes/watches do not publish late stopped
results. Step/continue requests also reject a mismatched observed stop.

These checks cannot roll back side effects of an Evaluate command already accepted
by the target. Explicit evaluation retains its existing side-effect semantics.

## Persistent breakpoints

`debug.functionBreakpoints` / `debug_function_breakpoints` replaces up to 128 named
function breakpoints. Entries support enabled, condition and hitCondition, with
actual function/condition/hit-count capability checks. Only enabled entries bind;
the UI distinguishes adapter verification and messages. Deferred breakpoint events
update verification without fabricating a source location.

`debug.exceptionBreakpoints` / `debug_exception_breakpoints` selects filter IDs from
the actual adapter's `exceptionBreakpointFilters`. `filters:null` uses advertised
defaults, `[]` disables all, and an explicit list must contain known unique IDs.
Both built-in and contributed adapters receive configuration before
`configurationDone`. No universal C++/.NET exception names are guessed.

Both setters support an `expected` snapshot, require `process.execute` and
`workspace.write`, and persist alongside source breakpoints/profile in
`.cycode/launch.json`. A nonblocking native lease serializes competing writers;
baseline-checked atomic writes reject a file changed since
this instance loaded/saved it. Unsupported stored enabled functions/exception
filters fail the next launch explicitly. Confirmed breakpoint locations are
session-owned and cleared when the adapter closes. The plugin host exports these
same guarded operations through its versioned JSON/C ABI.

## Verification and boundaries

Windows production app/core/CLI compilation and source-only MSVC `/Zs` checks
passed. Added regression sources cover malformed base64, unreadable ranges,
request/response bounds, variables, capability checks, function conditions,
exception defaults and explicit filter selection. The existing DAP integration
test also specifies stale-stop rejection and conditional actual memory/disassembly
queries when the installed engine advertises them.

Those native/MCP/GUI tests have not been executed during the Kaspersky interruption.
No debugger, debuggee or GUI acceptance was launched for this change. Actual
CodeLLDB/NetCoreDbg/contributed-adapter behavior, themes/DPI/keyboard and Linux/macOS
remain unverified. A capability branch and successful compilation are not runtime
acceptance. Data breakpoints and value/memory setters now have shared native
implementations with acceptance still pending. Dedicated machine-register commands,
remote/core dumps and
full Unreal visualizers/Live Coding remain separate. Reverse execution and frame
restart now have shared paths with real-adapter acceptance still pending.
Targeted step-in and set-next-statement also have shared guarded native/SDK/plugin
paths with pending acceptance; see [Targeted step-in](StepInTargets.md) and
[Set next statement](GotoTargets.md). The latter skips code instead of executing
through to a source position.

Run to cursor now has a shared native/SDK/plugin path with a verified temporary
source breakpoint, one Continue and guarded original-list restoration. Actual
adapter/cleanup/concurrency/GUI acceptance remains pending; see
[Run to cursor](RunToCursor.md).

Primary contract: [Debug Adapter Protocol specification](https://github.com/microsoft/debug-adapter-protocol/blob/main/debugAdapterProtocol.json).

## Exception binding evidence

`debug.status.exceptionBreakpointStatus` separates `acknowledged` (the adapter
accepted setExceptionBreakpoints) from `verificationReported` and the actual
bounded `breakpoints` verification rows. An advertised capability or an empty
reply never means a thrown exception was observed. Failed reconfiguration keeps
an unacknowledged state; shutdown clears prior binding evidence.

The Windows production check records CodeLLDB accepting `cpp_throw` while
reporting `verified:false`, "Resolved locations: 0", and letting an MSVC C++
throw reach the program's handler. An explicit function breakpoint on the
installed MSVC runtime's `_CxxThrowException` does stop that same production
throw. This is a separate, opt-in MSVC recipe through function breakpoints,
not DAP exceptionInfo and not an automatic hidden fallback. Preserve existing
function breakpoints and their expected-list guard when adding such a recipe.

Actual conditional function stops, instruction binding/clear, 19-thread pause
and selection, frame/stack/memory/disassembly, stale-stop refusal and attach/
detach are exercised on the existing production Core with its real CodeLLDB.
After detach, that target still answers its own native service.status. Wait for
actual stopped-state threads/frames: the event can arrive before inspection.
Remote machines, other adapters, complete exception semantics and graphical
controls remain separate acceptance scopes.

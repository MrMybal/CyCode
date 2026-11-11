---
id: analysis-progress
title: Persistent analysis progress
description: Persist native compiler traversal, distinguish historical evidence and measure large projects.
section: Start here
parent: native-core
order: 265
revision: 3
---


## Scope and evidence {#scope}

`analysis.prepare` traverses real compiler commands, one temporary native buffer at
a time. Existing editor buffers retain ownership. Inventory, visited files, current
compiler diagnostics and whole-project index completeness remain distinct.
The same owner serves native clients and the official CyToolsCore SDK/MCP.

## Save and restore traversal {#persistence}

Opt into native metadata under `.cycode/analysis-progress`:

```json
{"scope":"compilation-database","persist":true,"batchSize":64}
```

Keep the issued `checkpoint` ID. Pending work in the current session uses
`continuation`. After shutdown, discover saved IDs with `analysis_checkpoints`
and restore through `analysis_prepare`:

```json
{"checkpoint":"analysis-0123456789abcdef0123456789abcdef","batchSize":64}
```

The identifier above illustrates the format; use the actual issued value. An
optional batch size of zero inventories or restores metadata without parsing a
source. No fixed 128-file traversal ceiling exists. The initial response contains
at most 128 rows; `analysis_results` pages subsequent metadata in groups up to 256.

Restoration reuses traversal order, not compiler proof. All restored rows become
`diagnostics: unknown` with `evidence: historical-checkpoint`. Their old state is
available as `previousDiagnostics`; old revision numbers are historical metadata,
never current mutation capabilities. Read a live document before using a revision.
`historicalObserved` and `currentPassObserved` expose the distinction. The same
restoration works after a live-document change in the same Core process, explicitly
rebasing observations rather than silently reusing stale results.

The compilation database bytes use BLAKE2b-256; its configuration/path and metadata
must also match. Changes require a new inventory. An OS lease protects one writer,
paths remain workspace-relative, redirected cache paths are refused, and cache
format 2 uses immutable pages (up to 256 entries / 16 MiB each) and an atomic
32 MiB manifest. Aggregate progress can exceed 32 MiB; format 1 remains readable.
Only new/changed record pages are published. Saved progress contains no source text. Atomic metadata
publication reuses `CyCode::Files`. Cancellation preserves the completed prefix.
Ordinary restart persistence does not establish power-loss recovery or compiler
index persistence. Cache IDs are not access credentials or independent SDK jobs.

## Isolated source failures {#failures}

Unreadable, invalid UTF-8 or oversized text sources produce an explicit unknown
row with an error. An unopened file has revision zero, which cannot authorize an
edit. Other units continue. Cancellation, source/workspace revision conflicts and
backend failures retain their distinct operation failures; they are not converted
into clean diagnostics. Compiler errors also remain visible, without aborting a
read-only traversal. Rename still requires its stricter all-candidate proof.

## Measure actual work {#measurement}

Each processed source reports `elapsedMilliseconds`. While running,
`analysis_status` exposes `currentPath` and `currentFileMilliseconds` without
requesting another compilation. Aggregates retain clean/error/unknown/remaining
counts. None of these certify exhaustive semantic coverage of Unreal macros,
Blueprints, all configurations or the background index.

The opt-in `app/scripts/benchmark_unreal_campaign.py` drives the genuine UBT
database through official MCP, retains raw batches and per-file timings, samples
the owned native process tree, and checks selected source/database hashes.
Time or unit budgets leave a pending checkpoint instead of claiming completion.
Use the same clangd arguments when restoring a checkpoint.

```text
python app/scripts/benchmark_unreal_campaign.py --run-native --core . --executable <Core> --clangd <clangd> --workspace <UE-root> --database <actual-UBT-compile_commands.json> --max-seconds 600 --max-units 0 --output <new-results>
python app/scripts/benchmark_unreal_campaign.py --run-native --core . --executable <Core> --clangd <clangd> --workspace <UE-root> --database <actual-UBT-compile_commands.json> --checkpoint <issued-id> --output <new-results>
```

## Validation {#validation}

`verify_analysis_persistence.py --run-native` exercises real MSBuild metadata,
clangd and official MCP on more than 128 source files, normal restart,
cancellation, malformed/context guards and writer ownership. No generated native
test executable or graphical IDE is launched. `verify_inspection_edges.py` tests
actual clang-tidy result bounds and missing compiler commands. These runners are
explicit Windows/MSBuild checks; production source checks never start them.


The navigation cache now separates unique document revisions from read-only
analysis-draft allocations. Traversal preserves unchanged semantic snapshots;
real edits, compiler-context changes and backend/watcher guards still invalidate
results. `verify_navigation_analysis.py --run-native` checks this with actual
compiler requests, not a mocked cache.

The Windows evidence retained in the source benchmark report records 37,557 actual
Unreal translation units inventoried, 104 observed in a budgeted first process,
then 64 additional clean observations after restart. At that point the previous
104 are explicitly unknown historical observations and 37,389 remain unvisited.
The scoped DateTime/HTTP three-file rename still passes Apply/Undo without saving
Unreal sources. This is not exhaustive Unreal validation.

On a small MSBuild navigation fixture, 16 concurrent SDK/MCP requests retained
16 cache hits after the fix versus 3 before; median round trip changed from about
72 to 37 milliseconds. These local samples do not establish whole-IDE performance
or equivalence to Rider/CLion. Eight actual clangd transformations separately pass
preview/Apply/Undo, including two-file function-body moves. Actions not offered by
the tested backend are recorded explicitly.

## Use the IDE view {#ide}

In CyCodeIDE, open **View → Analysis and inspections → Analysis progress**. The
panel separates service availability, each language backend, the file inventory,
active-document diagnostics and source traversal. Choose the active C++ file or
the actual compilation database, set a bounded batch and explicitly prepare it.
**Pause analysis** cancels the operation issued by this view; **Resume next batch**
requests the next portion. Opening the panel does not start a compilation campaign.
The stopped Unreal campaign is not resumed automatically.

Opt into a durable traversal checkpoint for a compilation-database scope. Discover
existing checkpoints and restore one without compiling (batch zero), then request
the next batch explicitly. Result pages retain per-file errors and timings.
Historical and current-pass observations remain separate; old rows cannot
authorize edits. Source traversal percentages do not measure complete compiler
index coverage.

The References panel's **Refresh at caret** requests `refresh: true` for the selected
navigation operation. Use it when index progress leaves a cached initial result.
Automatic cache invalidation from background index progress remains unverified.

Production compilation with offline help passed on Windows; desktop interaction acceptance is still pending.

## Limits {#limits}

Restored files are skipped as historical traversal; they are not freshly
revalidated. Start a new inventory for a complete fresh pass. The current
editor/source-buffer size bound remains; failed files are reported, not silently
claimed supported. Cache discovery is bounded and reports invalid/truncated
entries without deleting user data. New Linux/macOS, power-loss, malicious
filesystem races and full graphical acceptance remain unverified. GitHub Actions
remains disabled. No release or installation update follows automatically.

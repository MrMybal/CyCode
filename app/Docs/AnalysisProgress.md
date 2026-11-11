# Persistent analysis progress

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

The [Windows evidence](Benchmarks/core-scale-20261111.json) records 37,557 actual
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

## Limits {#limits}

Restored files are skipped as historical traversal; they are not freshly
revalidated. Start a new inventory for a complete fresh pass. The current
editor/source-buffer size bound remains; failed files are reported, not silently
claimed supported. Cache discovery is bounded and reports invalid/truncated
entries without deleting user data. New Linux/macOS, power-loss, malicious
filesystem races and full graphical acceptance remain unverified. GitHub Actions
remains disabled. No release or installation update follows automatically.

## Full-length local campaign {#long-campaign}

The explicit production runner now accepts `--max-seconds 0` to continue the
remaining compiler inventory without a duration cutoff. Its default remains a
bounded measurement. `--max-units 0` traverses the remaining inventory; a positive
value is an explicitly chosen test scope. `--stop-file <path>` requests cooperative
cancellation when that file appears, retaining the completed native checkpoint.
The stop file is not removed. Continue in-process while possible; after shutdown,
restore the issued checkpoint rather than restarting the prefix.

```text
python app/scripts/benchmark_unreal_campaign.py --run-native --core . --executable <Core> --clangd <clangd> --workspace <Unreal-source-root> --database <real-UBT-compile_commands.json> --output <new-directory> --checkpoint <issued-checkpoint> --max-seconds 0 --max-units 0 --stop-file <owned-stop-file>
```

The runner hashes upcoming sources in bounded batches, rather than reading every
source before the first compiler batch. Source/database digests stream through
64 KiB blocks. `documents.jsonl` retains all fresh observations incrementally;
the report's `documents` contains only the latest batch. A final digest binds the
observation log. Read-only source checks cover each observed batch. A budget/stop
is reported pending, never whole-engine completion. This script is not an OS
service and does not authorize leaving antivirus alerts unattended.

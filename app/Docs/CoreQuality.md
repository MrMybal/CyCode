# Core quality and comparative validation

## Shared atomic publication {#atomic}

`CyCode::Files` owns `AtomicFile.h` and one native byte writer. Text documents,
CMake metadata, MSBuild metadata and private endpoint descriptors call this owner.
Temporary names are independent siblings, created exclusively. Writes are flushed
before publication and an owned temporary is cleaned on ordinary failure. Windows
partial ReplaceFile failures retain the remaining replacement bytes.

Callers choose exclusive creation, replacement preserving metadata, or replacement
of the newly protected file. Endpoint permissions are supplied and verified before
credential bytes are written: a protected user/LocalSystem DACL on Windows, owner
0600 on POSIX. Text retains read-only and disk-baseline checks. Generated metadata
has a 32 MiB bound and an unchanged-content fast path; MSBuild retains its bounded
retry for transient Windows sharing failures. Validation is repeated immediately
before publication; it is not an OS-level compare-and-swap. Source API 1 and both
existing plugin ABIs remain compatible. No UI dependency is introduced.

## MSBuild ownership {#msbuild}

Project selection, artifact metadata and compilation-index preparation have
separate helpers. Control flow is formatted for review instead of chained across
hundreds of characters. Cyclic or excessively nested solution folders are refused.
Evaluation continues to use actual Visual Studio MSBuild metadata, including
includes, definitions, language mode and exception handling. Existing explicit
project-selection and size bounds remain; preparing one profile does not certify
every configuration or every project in a solution.

## C++ inspections {#inspections}

`analysis_inspect` / native `analysis.inspect` uses an installed clang-tidy and one
saved translation unit with exactly one entry in an actual workspace
`compile_commands.json`. Read the current document first:

```json
{"parameters":{"path":"Source/Example.cpp","expectedRevision":7,"compilationDatabase":"build/compile_commands.json","profile":"recommended","timeoutSeconds":180}}
```

The official CyToolsCore runtime owns jobs and permissions. `process.execute` is
required; no workspace write permission is needed. A missing tool reports
BackendUnavailable. `recommended` enables Clang Static Analyzer, bugprone,
performance and portability checks. `security`, `modernize`, `performance` and
`portability` are also available. No guessed compiler flags, `-fix`, arbitrary
argument list or source rewriting is supplied by this operation. The selected
project's compiler/configuration inputs remain trusted executable tooling inputs.

Dirty/conflicting live documents are refused. Document identity/revision,
workspace observations, saved source and database bytes guard the result. A
changed observation returns `status: stale`. Compiler coordinates are one-based
UTF-8 byte columns (`byteColumn`), not editor UTF-16 positions. The result contains
bounded actual diagnostics, output and process/cancellation status. Use the
existing build status/output/cancel operations for its native process owner.
Each request analyses one translation unit and included headers; agents can
iterate an explicit scope without claiming whole-project completeness.

## Guarded C++ transformations {#refactoring}

Existing code-action listing/preparation now supports clangd's ExtractFunction,
ExtractVariable, InlineVariable, ExpandMacro, ExpandAutoType, DefineInline,
DefineOutline, RemoveUsingNamespace, ScopifyEnum and SwapIfBranches when the server
actually offers them. These are a fixed allowlist, not arbitrary command execution.

The server computes an edit; its `workspace/applyEdit` is always refused. One
captured edit is validated by the existing native preview/Apply/Undo transaction.
Source URI, range, document revision, workspace and backend/session guards remain.
No source is saved automatically. Unsupported file operations remain refused.
An interrupted command preview requires restarting the language backend before
another command preview, preventing an uncorrelated late edit from being reused.

## Repeatable measurements {#benchmarks}

The [recorded evidence](Benchmarks/core-quality-20261111.json) retains raw samples,
source hashes, actual tool discovery, cancellation, debugger and scoped Unreal
outcomes. No endpoint token, private job or machine-specific source root is included.

The explicit `app/scripts/benchmark_inspections.py --run-native` runner accepts
an existing Core executable, workspace compilation database, selected source files,
clang-tidy, and optionally a JetBrains `jb` command and solution. It records raw
samples, median/p95, source/context hashes, actual findings and observed process-tree
working-set sums. psutil memory sampling is optional and declared unavailable when
absent. It samples every 50 ms, may miss brief peaks and can count shared pages more
than once. Existing output directories are preserved.

A Windows comparison used the actual two-unit project and InspectCode 2025.1.5,
matching the installed Rider version. Core's median was approximately 0.10 seconds
for two inspection requests; InspectCode's approximately 5.47 seconds for a fresh
solution-inspection process. Both identified the intentionally uninitialised value.
These are different check suites and process lifecycles on a tiny corpus; they do
not establish faster IDE navigation, equal diagnostic coverage or lower overall
memory. OS caches were not flushed and only three samples per engine were taken.

## Reproduce validation {#validation}

Use the existing private Python/CyToolsCore/MCP runtime and accepted production
Core. Every runner requires explicit native execution; ordinary source checks do
not start it. Output must be a new owned directory outside the measured sources.

```text
python app/scripts/verify_core_quality.py --run-native --core . --executable <Core.exe> --clangd <clangd> --clang-tidy <clang-tidy> --output <new-results>
python app/scripts/verify_core_debugger.py --run-native --core . --executable <Core.exe> --adapter <codelldb> --output <new-results>
python app/scripts/verify_core_unreal.py --run-native --core . --executable <Core.exe> --clangd <clangd> --workspace <existing-UE> --database <actual-UBT-database> --output <new-results>
python app/scripts/verify_inspection_cancellation.py --run-native --core . --executable <Core.exe> --clang-tidy <clang-tidy> --output <new-results>
python app/scripts/benchmark_inspections.py --run-native --core . --executable <Core.exe> --workspace <project> --database <relative-compile_commands.json> --source <unit.cpp> --clang-tidy <clang-tidy> --output <new-results>
```

Windows evidence includes real SDK discovery, two MSBuild commands, Debug/Release
context changes, definitions/references, three-file rename, variable/function/macro
previews with Apply/Undo, live-header invalidation, an actual Clang Static Analyzer
finding, revision rejection, repeated saves, read-only refusal and disk-conflict
preservation. Real CodeLLDB stopped the production Core at entry, exposed its stack,
memory and disassembly, refused a stale stop and drained normally after continue.
No generated test executable or graphical IDE is launched by these checks.

The Unreal regression uses the existing real UBT database, inventories the scope,
continues a small batch and exercises DateTime/HTTP rename/Apply/Undo without saving
engine sources. Exact counts and timings belong to the recorded benchmark evidence.

## Remaining scope {#limits}

Current runs validate Windows x64. POSIX implementation remains in place; new
Linux/macOS execution, graphical acceptance, exhaustive Unreal semantic coverage,
Blueprint/asset refactoring, remote debugging, full data-flow coverage and complete
Rider/CLion feature parity are not established. Operation registration, a compiler
success or a quick benchmark is not proof of these capabilities. The installed
0.1.1 preview is separate from this development source; these changes need their
own accepted package before updating other clients. GitHub Actions remains paused.

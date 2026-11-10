---
id: native-core
title: Native Core and application integration
description: Shared native documents, project analysis and persistent headless services.
section: Start here
parent: overview
order: 3
revision: 12
---

## Ownership {#ownership}

CyCode provides the native C++20 Engine and CLI/service. CyCodeIDE owns the
SDL/OpenGL/CyUI interface. CyCodeReader consumes the Core's standalone text
session: documents, codecs, search, text plugins and native transport. Its
graphical editor remains a shared presentation adapter. Both native profiles
use the same BufferState and guarded file primitives; project and standalone
path policies stay explicit. Reader does not start language servers or gain
project semantics merely by linking the text profile.

The existing IDE guides and media remain useful for the extracted application.
Published 0.2.3 packages predate this separation; development sources do not
establish a new installer/release. Native plugin C ABIs remain compatible within
their original profiles; project and text plugin binaries are not interchangeable.

## Build without graphics {#build}

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

Core configuration/fetching needs no Reader, SDL, ImGui, CyUI, OpenGL or Tree-sitter.
Use CyCode::Core for the project Engine. Smaller hosts can select Documents,
TextSession, TextBridge, Process, Transport, Protocol, Files and Snippets through
CYCODE_COMPONENTS or cmake/CyCodeComponents.cmake. Dependencies resolve automatically;
the standalone text profile needs only the pinned JSON dependency. Source API 1
is a source integration contract, not a stable C++ binary ABI.

```sh
python app/scripts/fetch_dependencies.py --components TextSession TextBridge Process
cmake -S . -B runtime/text -DCYCODE_COMPONENTS="TextSession;TextBridge;Process" -DBUILD_TESTING=OFF
cmake --build runtime/text --config Release
```

Each Engine owns one project; keep it alive across requests and join callers
before destruction. Ordinary builds never execute applications or native tests.

## Persistent service {#contract}

`cycode_cli --disable-plugins <project> --stdio` accepts cycode-core/1 objects with
an id, method and object params. Replies preserve the id and return result or a
structured error with code/message and optional details. Revision/disk conflicts,
read-only edits and migrated backend failures retain their owner-issued codes.
Legacy runtime failures remain OperationFailed; no error-prose matching is used.
Requests are bounded to 8 MiB, responses to 20 MiB and string ids to 256 bytes.

service.shutdown or EOF drains the stdio host without saving buffers. The loopback
descriptors cycode-native/1 and cycodereader-native/1 retain compatibility and
advertise coreProtocol. Socket shutdown remains owned by the parent through
stdin/EOF. Transport joins handlers, bounds clients/messages and preserves a
newer instance's descriptor. CyToolsCore continues to own MCP, jobs, identities,
permissions and scheduling. MCP `service_status` exposes lifecycle inspection;
trusted native shutdown remains an owning-host control. See the
[AI integration guide](doc:core-agents#connect) for preparation and jobs.

service.status describes lifecycle/context. language.status.servers describes
each backend's configuration, observed launch, initialization, capabilities,
epoch, sent document revisions and version-bound diagnostics. An initialized
backend or queued text notification does not prove complete analysis. Whole-project
semantic-index completeness remains unknown; path caches and literal search
are separate from clangd's semantic index.

## Validation and measurements {#validation}

Core revision 84e4c78 passed the 63-case native suite on Windows x64, Ubuntu x64
and macOS arm64 (MSBuild is skipped on POSIX). Its POSIX Core case includes
80 concurrent child ownership probes. The corrected process-group postcondition
also passed the formerly failing macOS Git replay and three extra repetitions.
Reader 671c588 passed 17 service/view cases on those systems, including native
plugins, codecs, live edits, guarded IPC, snippets, comparison and Git history.
Those headless views do not establish GPU, IME or physical desktop acceptance.

The runner-only semantic benchmark completed both a deterministic synthetic
corpus and the actual native Core CMake project: 121 translation units and 230
owned source/header files. One Linux real-project run measured warm medians of
8.88 ms definition, 8.79 ms references, 303.82 ms completion and 18.55 ms rename
preview. First version-bound diagnostics took 2.91 seconds; instantaneous summed
CLI/backend RSS was 1,234,755,584 bytes. Five samples are a baseline, not a universal
claim; RSS can count shared pages twice and is not peak/private memory. The
answers do not prove complete reference coverage. No CLion/IntelliJ comparison
was performed. Raw reports and exact CI links are retained in the architecture guide.

A second post-consolidation real-project run at d0b1791 also passed with 121
translation units and 227 owned files: 9.31 ms definition, 8.74 ms references,
269.26 ms completion and 18.52 ms rename preview (warm medians). First current
diagnostics took 2.26 s; unsaved edit/diagnostic/definition median was 550.76 ms.
The corpus changed, so these observations are not an optimization comparison.
The architecture guide retains both raw reports and exact workflow revisions.

`app/scripts/benchmark_semantic.py --ci` executes only on an explicit runner.
`--prepare-only` creates source data without native execution. Local native runs
remain suspended during the existing antivirus investigation; production builds
and data/syntax checks are recorded separately from runtime acceptance.

The native response path now wakes its owner when a backend message arrives.
Protocol/semantic callbacks remain on that owner thread, with joined transport
lifetime and a guarded wake predicate. On the identical synthetic corpus, two
runner process runs observed definition medians near 5.08 ms instead of 8.3 ms,
and rename preview near 10–11 ms instead of 19 ms. The real Core project run
also completed, but completion and edit-to-analysis were slower in that
observation. This is not a general speedup claim or a same-machine comparison.
The architecture guide retains every raw report and exact workflow revision.

## Large projects and continuing enumeration {#large-projects}

File discovery now continues in an owned background stream, publishing immutable
sorted batches. indexing/complete distinguish progress from an ended snapshot;
page limits do not truncate the whole project. The IDE polls the evolving index
and builds the tree off the UI thread. Shutdown/refresh stop the owned process.
Actual failures remain explicit; path-index memory scales with the tree.

An explicitly owner-authorized Windows production-Core test used a real Unreal
5.5.4 engine source checkout and 37,679 UBT commands. Its 177,797 ignore-aware
paths exactly matched an independent enumeration. Actual Core/CoreUObject
diagnostics, navigation, completion, local rename preview and Apply/Undo were
checked without saving source files. A rapid Undo diagnostic-publication issue
was reproduced and fixed using clangd's transaction checkpoint. The source guide
retains aggregate timings and precise limits. Manual UBT export was required:
automatic engine-only Unreal preparation and whole-index completeness are not
established. This scoped test does not resume local GUI/native-fixture launches.

The local production Core now passes the actual Unreal three-file
`FDateTime::ParseHttpDate` rename on the first request, plus Apply/Undo and changed
scope refusal. Definition coordinates survive a valid header edit and restoration;
ten repeated World.cpp queries measured a cache median of about 0.52 ms. This is
one warming Windows run, not a controlled comparison to earlier samples.

The earlier real MCP acceptance covered 236 domain operations and 21 SDK controls; the current contract adds `analysis_results`. Read-only
permissions, progress, owner cancellation through native stop, native error codes,
shared Unicode buffers, private jobs and normal Core exit were checked. See the
[agent workflow](doc:core-agents#connect) for exact contracts and limits, including
rolling preparation, resumable batches, paginated coverage and unknown whole-engine semantic completeness.

The 128-file ceiling is removed: 160 actual compiled Runtime/Core sources
passed in four batches with current clean diagnostics, paginated observations and
three retained editor buffers. The inventory counts 37,557 unique Unreal units;
this small selection does not establish exhaustive engine semantics.

The subsequent 237-operation MCP run passed discovery, permissions, progress,
cancellation and checkpoint/page contracts, but was interrupted between Apply
and Undo. The owner reported a matching Kaspersky PDM alert on the production CLI.
Local native runs and rebuild/retry are suspended; the final MCP acceptance is
not marked passing. Disk sources stayed unchanged. Source/data/documentation
checks continue; no protection bypass or automatic restart was introduced.

**Current delivery:** independent sibling temporary names correct the observed
save/publication pattern. The owner authorised the corrected production build.
Real CyCodeIDE MCP search/navigation and twelve save/recovery cycles passed
with Kaspersky active and no new alert observed. The investigations below
record earlier incidents, holds and interrupted acceptance; they are not
the current installation instructions. No vendor false-positive verdict is
claimed. See [Core installation](core-installation#distribution).

## Windows antivirus investigation {#antivirus}

The owner reported a PDM alert on the production CLI. Its exact detected behaviour
is unknown; two recovered avp Windows events now identify its original SHA-256,
even though the executable is unavailable at its build path. Local native runs and rebuild/retry remain suspended. Windows source
changes now restrict child handle inheritance and refuse language-server launch
unless job ownership and thread resume succeed. MSVC syntax-only checks pass;
native behaviour and the antivirus outcome have not been retested.

`app/scripts/antivirus_evidence.py` prepares a local JSON report and vendor request.
It hashes only explicitly selected files, never runs/copies/rebuilds the sample,
never includes private report contents and never uploads data. A missing original
or hash remains explicit, rather than being replaced with a different build.

No paid signing certificate is required for this investigation. The
[official reanalysis procedure](https://support.kaspersky.com/common/error/other/1870)
and [free developer program](https://www.kaspersky.com/partners/allowlist-program)
provide vendor review paths, subject to eligibility and assessment. No submission,
protection change or false-positive confirmation has occurred. The detailed
antivirus event/export is still required to identify the flagged operation.

The transport's native credential now uses OS entropy and a descriptor created
privately before token bytes are written: protected user/LocalSystem ACLs on
Windows, mode 0600 on POSIX. Atomic publication avoids inherited old file ACLs
and a chmod-after-publication interval. The protocol and SDK ownership stay
unchanged; native acceptance remains unexecuted. The evidence tool can import
explicit avp event XML while excluding private command lines and rejecting
conflicting original hashes. No quarantine or protection setting was accessed.

## Remaining stages {#limits}

Large Unreal/MSBuild performance acceptance, complete context/dependency
invalidation coverage, incremental synchronization, concurrent/cancellation
load baselines and advanced command-based transformations remain further work.
Existing semantic edits retain previews, revision/disk checks, explicit saves
and Undo. Unsupported backend actions stay unavailable. IDE presentation and
installer/update identity need their own acceptance before a CyCodeIDE release;
the historical updater is disabled in this development build.

The owner later authorised production installation and MCP validation for this
delivery. The installed runtime completed real CyCodeIDE search, 62-unit MSBuild
evaluation, clean diagnostics on two observed files, compiler navigation and
guarded guide authoring. Kaspersky deleted that installed CLI again at 19:20
Europe/Paris. Its MD5 matches the built payload; full installed acceptance is
interrupted, MCP is disabled and binary publication is suspended. No automatic
native retry followed. See [Core installation](core-installation#distribution).

---
id: native-core
title: Native Core and application integration
description: Shared native documents, project analysis and persistent headless services.
section: Start here
parent: overview
order: 3
revision: 4
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
permissions and scheduling; these host controls are not new SDK/MCP operations.

service.status describes lifecycle/context. language.status.servers describes
each backend's configuration, observed launch, initialization, capabilities,
epoch, sent document revisions and version-bound diagnostics. An initialized
backend or queued text notification does not prove complete analysis. Whole-project
semantic-index completeness remains unknown; path caches and literal search
are separate from clangd's semantic index.

## Validation and measurements {#validation}

Core revision d0b1791 passed 63 native tests on Windows, Linux and macOS; the
Windows-only MSBuild case is skipped on POSIX. macOS replay also passed three
repeats, while earlier intermittent failures remain a diagnostic item. Reader
revision c4a013b passed seven native integration tests on all three platforms,
including encodings, plugins, live revisions, disk conflicts, IPC and shutdown.
Changes after those revisions require their own acceptance run.

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

## Remaining stages {#limits}

Large Unreal/MSBuild performance acceptance, complete context/dependency
invalidation coverage, incremental synchronization, concurrent/cancellation
load baselines and advanced command-based transformations remain further work.
Existing semantic edits retain previews, revision/disk checks, explicit saves
and Undo. Unsupported backend actions stay unavailable. IDE presentation and
installer/update identity need their own acceptance before a CyCodeIDE release;
the historical updater is disabled in this development build.

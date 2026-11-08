---
id: native-core
title: Native Core and IDE extraction
description: Reuse the native engine independently of the desktop interface.
section: Start here
parent: overview
order: 3
revision: 1
---

## Ownership {#ownership}

CyCode now owns the native C++20 Engine and CLI/service. CyCodeIDE owns the
SDL/OpenGL/CyUI interface. CyCodeReader and the shared graphical editor consume
CyCode's common native Paths, TextFile and Snippet implementations (source API 1).
Reader's richer document/search session has not yet been migrated to Engine.
The existing IDE guides remain useful for the extracted interface; published
0.2.3 downloads describe the application before this repository separation.

## Build without graphics {#build}

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

Core needs no Reader, SDL, ImGui, CyUI, OpenGL or Tree-sitter checkout. Native hosts
may add the source directory and link `CyCode::Core`. Small editors may import only
`common/` and link `CyCode::Files` / `CyCode::Snippets`. Each Engine owns one project.
Ordinary builds never execute an application or native tests.

## Persistent service {#contract}

`cycode_cli --disable-plugins <project> --stdio` emits a ready JSON line, then
accepts request objects using `cycode-core/1`, an id, method and object params.
`service.status` exposes lifecycle and context identity. `service.shutdown` or EOF
drains the host through existing Engine destruction without saving documents.
Requests reuse the same Engine/backend workers. Errors have code and message;
legacy Engine failures conservatively use OperationFailed.

The existing authenticated `--serve <descriptor>` remains compatible with
`cycode-native/1`. Its parent owns shutdown through stdin/EOF. The new stdio
control operations are not additional SDK/MCP commands. `language.status` reports
the existing backend readiness, capabilities, epochs and revision-bound diagnostics.
Initialized does not mean the whole project's semantic index is complete.

## Validation and measurements {#validation}

Windows production compilation covers Core/CLI, the extracted IDE and Reader CLI.
Native execution remains suspended locally while the existing antivirus removals
are unresolved. Data-only checks and compiler syntax checks do not establish
post-extraction runtime acceptance. Linux/macOS need fresh builds and runtime checks.

The explicit runner-only `app/scripts/benchmark_semantic.py --ci` workflow measures
actual native/clangd requests on a deterministic synthetic corpus, including JSON
transport, process startup, version-bound diagnostics, navigation, references,
completion, guarded rename preparation, unsaved edits and scoped memory samples.
`--prepare-only` creates source data without native execution. No timings have yet
been recorded for this extraction; synthetic results will not prove large-project
performance or parity with another IDE.

## Remaining stages {#limits}

Real CMake/MSBuild/Unreal baselines, context invalidation, incremental synchronization,
advanced semantic transformations and broader Reader Engine reuse remain next stages.
The IDE's installer/update identity must be migrated before publishing its release.
The extracted development build disables the old updater to avoid offering an
installer for the previous application. Existing transaction/revision/disk guards
and explicit saves remain authoritative.

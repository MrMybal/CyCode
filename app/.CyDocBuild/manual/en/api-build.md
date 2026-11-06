---
id: api-build
title: CMake builds
description: Public operations for CMake builds.
section: API and extensions
parent: api-links
order: 102
revision: 2
---

Public operations for CMake builds.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`build_preset_plan`](doc:operation-build-preset-plan-build-preset-plan) — Inspect a test/package/workflow preset without executing CMake, CTest, CPack or project scripts.
- [`build_preset_run`](doc:operation-build-preset-run-build-preset-run) — Execute the reviewed CTest/CPack/CMake workflow preset through the existing native build owner.
- [`build_inspect`](doc:operation-build-inspect-build-inspect) — Inspect the saved build profile and CMake File API target/toolchain metadata without executing project code.
- [`build_presets`](doc:operation-build-presets-build-presets) — Read all five CMake preset families, included workspace files, inheritance, linked binary/output paths and workflow steps without executing project tools.
- [`build_status`](doc:operation-build-status-build-status) — Inspect the currently owned CMake, MSBuild or explicit Unreal build operation.
- [`build_location`](doc:operation-build-location-build-location) — Resolve one compiler display line to an existing workspace file without executing tools or reading file contents.
- [`build_output`](doc:operation-build-output-build-output) — Read the latest explicit native build output tail without waiting for adapter/plugin/document locks.
- [`build_cancel`](doc:operation-build-cancel-build-cancel) — Request cancellation of an owned CMake/MSBuild/Unreal process tree.
- [`build_configure`](doc:operation-build-configure-build-configure) — Configure a trusted CMake project manually or through a configurePreset from build_presets.
- [`build_run`](doc:operation-build-run-build-run) — Build an already configured trusted CMake project, optionally with native buildPreset targets/jobs/environment.

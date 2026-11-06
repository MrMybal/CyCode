---
id: api-unreal
title: Unreal Engine
description: Public operations for Unreal Engine.
section: API and extensions
parent: api-links
order: 100
revision: 2
---

Public operations for Unreal Engine.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`unreal_automation_plan`](doc:operation-unreal-automation-plan-unreal-automation-plan) — Preview one Unreal Engine 5 cook/package action without executing tools or creating output files.
- [`unreal_automation_run`](doc:operation-unreal-automation-run-unreal-automation-run) — Execute a matching issued cook/package preview once, after revision/tool/input/output checks and saved/reconciled workspace documents.
- [`unreal_prepare`](doc:operation-unreal-prepare-unreal-prepare) — Prepare and cache Unreal code analysis, including UHT-generated headers; reuses a matching existing cache.
- [`unreal_inspect`](doc:operation-unreal-inspect-unreal-inspect) — Inspect an Unreal .uproject, engine, targets, modules and plugins without executing project rules.
- [`unreal_artifacts`](doc:operation-unreal-artifacts-unreal-artifacts) — Inspect ordinary UE4/UE5 desktop Editor outputs in Development/DebugGame without executing tools or loading binaries.
- [`unreal_launch_profile`](doc:operation-unreal-launch-profile-unreal-launch-profile) — Prepare a shared debug profile patch for an installed UE4/UE5 editor, standalone game (-game) or uncooked dedicated server (-server).
- [`unreal_generate`](doc:operation-unreal-generate-unreal-generate) — Generate Unreal solution/project files using the selected engine UnrealBuildTool.
- [`unreal_database`](doc:operation-unreal-database-unreal-database) — Generate compile_commands.json for clangd via UnrealBuildTool.
- [`unreal_build`](doc:operation-unreal-build-unreal-build) — Compile a discovered Unreal target with UnrealBuildTool after saving or reconciling workspace documents; does not cook or package assets.

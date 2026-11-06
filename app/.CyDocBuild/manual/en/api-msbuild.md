---
id: api-msbuild
title: Solutions and MSBuild
description: Public operations for Solutions and MSBuild.
section: API and extensions
parent: api-links
order: 110
revision: 2
---

Public operations for Solutions and MSBuild.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`msbuild_inspect`](doc:operation-msbuild-inspect-msbuild-inspect) — Inspect a .sln/.slnx or direct MSBuild project, solution-to-project configurations and saved artifacts/index.
- [`msbuild_tools`](doc:operation-msbuild-tools-msbuild-tools) — Detect Visual Studio MSBuild and installed .NET SDKs without loading project rules.
- [`msbuild_build`](doc:operation-msbuild-build-msbuild-build) — Build the selected trusted .sln/.slnx/project with real MSBuild; solution project selection honors nesting/configuration mappings.
- [`msbuild_rebuild`](doc:operation-msbuild-rebuild-msbuild-rebuild) — Rebuild the selected trusted solution/project, including its clean target.
- [`msbuild_clean`](doc:operation-msbuild-clean-msbuild-clean) — Run the Clean target of the selected trusted solution/project.
- [`msbuild_restore`](doc:operation-msbuild-restore-msbuild-restore) — Run package restore for the trusted solution/project through MSBuild and configured package sources.
- [`msbuild_evaluate`](doc:operation-msbuild-evaluate-msbuild-evaluate) — Explicitly evaluate one trusted workspace project with MSBuild 17.8+ and read real target properties/items.
- [`msbuild_prepare`](doc:operation-msbuild-prepare-msbuild-prepare) — Explicitly evaluate up to eight enabled local VC++ projects and prepare a managed clangd compilation database from evaluated includes/defines/forced includes/language modes.

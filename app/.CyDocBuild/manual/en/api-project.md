---
id: api-project
title: Project tasks
description: Public operations for Project tasks.
section: API and extensions
parent: api-links
order: 109
revision: 2
---

Public operations for Project tasks.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`project_inspect`](doc:operation-project-inspect-project-inspect) — Inspect a Cargo, npm, Java, Maven, Gradle or shader project and saved profile without executing project code, resolving dependencies or invoking compilers.
- [`project_status`](doc:operation-project-status-project-status) — Inspect the currently owned Cargo/npm/Java/Maven/Gradle/shader task without waiting for execution, plugin or document locks.
- [`project_output`](doc:operation-project-output-project-output) — Read the latest project task live display tail without waiting for execution locks.
- [`project_configure`](doc:operation-project-configure-project-configure) — Select and save a project task profile.
- [`project_build`](doc:operation-project-build-project-build) — Build Cargo, npm, standalone Java, Maven package (skip tests), Gradle build, or one selected HLSL/GLSL shader with installed DXC/glslang.
- [`project_test`](doc:operation-project-test-project-test) — Run Cargo tests, npm test, Maven test or Gradle test with real project framework configuration, bounded output/deadline and process-tree cancellation.
- [`project_install`](doc:operation-project-install-project-install) — Explicitly install this npm project dependencies using npm ci when a package-lock exists, otherwise npm install.
- [`project_script`](doc:operation-project-script-project-script) — Execute one existing npm script or one simple Maven goal/Gradle task with bounded output/deadline.
- [`project_launch`](doc:operation-project-launch-project-launch) — Build/run a Cargo or Java main target, including Maven/Gradle resolved runtime classpaths, or start an npm script in a persistent PTY.
- [`project_debug_target`](doc:operation-project-debug-target-project-debug-target) — Build the selected Cargo executable or Java/Maven/Gradle main class.
- [`project_cancel`](doc:operation-project-cancel-project-cancel) — Cancel the active native project compilation/test/script process tree.

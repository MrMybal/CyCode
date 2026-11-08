---
id: api-workspace
title: Workspace
description: Public operations for Workspace.
section: API and extensions
parent: api-links
order: 116
revision: 2
---

Public operations for Workspace.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`workspace_describe`](doc:operation-workspace-describe-workspace-describe) — Describe the live workspace.
- [`workspace_counterparts`](doc:operation-workspace-counterparts-workspace-counterparts) — Find C/C++ header/source companion files for an already-open saved workspace document.
- [`workspace_include`](doc:operation-workspace-include-workspace-include) — Find workspace files matching the literal C/C++ #include on the caret logical directive of an already-open saved workspace document, including unsaved buffer changes.
- [`workspace_files`](doc:operation-workspace-files-workspace-files) — Read a cached workspace path-index page, respecting ignore rules.
- [`workspace_poll`](doc:operation-workspace-poll-workspace-poll) — Reconcile disk changes — reload clean open buffers, retain dirty/missing buffers and report conflicts.

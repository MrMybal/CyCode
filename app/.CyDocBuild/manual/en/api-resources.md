---
id: api-resources
title: Workspace resources
description: Public operations for Workspace resources.
section: API and extensions
parent: api-links
order: 108
revision: 2
---

Public operations for Workspace resources.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`resources_preview`](doc:operation-resources-preview-resources-preview) — Prepare a bounded filesystem action without mutating project files.
- [`resources_apply`](doc:operation-resources-apply-resources-apply) — Apply an issued filesystem plan after revision/content guards.
- [`resources_undo`](doc:operation-resources-undo-resources-undo) — Undo an applied in-process filesystem action only while its content still matches and affected buffers are saved.
- [`resources_trash`](doc:operation-resources-trash-resources-trash) — List bounded persistent project-trash entries; restoring needs preview/apply and workspace.write.
- [`resources_children`](doc:operation-resources-children-resources-children) — List one physical workspace directory, including empty folders and hidden/ignored project files.
- [`resources_events`](doc:operation-resources-events-resources-events) — Read bounded filesystem-operation events after a cursor.

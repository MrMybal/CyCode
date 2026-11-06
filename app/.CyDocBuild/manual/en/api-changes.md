---
id: api-changes
title: Change transactions
description: Public operations for Change transactions.
section: API and extensions
parent: api-links
order: 115
revision: 2
---

Public operations for Change transactions.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`changes_preview`](doc:operation-changes-preview-changes-preview) — Read before/after text and concrete UTF-16 edits for one file in a prepared change.
- [`changes_apply`](doc:operation-changes-apply-changes-apply) — Apply a prepared change to live workspace buffers only.
- [`changes_undo`](doc:operation-changes-undo-changes-undo) — Undo a previously applied refactoring across its live buffers.

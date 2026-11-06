---
id: api-recovery
title: Document recovery
description: Public operations for Document recovery.
section: API and extensions
parent: api-links
order: 118
revision: 2
---

Public operations for Document recovery.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`recovery_status`](doc:operation-recovery-status-recovery-status) — Inspect per-instance checkpoint health and pending changes.
- [`recovery_list`](doc:operation-recovery-list-recovery-list) — List bounded crash-recovery checkpoints.
- [`recovery_preview`](doc:operation-recovery-preview-recovery-preview) — Read a bounded UTF-8 checkpoint preview and target revision/disk guards, without changing documents.
- [`recovery_restore`](doc:operation-recovery-restore-recovery-restore) — Restore into a live buffer, never directly into the project file.
- [`recovery_discard`](doc:operation-recovery-discard-recovery-discard) — Permanently discard the selected inactive checkpoint after reviewing its reference.
- [`recovery_flush`](doc:operation-recovery-flush-recovery-flush) — Persist current dirty buffers to recovery checkpoints now, with no project-file save.

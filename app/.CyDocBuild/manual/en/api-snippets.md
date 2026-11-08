---
id: api-snippets
title: Insertion snippets
description: Public operations for Insertion snippets.
section: API and extensions
parent: api-links
order: 121
revision: 2
---

Public operations for Insertion snippets.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`snippets_complete`](doc:operation-snippets-complete-snippets-complete) — Read saved project, host-configured personal and active ABI2 plugin snippet catalogues for an already-open saved document.
- [`snippets_user_read`](doc:operation-snippets-user-read-snippets-user-read) — Read the fixed host-configured personal snippet catalogue shared by all workspaces of this installation.
- [`snippets_user_draft`](doc:operation-snippets-user-draft-snippets-user-draft) — Capture one nonempty ordered UTF-16 selection from an already-open saved workspace document at expectedRevision into a personal snippet JSON draft.
- [`snippets_user_save`](doc:operation-snippets-user-save-snippets-user-save) — Validate and explicitly save one personal JSON catalogue to the fixed application-owned file, shared across workspaces.

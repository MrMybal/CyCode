---
id: api-templates
title: Project templates
description: Public operations for Project templates.
section: API and extensions
parent: api-links
order: 107
revision: 2
---

Public operations for Project templates.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`templates_list`](doc:operation-templates-list-templates-list) — List built-in and active plugin project templates, typed option fields and supported .NET target frameworks.
- [`templates_preview`](doc:operation-templates-preview-templates-preview) — Preview exact project files without filesystem writes.
- [`templates_create`](doc:operation-templates-create-templates-create) — Create exactly an issued project preview in a new folder under this workspace.

---
id: api-plugins
title: Native extensions
description: Public operations for Native extensions.
section: API and extensions
parent: api-links
order: 120
revision: 2
---

Public operations for Native extensions.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`plugins_list`](doc:operation-plugins-list-plugins-list) — List loaded native plugins, commands and revision-control providers.
- [`plugins_invoke`](doc:operation-plugins-invoke-plugins-invoke) — Execute a registered command or panel callback from a trusted native plugin.
- [`plugins_configure`](doc:operation-plugins-configure-plugins-configure) — Enable or disable an installed plugin for this workspace.
- [`plugins_project`](doc:operation-plugins-project-plugins-project) — Invoke an advertised project-provider action through the shared native plugin host.

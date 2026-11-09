---
id: mcp
title: Connect an MCP client
description: Connect to the running IDE with a read or editing identity.
section: Work in CyCode
order: 260
parent: plugins
revision: 5
---

## Connect {#connection}

Windows packages built from this revision include the real CyToolsCore companion and its isolated Python/MCP runtime. No system Python or startup download is required. Open the MCP panel and copy the connection configuration for this running IDE instance into your MCP client.

Released versions through 0.2.3 and Linux/macOS source builds still need the separate development runtime. Building the desktop alone does not prepare the redistributable environment; use the documented packaging command.

The default configuration is read-only. Select editing access deliberately for document writes, version-control changes, process execution or plugin calls. Each application instance has its own connection context. Do not share private endpoint/identity files.

## Jobs and documents {#operations}

The official MCP adapter exposes `op_<operationId>`, `cy_get_job`, `cy_events` and `cy_cancel_job`. A submitted job is not its completed result: wait for Completed, Failed or Cancelled. For process results, also inspect `exitCode`, `timedOut` and `truncated`.

Reads and writes use the same native documents as the editor. Read a fresh revision before replacing text; save explicitly afterward. Workspace source documents are persistent references, not disposable task artifacts. The [public operation reference](doc:api-links#contract) documents parameters and results.

## Headless Core {#headless}

The [AI integration guide](doc:core-agents#connect) documents standalone Core startup, all 237 operation contracts, permissions, jobs, analysis preparation, navigation and guarded refactorings.

---
id: mcp
title: Connect an MCP client
description: Connect to the running IDE with a read or editing identity.
section: Work in CyCode
order: 260
parent: plugins
revision: 2
---

## Connect {#connection}

CyCode uses the real CyToolsCore companion. A compatible private SDK/runtime must be installed locally; it is not included in the public application archive. Open the MCP panel and copy the connection configuration for this running IDE instance into your MCP client.

The default configuration is read-only. Select editing access deliberately for document writes, version-control changes, process execution or plugin calls. Each application instance has its own connection context. Do not share private endpoint/identity files.

## Jobs and documents {#operations}

The official MCP adapter exposes `op_<operationId>`, `cy_get_job`, `cy_events` and `cy_cancel_job`. A submitted job is not its completed result: wait for Completed, Failed or Cancelled. For process results, also inspect `exitCode`, `timedOut` and `truncated`.

Reads and writes use the same native documents as the editor. Read a fresh revision before replacing text; save explicitly afterward. Workspace source documents are persistent references, not disposable task artifacts. The [public operation reference](doc:api-links#contract) documents parameters and results.

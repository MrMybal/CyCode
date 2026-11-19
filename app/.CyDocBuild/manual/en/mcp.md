---
id: mcp
title: Connect an MCP client
description: Connect to the running IDE with a read or editing identity.
section: Work in CyCode
order: 260
parent: plugins
revision: 8
---



## Connect {#connection}

Core 0.1.5 uses native CyMCPSDK 0.7.0. Use Manager to configure a project-bound,
read-only connection, or launch the native CLI with `--sdk --workspace <project>
--connection-state <private-root> --disable-plugins mcp`. No Python MCP server or
embedded chat is required. Attaching through `--native-descriptor FILE` reuses
an already-open Engine; `--bridge-config FILE --access read|edit mcp` connects to
its private shared HTTP service. Do not publish endpoint tokens.

## Operations and results {#operations}

Five default MCP tools discover and call all 257 canonical operations. Search
with `cy_tools_search`, load one contract with `cy_tools_describe`, then call
`cy_tools_call` with `{"name":"documents_read","arguments":{"path":"Source/main.cpp"}}`.
Large results provide private resources with complete chunk reads. Default access
is read-only; host permissions and installed tools govern edits and other actions.
Read actual document revisions, preview/apply native transactions and save explicitly.

Mutations and explicit jobs keep durable results across restarts. Interrupted
actions are never automatically replayed. Legacy `code`/`full` profiles retain
`op_<id>`, `cy_get_job`, `cy_cancel_job` and their job envelopes.

## Headless Core {#headless}

See [native automation](native-automation#connect), [agent workflows](core-agents#connect)
and [the public contracts](api-links#contract) for exact schemas, permissions,
project index, semantic prerequisites, persistence and independent tuning options.

---
id: native-automation
title: Native CLI and MCP
description: CyMCPSDK, indexed search, durable jobs and private resources.
section: API and extensions
parent: core-agents
order: 264
revision: 3
---

## Native connection {#connect}

Current sources use **CyMCPSDK 0.7.2**, revision
`e562c731a146d599e3404718ea8414bb5b96b66d`. Core, Platform, Http and MCP are compiled;
AI, Local and ChatUI are off. Core remains headless, with no chat or Python MCP server.
Standalone distribution 0.1.6 includes all four access profiles and defaults to Free.

```text
cycode_cli --sdk --workspace <project> --connection-state <private-root> --disable-plugins mcp
cycode_cli --sdk --workspace <project> --data-dir <state> describe-operation search_text
cycode_cli --sdk --workspace <project> --data-dir <state> invoke search_text --params-file query.json
```

`query.json`: `{"query":"answer","engine":"index","waitMs":20000}`. The operation
uses Core's persistent project index. It does not run SDK ripgrep over the project.
`--connection-state` isolates each connection's jobs and shares its project index;
explicit `CYCODE_INDEX_DIR` takes precedence. Native commands and Python launcher
commands remain compatible. New Manager configurations launch the native CLI directly.

## Discovery and permissions {#operations}

Default compact discovery exposes five SDK tools: `cy_tools_search`,
`cy_tools_describe`, `cy_tools_call`, `cy_resources_read`, `cy_resources_grep`.
Retain discovered schemas and call one real operation with:

```json
{"name":"documents_read","arguments":{"path":"Source/main.cpp"}}
```

`direct-full` exposes canonical names. Legacy `code`/`full` retain `op_<id>` and
job controls. Free is the default; controlled profiles use host permissions for
edits, VCS, processes, plugins and settings. Read current revisions, preview and apply
native transactions, then save only when requested. Attaching with
`--native-descriptor FILE` uses the existing Engine's unsaved state and Undo/Redo.
`serve --service-descriptor FILE` publishes private read/edit HTTP endpoints;
`--bridge-config FILE --access read|edit mcp` connects to them. Protected and Token only require credentials; Free needs only the URL.

## Local access profiles {#access-profiles}

Current sources select **Free** when no access choice exists. HTTP clients connect to the local `/mcp` address alone and can call every registered operation without another confirmation. Arguments, revisions, workspace paths and configured import/export roots remain validated.

| Profile | HTTP token | Operation policy |
|---|---|---|
| Free (default) | None | All registered operations |
| Local controlled | None | Host permissions |
| Token only | Required | All registered operations |
| Protected | Required | Host permissions |

Choose `--local-access-profile free|local-controlled|token-only|protected` or `CYCODE_MCP_ACCESS_PROFILE`. Servers bind to `127.0.0.1`. Existing explicit profiles and legacy read/edit permissions are preserved; unset configurations migrate to Free. The selected server profile is saved in its private `local-access.json` and restored at restart. Explicit CLI/environment settings take precedence. Token requirements apply to HTTP; stdio already uses the host local identity. Setup and the portable ZIP for 0.1.6 include this update; 0.1.5 introduced the initial native integration.

## Results and optimizations {#results}

Mutations have durable jobs. `core_job_submit/get/list/cancel` can be discovered
through the gateway. Queued/running jobs interrupted by a restart are marked
failed, never replayed. Legacy SQLite jobs, credentials and artifact paths are
preserved through the stdlib-only migration helper; installed packages run the
conversion once when needed. Live old owners must stop before conversion.

Large results use private resource handles and complete chunk reads. Expiration
never replays an action; retrieve its durable job or explicitly repeat a read.
The independent CLI options control compact discovery, inline threshold, memory/
disk quotas, disk spooling, result TTL, cache TTL, optional notifications and
metadata-only JSONL telemetry. SDK resource grep searches registered snapshots,
not workspace directories. Native event metadata invalidates the optional
`cycode://workspace/changes` resource; older MCP clients use TTL/refresh.

The SDK schema projection omits pattern/dependency/contains conditionals; Core
still enforces the complete canonical contract. Read `cycode://contracts/<id>` or
`core_contract` for it. Native error details remain in the JSON-encoded MCP error
message and durable job. See [the full contracts](api-links#contract).

## Build and distribution {#build}

Build from a clean pinned SDK source with
`python app/scripts/build.py --core-only --no-install --cymcp-source <source>`.
`python app/scripts/prepare_manager_runtime.py` prepares only pinned CPython
stdlib for Manager/compatibility, without SDK wheels or site-packages. Core/MCP
itself requires no Python. Native SDK/transitive MIT notices ship in the package.
Windows x64 is the validated platform; Linux/macOS execution remains pending.

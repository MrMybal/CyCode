# Native automation with CyMCPSDK

This source revision replaces the Python CyToolsCore server with **CyMCPSDK 0.7.0**,
revision `791d4d7d7680213b282ec8529d248a10f8494d77` (see `app/cymcp-source.lock.json`).
Standalone distribution 0.1.5 includes this migration. Its package version is independent of the historical engine contract version 0.2.3.
Core remains headless. There is no chat, Conversation, AI provider or ChatUI.

## Build and ownership

```text
python app/scripts/fetch_dependencies.py --core-only
python app/scripts/build.py --core-only --no-install --cymcp-source <clean-CyMCPSDK-checkout>
```

The build reads the exact clean SDK source pin and compiles Core, Platform, Http
and MCP only. AI, Local, ChatUI, ChatApp, examples and SDK/native tests are disabled.
`prepare_cymcp.py` prefers the supplied/sibling local checkout; Git is a fallback
only when local sources are absent. It never resets a local checkout.
Embedders can keep `CYCODE_BUILD_AUTOMATION=OFF`: `CyCode::Core` has no SDK or UI
dependency. Opt into `CyCode::Automation` to use the same registry as the CLI/MCP.
One Engine owns documents, index, services, validation, transactions and Undo/Redo.
The adapter routes all 257 canonical contracts to that Engine. Every registered business operation is discoverable through the five-tool gateway; project scope, permissions and installed backends still apply. It adds no Python
editor model and does not substitute SDK ripgrep for Core indexed search.

## CLI and stdio MCP {#connect}

```text
cycode_cli --sdk --workspace <project> --data-dir <private-state> list-operations
cycode_cli --sdk --data-dir <private-state> describe-operation index_symbols
cycode_cli --sdk --workspace <project> --data-dir <private-state> invoke search_text --params-file query.json
cycode_cli --sdk --workspace <project> --connection-state <private-root> --disable-plugins mcp
```

A tested `query.json` is `{"query":"answer","engine":"index","waitMs":20000}`.
`invoke` returns domain JSON; failures produce JSON on stderr and exit 1. Help
uses `--help`. `--params` and positional JSON remain available; use a params file
for large inputs or shell-independent quoting. No GUI or Python MCP runtime is
needed. The native `WORKSPACE METHOD [JSON]`, `WORKSPACE --stdio`, `WORKSPACE
--serve DESCRIPTOR`, diagnostic options and `--version` remain unchanged.

New client configurations launch `cycode_cli --sdk` directly. `--connection-state`
allocates a private SDK/job directory per connection while sharing the installation-
owned persistent index under `<private-root>/index`. It sets no global PATH.
The index warms on the first real operation (disable with `CYCODE_INDEX_AT_START=0`).
Explicit `CYCODE_INDEX_DIR` retains precedence. A warm/lazy semantic backend is
not proof of exhaustive compiler coverage or the availability of clangd.

Default **compact** advertises exactly five official SDK tools:
`cy_tools_search`, `cy_tools_describe`, `cy_tools_call`, `cy_resources_read`,
`cy_resources_grep`. Discover one operation and retain its schema for the connection:

```json
{"name":"search_text","arguments":{"query":"answer","engine":"index","waitMs":20000}}
```

That object is the argument to `cy_tools_call`. `direct-full` exposes canonical
names directly. Legacy `code`/`full` profiles retain `op_<id>`, the nested
`op_core_invoke` envelope, `cy_get_job`, `cy_cancel_job`, tasks and discovery.
`app/tool.py` forwards existing Python commands/options to the native host;
its `invoke` retains the historical final-job envelope. `agent_host.py` remains
an old-configuration shim. New clients do not load either Python entry.
`--backend native`/empty and empty `--model` remain accepted; there is no inference model.

## Permissions and existing instances

Default access is read-only. Select explicit `--permissions` such as
`workspace.write,process.execute`; VCS, plugin and settings writes retain their
own canonical permissions. `--access edit` explicitly selects document/VCS/process/
plugin rights. Managed read/edit profiles preserve the narrower existing grants
when refreshed. `--owner` is for a trusted local operator, never generated for agents.
Host roots and credentials cannot be changed through operation JSON.

Use `--native-descriptor <private-native.json>` to act on an Engine already owned
by an IDE/Reader; unsaved text, native plans and Undo history remain in that owner.
No second Engine is started in this mode. Use `serve --service-descriptor FILE`
to publish separate read/edit loopback HTTP endpoints. Their private descriptor
retains `url`, `readToken`, `editToken` and adds `readUrl`, `editUrl`, `mcpReadUrl`,
`mcpEditUrl`. `--bridge-config FILE --access read|edit mcp` connects to that actual
service; `--connect URL` uses `CYMCP_TOKEN` (`CYTOOLS_TOKEN` is a legacy alias).
The old `/v1` job protocol is kept. Tokens bind a trusted endpoint identity;
caller-supplied principal fields never grant rights. SDK origins/bearer checks
remain enabled. The SDK has one token per HTTP endpoint, so roles use separate ports.
A shared role credential shares job/resource ownership: issue separate services/
credentials when independent actors need isolation. Never publish descriptors.

## Durable jobs and compatibility

CyMCPSDK supplies the registry, policy, executor, MCP, resources, HTTP and metrics;
it has no persistent scheduler/database compatible with the former Python SDK.
The Core adapter therefore owns its durable jobs and identities. Queues have
priority, RAM admission, exclusive groups, queue/execution deadlines and bounded
disk admission. A native cancellable action retains its worker until the actual
Engine/process acknowledges cancellation. A committed/noncancellable transaction
is never interrupted by discarding its reservation.

Mutations receive durable identities before execution. Terminal outputs and full
structured native errors are saved under `<state>/jobs/*.json`; ownership and
sessions are retained in `identities.json`. `core_job_submit/get/list/cancel`
expose those jobs through compact discovery. Transient resource TTL never deletes
durable results. A restarted queued/running job becomes Interrupted/Failed and
is **never replayed**. A disconnected or cancelled mutation must be reconciled
with its job/native change receipt before another action is considered.
Persistence failure reports `ResultPersistenceFailed` with the job ID and actual
output; invalid native output retains its actual result in error details.

Existing `state.sqlite3` records are converted once by the shipped **stdlib-only**
`app/scripts/migrate_legacy_jobs.py --state-dir <state>`. The installed native CLI
runs that conversion when its bundled Manager Python/helper is available; source
builds can run it explicitly. It refuses a live old runtime lock. Old databases,
credentials, artifacts and output paths remain intact; it never runs old jobs.
A missing import raises MigrationRequired instead of silently hiding old jobs.
Import/export controls require explicit `files.import`/`files.export` permissions
and trusted `--import-root`/`--export-root`. Copies/results are retained under quota,
not substituted for native workspace files; export includes preserved legacy job
artifacts with adjusted output paths. No arbitrary filesystem root is exposed by MCP.

## Independent optimizations and actual limits

- Discovery: `--mcp-profile compact|direct-full|code|full`.
- Inline threshold: `--mcp-inline-bytes` (16 KiB default); larger JSON results use
  private SDK handles with complete UTF-8-safe 32 KiB chunk reads. `cy_resources_grep`
  is literal grep of registered snapshots/contracts, not a workspace scan.
- Retention: `--mcp-result-ttl-seconds` (300); memory quota
  `--mcp-result-memory-bytes` (64 MiB); optional `--mcp-spool DIR` and
  `--mcp-result-disk-bytes` (1 GiB). Spooling defaults off; threshold is 1 MiB.
  Expiration never replays a call. Recover durable jobs or explicitly issue a new read.
- Cache: `--mcp-cache-ttl-ms` (30000); zero disables TTL reuse. SDK discovery/resource
  metadata supports TTL and explicit refresh; tool mutations are never cached/replayed.
- Optional `--mcp-notifications`: native event metadata updates
  `cycode://workspace/changes` and the SDK change feed. Subscription delivery depends
  on negotiated MCP/transport support; older clients must use TTL/refresh.
- Optional `--mcp-telemetry FILE`: private JSONL duration/status/byte-count/cache
  metadata only. No source, result text, token, request/client/session IDs or secrets.
- Jobs: `--job-disk-bytes` (1 GiB), `--job-ram-bytes` (512 MiB reservation budget).
  These are admission budgets, not a bound on total Engine/process memory.

The SDK JSON Schema profile does not support `pattern`, `dependentRequired` or
`contains`. The host retains and enforces the complete canonical contracts,
including conditions/alternatives; SDK discovery carries their structural projection.
Fetch `cycode://contracts/<id>` or `core_contract` for the exact contract. Native
errors preserve details in the JSON-encoded MCP error message; durable jobs keep
those fields directly. Resources never expose private host state directories.
Native transport budgets and the existing paged document/search cursors still apply.
Windows x64 is the validated platform for this migration; Linux/macOS await execution.

## Packaging and validation

```text
python app/scripts/prepare_manager_runtime.py
python app/scripts/build_manager.py --cyui-source <pinned-CyUI>
python app/scripts/package_core.py --revision <clean-commit> --build <production-build> --executable <accepted-cli> --manager-executable <manager-build>/Release/CyCodeManager.exe --cyui-source <pinned-CyUI> --manager-bundle <verified-bundle> --tools-root <verified-tools> --documentation app/Documentation --output <new-output-directory>
python app/scripts/verify_sources.py
python app/scripts/generate_mcp_reference.py --check
python app/scripts/verify_native_automation.py --executable <production-cli> --fixture <new-isolated-directory>
```

The verification command is explicitly opt-in, uses an external dev-only `mcp==1.30.0`
client and starts production Core processes; it compiles no test executable.
Manager Python is pinned CPython standard library only: no CyToolsCore, Python
MCP server, third-party wheels or site-packages in new packages. Native SDK MIT
and transitive notices ship with them. clangd remains a reviewed optional private
installation; the native SDK HTTPS helper verifies pinned size/SHA-256 and resumes
complete bounded ranges. No GitHub Actions, publication or antivirus changes are
part of this migration.

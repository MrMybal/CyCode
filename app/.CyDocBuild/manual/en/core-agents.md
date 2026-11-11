---
id: core-agents
title: Use Core from AI agents
description: Persistent Core, complete MCP contracts and guarded code changes.
section: API and extensions
parent: mcp
order: 261
revision: 5
---

## Connect {#connect}

Install the standalone distribution and agent skill through [Core installation](core-installation#distribution).

CyCode Core exposes the same persistent native Engine to applications and AI
clients. There are 252 domain operations, each exposed as `op_<operationId>` by
the official CyToolsCore MCP adapter. The SDK adds its own job, discovery, task
and identity controls. A listed operation is a contract, not proof that a
language server, debugger, compiler, VCS provider or plugin is installed.

From a source checkout with the private SDK runtime already prepared:

```sh
python -B -X utf8 app/tool.py --core-workspace <project-directory> --core-executable <trusted-cycode_cli> --disable-native-plugins --data-dir <private-job-directory> mcp
```

Use the isolated interpreter prepared by `app/scripts/setup_runtime.py`; the
Windows desktop distribution uses its bundled interpreter. This command starts
one production native Core without a GUI, retains it across requests, and drains
it when MCP closes. It never downloads a binary. `--core-executable` is optional
when a built CLI is discoverable beside the application or in its runtime build
directory. `CYCODE_CORE_EXECUTABLE` is another explicit host setting. The project
root is fixed by the trusted host, never by an AI operation parameter.

Standalone MCP is read-only by default. The host can set `CYCODE_PERMISSIONS` to
an explicit comma-separated list such as `workspace.write,process.execute`.
VCS writes, native plugin execution and user settings have separate permissions;
read each operation's `permissions` before enabling them. Disabling native
plugins is recommended when the agent only needs built-in code services.

For an existing IDE, copy its MCP configuration instead: `--bridge-config
<private-service-descriptor> --access read mcp` uses its actual shared SDK
service. Choose `edit` only when authorised. Headless hosts may start that same
official shared service with `--core-service --service-descriptor <private-file>
--data-dir <private-job-directory>` together with the two Core startup options.
Keep that service process alive and stop its owned native host to drain it.
Do not publish endpoint descriptors or read/edit identity tokens. Two standalone
MCP runtimes do not share jobs merely because they use the same project.

### Client configuration {#configuration}

For clients supporting a `mcpServers` stdio configuration, replace the absolute
path placeholders in this read-only example. Other clients may use a different
outer configuration format; keep the same executable and argv.

```json
{
  "mcpServers": {
    "cycode": {
      "command": "<private-python>",
      "args": [
        "-B",
        "-X",
        "utf8",
        "<CyCode>/app/tool.py",
        "--core-workspace",
        "<project-directory>",
        "--core-executable",
        "<trusted-cycode_cli>",
        "--disable-native-plugins",
        "--data-dir",
        "<private-job-directory>",
        "mcp"
      ],
      "env": {
        "CYCODE_PERMISSIONS": ""
      }
    }
  }
}
```

For several agents sharing one native document context and one SDK scheduler,
start the service once from the trusted host:

```sh
python -B -X utf8 app/tool.py --core-workspace <project-directory> --core-executable <trusted-cycode_cli> --disable-native-plugins --core-service --service-descriptor <private-sdk-descriptor> --data-dir <private-job-directory>
```

Each client then runs the private interpreter with `app/tool.py --bridge-config
<private-sdk-descriptor> --access read mcp`. An authorised editing client chooses
`--access edit`. The SDK owns both identities; neither receives the other's
private jobs. Stop the owning service with Ctrl+C for a normal shutdown.

## Discover operations {#methods}

Read `cy_describe`, `cy_list_operations`, `cy_describe_operation` and `cy_can_run`
before assuming availability or permissions. The generated `app/MCPMethods.json`
contains every MCP name, native method, full input/output schema, permission,
exclusive group and progress/cancellation flag from the executable contract.
Regenerate it with `python app/scripts/generate_mcp_reference.py`; verify without
writing with `--check`. The bilingual HTML API reference contains the same 252
operation contracts, grouped by domain. It is not a listing of internal C++
functions and does not expose trusted native shutdown/cancellation controls.

| Need | Operations to discover |
| --- | --- |
| Context and preparation | `service_status`, `analysis_status`, `analysis_prepare`, `language_status` |
| Files and text | `workspace_files`, `workspace_refresh`, `workspace_include`, `search_text`, `documents_*`, `sources_*` |
| Code intelligence | `language_definition`, `language_declaration`, `language_type_definition`, `language_references`, `language_symbols`, `language_document_symbols`, `language_complete`, `language_hover`, hierarchy and diagnostic operations |
| Changes | `language_rename`, formatting/code-action operations, `changes_preview`, `changes_apply`, `changes_undo` |
| Builds and execution | `toolchains_*`, `build_*`, `msbuild_*`, `unreal_*`, `project_*`, `tests_*`, `terminal_*`, `debug_*` |
| Extensions and VCS | `plugins_*`, `packages_*`, `vcs_providers`, `vcs_context`, `vcs_provider_action`, typed Git operations |
| Recovery and events | `recovery_*`, `events_read`, `snippets_*` |

These are discovery groups, not wildcard MCP calls. Use exact names and schemas
from the catalogue, including prerequisites and confirmation fields.

## Run jobs {#jobs}

An MCP domain call wraps its native parameters in `parameters`. Submission returns
a `jobId`. Poll `cy_get_job({"jobId":"<issued-job-id>"})` until Completed, Failed
or Cancelled; read the domain result from `outputs` only after Completed. Read
`error.code` and `error.technicalDetails` after Failed. For process operations,
also inspect `exitCode`, `timedOut`, `truncated` and any operation-specific result.

```json
{"tool":"op_analysis_prepare","arguments":{"parameters":{"paths":["Source/Game.cpp","Source/Game.h"]}}}
```

The paths above illustrate a request shape: select existing project paths first.
Never invent a revision, document identity, backend epoch, preview identifier,
continuation cursor, confirmation identifier or job identifier.

`analysis_prepare` and C++ rename report semantic-preparation progress through
the SDK job. `analysis_status` is an aggregate native snapshot; it does not grant
ownership of another client's job. Use `cy_cancel_job` with your own issued job
ID. Cancellation retains the native reservation until the real operation stops.
Slow AST attempts can retry while indexing continues; cancellation and genuine
backend/configuration/revision failures remain distinguishable. No separate
Python scheduler or editor model exists.

The SDK manages temporary imported/generated task files. Workspace source paths
are persistent native references; do not import project files with
`cy_import_file`, pass them as disposable task inputs or delete them on
`cy_finish_task`. Export wanted temporary artifacts before ending their task.
Closing an MCP task does not save or discard shared native editor buffers.

## Prepare and navigate {#analysis}

Start with `service_status`, then inspect `language_status.servers`. Service
`ready`, server initialization and sent document revisions do not prove that
the whole project is semantically indexed. `analysis_prepare` waits for actual
compiler document-symbol/AST responses using native live buffers. Explicit paths
have no fixed file-count ceiling; native transport and per-file byte bounds still
apply. Only one analysis-owned temporary document is retained at a time. Existing
and dirty editor documents remain open. C++ explicit preparation reopens the LSP
draft to obtain fresh diagnostics without changing native document identity,
revision, text or UI ownership. Ordinary navigation keeps its existing cache.

For a large Unreal root, inventory the **active** compiler database first:

```json
{"scope":"compilation-database","batchSize":0}
```

This returns command/unique-translation-unit counts, a session and continuation,
without analysing sources. Optional `pathPrefix` selects an existing workspace
directory. Process batches (64 by default for compiler-database scope):

```json
{"continuation":"<issued-session>","batchSize":64}
```

Repeat while `state` is `pending`. `completed`, `total` and `coverage.remaining`
describe the selected scope. `diagnosticClean`, `diagnosticErrors` and
`diagnosticUnknown` distinguish version-bound compiler publications from missing
evidence. A finished traversal is **not** an exhaustive Unreal semantic
certificate: inactive configurations, assets, macro-created names and compiler
limitations remain outside that claim. `wholeProjectIndexCompleteness` stays
`unknown`. Sources containing compiler errors remain explicit observations.

The result includes only its first 128 metadata rows. Use `analysis_results` with
the issued `session`, `offset` and a `limit` up to 256 to read the remaining rows;
follow `nextOffset` while `hasMore`. Pages contain metadata, never source text.
They are historical observations, not a guarantee that the session is still
current. Native document/workspace changes, backend epochs or compiler-database
changes reject a continuation with `RevisionConflict`; start a fresh preparation.
Owner cancellation retains the last completed rows; a subsequent explicit
`analysis_prepare` can resume that session. The checkpoint lives only in this
Core process and can be replaced by another preparation. Global rename currently
restarts after cancellation rather than resuming a partial edit collection.

Preparation does not enable a server, compile a project, save source text or
prove a complete workspace index. Missing real backends fail.

Enable an installed backend explicitly with `language_configure` and its trusted
executable argv. For Unreal, use real UBT compiler commands and generated headers.
`unreal_prepare` is the project adapter; engine-source-only roots without a
`.uproject` still require a separately exported UBT compilation database supplied
to clangd. Do not treat lexical colouring or a file-list cache as semantic data.

`workspace_files` is paginated: retain both `generation` and `indexRevision`,
follow `nextOffset`, and restart if either changes. `complete`, `indexing`,
`indexError` and `truncated` describe that file snapshot, not clangd completeness.
Navigation uses zero-based UTF-16 `line`/`character` coordinates and a fresh
`expectedRevision` from `documents_read`; preserve document IDs/epochs when
supplied by the operation. `sources_*` handles supported read-only SDK targets.

Definition, declaration and type-definition can reuse a bounded native response
cache. Source revisions, the native document context, backend epoch, observed
workspace changes, compiler-database context and target disk stamps guard reuse.
`refresh:true` bypasses it; `cacheHit` reports actual reuse. References and
implementations are not cached because their results can grow during indexing.
The cache does not establish whole-project index completeness.


The built-in C++ launch enables clangd draft headers by default (clangd 14+;
locally verified with 18.1.8); an explicit `--use-dirty-headers=false` remains an
opt-out. For definition/declaration into an already opened C++ target, Core can
reconcile a stale index destination against the target's current compiler AST
using exact USR identity. `astReconciled:true` reports an adjusted result. An
ambiguous compiler match is counted as invalid rather than guessed. This also
handles restoring an edited header to its original text. No independent parser
or text-based symbol substitution is used.

## Refactor safely {#refactor}

1. Read the live document and its exact revision. Submit `language_rename` at
   the compiler-resolved symbol with the desired new name.
2. Inspect every returned file, `coverage`, annotation and `changes_preview`.
   C++ traverses every source-spelled workspace candidate with rolling buffers.
   Compiler USR identity confirms the selected symbol in each relevant file;
   only that file's compiler-issued rename edits are collected. Closing an
   unrelated candidate cannot make its references disappear from the edit plan.
   A compiler-confirmed local parameter avoids the global scan.
3. There is no 128-candidate cutoff and no 100-file transaction cutoff. The
   built-in clangd launch supplies `--rename-file-limit=0` unless explicitly
   overridden. Incomplete/ambiguous compiler identity, missing or errorful current
   diagnostics, unsupported edits and provider failures refuse the whole preview.
   Native snapshots now spool to private disk; no aggregate 16 MiB transaction
   cutoff remains. Per-operation capacity failures never issue a partial plan. Macro token-pasting, serialized Blueprint/reflection names,
   inactive platform/configuration branches, external projects and unreported
   semantic references remain limitations.
4. Apply the issued plan with `changes_apply`, passing every required
   `confirmedAnnotations` ID. Revisions, disk baselines and the prepared global
   scope are rechecked before the first buffer mutation. A changed scope rejects
   the entire plan. Inspect all results before taking another action.
5. `changes_undo` restores the whole transaction if its buffers and disk
   baselines still match. Neither apply nor undo saves source files. Use
   `documents_save` deliberately, with the current revision, when authorised.

The real Unreal acceptance case `FDateTime::ParseHttpDate` covers its implementation,
header declaration and HTTP caller on the first rename request, without manually
opening the latter two files. This case does not prove exhaustive semantics for
all 37,557 Unreal translation units.

## Errors and capability limits {#errors}

`RevisionConflict` requires a fresh read/status and a new plan; do not blindly
replay a stale mutation. `Cancelled` is a terminal owner cancellation.
`DeadlineExceeded` is an individual timed provider attempt; semantic preparation
can continue with progress. `BackendUnavailable` requires repairing/configuring
the backend. `CapacityExceeded` requires a smaller supported scope or a future
capacity change, never treating partial data as complete. Native codes/details
are preserved through the SDK. Invalid output is a bridge contract bug, not a
successful native operation to ignore.

Check advertised capabilities and installed toolchains for LSP/DAP, Unreal,
language builds, native plugins and non-Git providers. Do not infer that every
feature listed by an extension descriptor is executable. Current changes are
validated locally on Windows with the production Core/clangd and real Unreal;
Linux/macOS execution, new installers and GUI acceptance require separate checks.
GitHub Actions remain paused by the owner.

[Paged native text and large transaction workflow](doc:large-projects#agents).


## Shared native history {#history}

Use `changes_history_status` and `changes_history_prepare`, then review/apply the
issued replay for guarded multi-step Undo/Redo on the same Engine. Require all
affected live identities/revisions and the current history head/revision.
`changes_apply` / `changes_undo` return the exact committed document metadata in
`commit`; post-commit notification failure is distinct from a rejected edit.
Reconcile a lost RPC response with `changes_status` before retrying. This is
process-local, bounded native transaction history, not another text owner or
durable recovery. See [the complete contract](doc:large-projects#history).

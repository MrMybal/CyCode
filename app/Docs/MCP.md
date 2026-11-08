# Integrated MCP

## Bundled Windows runtime and shared service

Windows distributions built from this revision contain CPython 3.13.17,
CyToolsCore 0.10.1 and the official MCP Python SDK 1.30.0 under tools/python.
The fixed _pth file isolates imports to the bundled standard library, installed
packages and this application's app directory. Startup and copied MCP commands
use -B -X utf8: no managed bytecode cache, ambient Python or first-use download.
Native editing remains independent of the companion. Source/development and
Linux/macOS builds retain the existing private-runtime fallback.

The desktop starts one real SDK service for its own native bridge. Read/edit
MCP connections use distinct identities on that shared scheduler. A standalone
MCP process has a separate Runtime; it does not automatically share jobs. Native
documents/revisions and resource/process owners remain common to GUI/CLI/SDK.

The runtime lock pins all 32 wheels, published SDK commit and 53 package hashes,
the official Python archive and the exact installed payload. Wheel .data records
are installed at their proper locations; licenses remain beside distributions.
SDK upstream manuals and unused dependency GUI/service launchers are excluded.
See Build.md for generation and THIRD_PARTY_NOTICES.md for attribution.

The new Windows runner check uses the actual installed interpreter/CLI and two
official MCP clients against one shared Core. It must verify all 233 operation
names, Unicode live editing, read-only denial, private-job isolation, stale
revision refusal, bundled plugin loading, unchanged disk bytes and normal
native/Core shutdown. Preparation/hash checks do not establish these runtime
results; see Finalization.md for their status. Local native runs remain suspended.

Public binaries through 0.2.3 do not yet contain this environment. Source builds
can still use setup_runtime.py with a real SDK wheel. Automatic Linux/macOS
runtime packaging is not implemented by the Windows authoring script.

## Development milestone record

The notes below retain earlier contracts and their historical operation counts.
The current generated API reference/manifest has 233 operations; the capability
ledger and individual feature guides supersede old pending-status statements.


workspace_include / workspace.include follows bounded literal logical C/C++
directives with inline comments and immediate backslash-newline continuations.
Requested caret and actual physical inner-header UTF-16 range are separate.
workspace_files now requires a positive cache generation in every page; clients
must compare it together with indexRevision before combining pages, including
after manual rebuilds. The native API remains 233 operations with unchanged ABI2
dispatch. See [Includes](CppIncludes.md) and [Workspace](Workspace.md).

workspace_counterparts / workspace.counterparts adds guarded local C/C++
header/source discovery (233 public operations). It returns bounded metadata only;
no process, recursive scan, source edit/save or UI opening occurs through SDK/ABI2.
See [C++ companions](CppCounterparts.md).

snippets_user_draft / snippets.userDraft provides read-only selected-code capture.
It returns the original
personal snapshot plus a literal JSON draft, never a source edit or implicit save.
Persist separately with snippets_user_save and settings.write; see
[Selection capture](SnippetCapture.md).

snippets_user_read / snippets.userRead and snippets_user_save / snippets.userSave
provide personal persistence. The trusted embedding configures one
fixed personal store; no path argument, project-selected storage or temporary task
artifact is used. Save is noncancellable with settings.write and the user-settings
group, not workspace.write. Revisions are opaque lowercase-hex content guards or
absent; native preflight, lease and atomic write preserve competing writers and
report unconfirmed postflight. Read-only plugin callbacks cannot save. See
[Personal snippets](UserSnippets.md).

snippets_complete merges project, personal and active ABI2 plugin catalogues
without separate provider operations. Items retain cycodeSnippetSource;
result.cycodePluginRevision binds the captured registry. Native issuance and
preparation recheck registry changes in addition to document/catalogue/disk state.
No client-supplied plugin path or callback is used. Already returned concrete plans
retain their usual guarded apply semantics; query/preparation stays read-only.

snippets_complete / snippets.complete adds saved workspace insertion catalogues
without a language-server request (228 operations). An already-open saved workspace
document, expectedRevision and UTF-16 position yield an issued completion session.
Optional selection supplies ordered UTF-16 start/end coordinates with the caret
at either endpoint and at most 8 KiB of source text. The issued byte range owns
TM_SELECTED_TEXT; unavailable bodies carry cycodeSnippetAvailable/cycodeSnippetReason metadata.
Preparation requires actual variable use and exact replacement bounds, allowing
multiline local wrapping with literal layout. Coordinates reject fractional,
negative, out-of-document and surrogate-splitting values. SDK count stays 228.
language_completion_prepare accepts the index/session, rechecks catalogue/source/
workspace/disk guards and returns the existing plan/byte-field metadata. Applying
still requires workspace.write; querying/preparing is read-only. Regex/rendering
work releases the main Engine mutex and revalidates before storing a plan. No
Python editor model, process or catalogue-file write is introduced. See
[Project snippets](SnippetCatalogue.md).

language_document_highlights / language.documentHighlights adds the shared
permission-free in-document read (227 operations). Current document identity,
revision and backend epoch are checked before and after the provider reply. Output
contains actual Text/Read/Write kinds, sorted UTF-16 ranges, query position and
invalid/truncated counts. Bounds are 4096 rows, 8192 records and 4 MiB. Disabled or
unsupported providers are rejected; text-only highlights do not imply exhaustive
semantic references. No cursor/source edits are exposed by this read. See
[Document occurrences](Occurrences.md) for inputs, defaults and limits.

language_declaration, language_type_definition and language_implementation join
language_definition/references in the shared native location pipeline. All five
accept optional expectedDocumentId/expectedEpoch and return source identity,
backend/epoch, method/position, validated fullRange/optional originRange and
invalid/outside/truncated counts. Schemas bind each operation's returned method.
Source lifetime is precise; other-file targets remain index observations. The
surface has 226 operations; the three additions are permission-free reads without
execution/mutation groups. See [Source navigation](SourceNavigation.md).

language_symbols preserves complete symbol rows and now returns issued session/
resolveIndex references for scoped URI-only entries. language_symbol_resolve uses
the original native server payload, two-minute/four-session bounded cache and
workspace/lifecycle/monitor/epoch guards before and after resolving. Complete
positions remain index observations. document_symbols retains precise document
identity/range checks. language_status exposes symbolResolve capabilities.
The added read operation has no permissions or mutation groups; 223 operations
share the native owner and ABI2, with no independent Python model. See
[Symbol navigation](SymbolNavigation.md).

build_location resolves one supplied compiler display line to an existing
workspace source file. It uses the shared native parser without execution,
source-content reads, permissions or mutation groups. The bridge does not inject
a build operation ID or cancellation target. language_status diagnosticRevisions
now pairs required native id/revision with the publication URI; raw diagnostics
remain advisory until consumers verify that pair. The current manifest has 222
operations. See [Compiler locations](CompilerLocations.md) and
[Diagnostics](DiagnosticNavigation.md).

project_status/project_output read the existing project task owner without waiting
for execution/plugin/document locks or saved buffers. Status is permission-free;
output requires process.execute and has no mutation group. Context, conditional
replacement snapshots and Cargo rendered-message byte accounting are explicit.
Read-job cancellation never targets the compiler. The manifest has 222 operations;
see [Project live output](ProjectTaskOutput.md).

build_output reads the latest explicit CMake/MSBuild/Unreal live tail while a
build owns the mutation group. It requires process.execute but is read-only and
has no exclusive group. Conditional streamId/revision reads return replacement
snapshots, never append-only chunks; byte counts and bounded display repair are
explicit. The manifest has 222 operations. This uses normal SDK polling, not MCP
push notifications; see [Live build output](LiveBuildOutput.md).

unreal_artifacts / native unreal.artifacts add a read-only assessment of ordinary
desktop Editor receipts, module IDs and recorded project binaries. The operation
has no permissions/mutation group and uses a 64 MiB SDK memory reservation. The
manifest now contains 222 operations. Native Unreal build results optionally add
artifacts and inspectionError while retaining actual process outcome. Bound
run/launch and desktop prebuild continuation require scoped ready outputs; profile
configuration and SDK launch's explicit-build behavior remain unchanged. See
[Unreal outputs](UnrealArtifacts.md) for bounds and incomplete/unsupported results.

Prepared Unreal launch settings now include a required non-null unrealLaunch
profile/request binding. Common debug profile/configure/run/launch schemas accept
it, null detachment or absence for legacy profiles. The native owner revalidates
bound program/cwd/argv/runtime before write or actual launch; native Unreal build
requires saved/reconciled workspace buffers. The desktop prebuild uses the bound
context and guarded captured parameters, independent of toolbar changes. SDK
launch still does not compile automatically. The current manifest has 222 operations and
permissions/ABI stay unchanged; see [Unreal launch](UnrealLaunch.md).

unreal_launch_profile prepares typed UE4/UE5 editor/game/uncooked-server settings
through the native Unreal inspector without compiling, saving or executing tools.
It requires process.execute like debug_profile because expectedProfile can contain
the saved environment. Send settings + expectedProfile to debug_configure for a
guarded save, then use existing debug_run/debug_launch; SDK launch does not build
automatically. The manifest now has 222 operations. See
[Unreal launch profiles](UnrealLaunch.md) for flags, bounds and pending acceptance.

unreal_automation_plan/run add reviewed UE5 cooking/packaging with explicit
maps/container/deadline controls and issued input/tool/output guards. Run requires
process.execute and workspace.write, uses the existing mutation group and actual
build.status/cancel owner, and retains persistent workspace intent/logs/results.
The bridge matches cancellation by operation ID and waits for the reviewed
deadline plus postflight allowance. The manifest has 222 operations; Runtime/
actual UE acceptance remains pending. See [Unreal automation](UnrealAutomation.md).

vcs_replay_state/plan/apply add reviewed single-commit cherry-pick/revert, resolved
continuation and quit tracking with retained work/index. Full source IDs and
repository/state/plan guards exclude ranges, arbitrary options and external VCS
providers. Apply persists an intent, retains actual process/inspection outcomes,
requires VCS/workspace/process permissions and shares the existing mutation
group. Native ownership is noncancellable; the bridge waits up to 1,200 seconds
for repeated inspection/mutation/postflight. Actual Runtime acceptance remains
pending. See [Git replay](GitReplay.md).

vcs_merge_state/plan/commit/quit add actual merge blockers/staged review, guarded
unsigned completion and quit tracking with retained working/index state. Typed
requests bind repository/state/plan revisions and message/hooks policy; intent
receipts are persistent workspace files. Actual process status and postflight
inspectionError remain explicit. Mutations require VCS/workspace/process
permissions, share the existing group and keep noncancellable native ownership.
SDK Runtime acceptance remains pending.
See [Merge workflow](GitMergeWorkflow.md).

vcs_checkout_plan/apply/undo expose reviewed single-file synchronization after
staging, with persistent original/displaced backups and separate guarded Undo.
Paths are repository-relative; binary bytes remain native. Apply rechecks the
issued plan and complete index; Undo protects newer file content. Both retain
workspace/VCS/process permissions and report partial/inspection outcomes. All
three use noncancellable native ownership.
Actual SDK Runtime acceptance remains pending. See [Git working files](GitWorkingFiles.md).

`vcs_conflict_list/inspect/plan/apply` add typed Git structural conflict services.
The new paths are repository-relative; documentPath remains workspace-relative.
Preview/apply require observed repository/catalogue guards, explicit per-path
choices and an additional preview revision on apply. Nullable text never carries
binary bytes. Apply changes the index only, requires VCS/workspace/process
permissions and retains noncancellable native ownership. Actual exit status plus
catalogue or catalogError remains explicit. Four typed conflict operations are included;
SDK Runtime acceptance remains pending. See [Git conflicts](GitConflicts.md).

`vcs_worktree_plan` previews `move` or `repair` with required observed repository/
catalogue guards and path/destination. Apply through `vcs_worktree_move` or
`vcs_worktree_repair` with the same inputs and issued expectedPlanRevision. Plans
expose checked paths and explicit file/index/transport effects; apply returns real
process status plus catalogue or an unconfirmed catalogError. Preview is read-only;
apply requires VCS/workspace/process permissions. Both retain noncancellable native
ownership and existing 600-second bridge waits. Repair audits the full linked set
because Git can update other linking files. Session selection is desktop-owned;
SDK callers continue to select repositories explicitly. Runtime acceptance remains
pending; see [Git worktrees](GitWorktrees.md).

`vcs_submodule_settings_plan` previews one URL/tracking-branch metadata edit,
including previous value redaction, existing overrides, exact argv and effects.
`vcs_submodule_settings` requires its preview revision plus the same observed
repository/catalogue/input guards. Both share native handling of staged/unstaged
metadata, document/directory guards and actual postflight/index/checkout checks.
Preview is read-only; application requires VCS/workspace/process permissions.
Neither requests transport/staging/checkout; URL sync follows Git config rules.
Literal, empty remote-default and dot parent-name tracking branches are supported.
Actual Runtime/Git/GUI acceptance remains pending; see [Git submodules](GitSubmodules.md).

`vcs_submodule_deinit_plan` inspects a clean, absorbed, parent-owned module and
returns removal/preservation effects plus a separate preview revision. The guarded
`vcs_submodule_deinit` removes only that checkout/local registration, preserving the
gitlink, .gitmodules and Git metadata. It refuses ignored/untracked files, hidden
flags, nested gitlinks and open native documents. Read preview has no mutation
permission; the action requires VCS/workspace/process permissions and retains
noncancellable native ownership. Actual removal/retention is checked after Git,
including partial outcomes with exit zero. Runtime acceptance remains pending.

`vcs_submodule_add_plan` previews a new submodule locally. The separate
`vcs_submodule_add` clones/stages only after the observed catalogue, repository
and preview revisions match. Both share native guards/process ownership and
typed actual/unknown metadata; read inspection has no mutation permission,
while addition requires VCS/workspace/process permissions. See
[Git submodules](GitSubmodules.md) for remaining runtime acceptance.

`vcs_submodules` reads configured top-level modules for an explicit Git repository.
The guarded `vcs_submodule_init/sync/update` operations share native process ownership,
typed actual state and the desktop's reviewed actions. Mutations require VCS,
workspace and process permissions; no SDK task artifact staging or independent
Python Git model is involved. Real Git/Runtime acceptance remains pending; see
[Git submodules](GitSubmodules.md).

`debug_instruction_breakpoints` replaces the full session-only list with explicit
session/stop/revision guards. Disabled entries remain local, enabled entries carry
actual adapter verification, and ambiguous failures leave synchronization false.
It requires process.execute and is noncancellable after submission, sharing the
desktop/plugin DAP owner. No process addresses are persisted into launch.json.
See [Instruction breakpoints](InstructionBreakpoints.md).

`debug_step_back`, `debug_reverse_continue` and `debug_restart_frame` require
explicit observed session/stop plus selected thread/frame references. Existing
forward step/Continue operations retain optional queued context and gain typed,
capability-gated granularity/singleThread options. All seven handlers are
noncancellable after submission and expose actual lastExecution acknowledgement/
uncertainty, rather than inventing a completed target step. They require only
process.execute and share the native GUI/plugin owner. See
[Execution controls](DebugExecution.md).

`debug_set_variable`, `debug_set_expression` and `debug_write_memory` require
process.execute and explicit observed session/stop generations. Expression edits
also capture the selected frame; memory edits require expected bytes checked by a
fresh read, with bounded explicit replacement bytes and subsequent read-back.
The common native owner reports typed lastMutation outcomes, including partial or
unconfirmed effects. These three handlers are noncancellable after submission and
do not edit source or participate in document Undo. See
[Value and memory editing](DebugValueEditing.md).

`debug_data_breakpoint_info` inspects a paused target; `debug_data_breakpoints`
replaces the entire session list. Both require process.execute and observed
session/stop generations. Replacement also needs dataBreakpointRevision and
current-pause issued handles, with explicit verification/uncertain state. Its
noncancellable SDK handler retains the native mutation outcome. See
[Data breakpoints](DataBreakpoints.md).

`debug_hot_reload` uses the current `debug_status.sessionGeneration` and an optional
unique operationId to build the imported JDT workspace and apply Java changes on
the same native session. Both this operation and `debug_cancel_reload` require
process.execute. Cancellation matches a pending call; the bridge retains the
SDK reservation until its native result, and never rolls back an already sent
redefine request. javaReload status reports actual returned class names, unchanged
or failure. The cancellation operation has no workspace exclusive group so it can
signal the running job. See [Java debugging](JavaDebugging.md) for requirements,
VM limits and pending runtime acceptance.

Prepared text edits expose passive annotations and issued per-plan confirmation
IDs. Inspect their labels/descriptions/files, then pass required IDs through
changes_apply.confirmedAnnotations. The native Engine rejects missing, duplicate,
unknown or foreign-plan IDs before mutation. Plain-plan calls remain compatible;
permissions, scheduling and operation IDs are unchanged. See
[annotated changes](ChangeAnnotations.md).

Visual Studio solution/project build, clean/rebuild/restore, artifact evaluation and
C++ index preparation use the native msbuild_* operations. Inspecting configurations
is read-only; evaluation and builds require process.execute. See [MSBuild](MSBuild.md).

Git branches, remotes, sync, stashes and conflict operations are documented in
[Version control](VersionControl.md), with separate vcs/workspace/process permissions.

Interactive terminal/run/debug operations are documented in [Execution](Execution.md).
They all require process.execute, including status/screen/profile reads. Persisting
debug profiles and breakpoints also requires workspace.write. These native sessions
survive individual Core jobs/tasks; explicit terminal_close/debug_stop or IDE/workspace
shutdown ends them. Temporary task cleanup never deletes their original project files.
The same debug_* contracts support native and .NET targets. runtime accepts
automatic/native/dotnet, dotnet selects an explicit host or automatic discovery,
and adapterArgs supplies optional bounded argv elements. For attach without a
managed program path, select runtime=dotnet explicitly. Managed DAP output is in
debug_status.output; debug_run provides a real terminal for interactive stdin.

The desktop starts its private CyToolsCore 0.10.1 companion if runtime/python exists.
Open the MCP panel and copy/save its connection configuration. It references this
running IDE instance. Configuration contains no token: tool.py reads the private
Core descriptor and uses the existing SDK HTTP/MCP adapters.

Read access is the default configuration. The panel's editing switch selects the
identity granted workspace.write, vcs.write, process.execute and plugins.execute.
Repository build scripts and native plugin commands execute code; these are deliberate
operation capabilities. Git honors repository hooks and signing configuration.

The default desktop configuration uses one read identity and one edit identity per
GUI instance. Reusing the same configuration shares that identity's job visibility;
different roles remain isolated. Dedicated authenticated clients can be provisioned
through the SDK rather than assuming one credential means several independent users.

Operations are described in CyTool.json, generated by contracts.py. The official MCP
adapter exposes op_<operationId>, plus cy_get_job, cy_events, cy_cancel_job and the
SDK's session controls. A submitted job is not a completed result. Poll until Completed,
Failed or Cancelled. A native failure becomes a failed job; transport success is not
business success. Progress currently uses polling rather than MCP notifications.

Native exceptions produce failed jobs. A completed process operation can still
contain a nonzero exitCode, including compilation errors and merge conflicts;
inspect exitCode/timedOut/truncated before treating that result as successful.

Document writes require expectedRevision from a fresh read. Replacements change the
live buffer; documents_save explicitly persists it. Save rejects external disk changes.
The native service treats project documents as persistent workspace references.
SDK temporary task cleanup removes job artifacts, never original source documents.

toolchains_detect and build_inspect are read operations; they describe installed
tools and the saved CMake profile/targets without configuring the project. Tool
discovery queries installed tool versions. build_configure and build_run require
process.execute. They accept source, buildDirectory, configuration, toolchain
(a detected ID or automatic), generator (a CMake name or automatic), and target
(empty for default targets). Omitted fields use the active local profile. Configure
prepares shared clangd settings and saves the profile only on success. Build
validates target names and rejects configuration mismatches for single-config trees.
The process result includes exitCode/output/timedOut/truncated and project metadata
on success; a nonzero process exit has project=null. Always inspect exitCode even
when the Core job completed. See [Build services](Build.md) for paths and limitations.

Unreal operations are unreal_inspect, unreal_generate, unreal_database and
unreal_build (prefixed op_ over MCP). Inspect is read-only; the other operations
require process.execute. They accept project, engineRoot, target, platform and
configuration, returning native Unreal project metadata. Inspect exitCode on every
process result. The 180-second deadline and build cancellation limits also apply
to these jobs. See [Unreal services](Unreal.md).

C/C++ and C# semantic operations use the same native servers and live overlays as the UI.
C# activation through language_configure requires process.execute because loading
projects evaluates MSBuild imports. Read-only document/status calls do not activate it.
language_status returns per-server connection state and merged published diagnostics;
language_definition and language_references accept path, expectedRevision and
position {line, character}. Both coordinates are zero-based; character counts UTF-16
code units. Read the live document first, and convert positions from that text.
References also accepts includeDeclaration (default true); both operations accept
limit (default 1000, maximum 5000). Results include source revision, positionEncoding,
workspace-relative locations with LSP ranges, truncated and outsideWorkspace.
Outside-workspace locations are counted but omitted by default. Set includeExternal
to true for registered read-only SDK locations; these carry an opaque sdk:// path,
external=true and displayPath. sources_read accepts the issued source reference and
returns a readOnly document with revision/text/displayPath. Arbitrary external paths
and SDK writes remain refused. A changed live revision during
a query fails the job; read again and retry. Semantic operations are available to
the read identity and never save documents. language_complete accepts path,
expectedRevision and a UTF-16 position and returns standard LSP items. Applying snippets,
additional imports and completion resolve is pending. See [C# language services](CSharp.md).

language_highlight accepts path and expectedRevision and returns supported,
generation, truncated and tokens. Tokens contain line/character/length in UTF-16,
glyphStart/glyphEnd in Unicode codepoints, the negotiated type name and modifier
names. A response is bounded to 50,000 tokens or a 4 MiB token payload. The native
adapter validates ranges, overlaps and surrogate boundaries; obsolete revisions
fail the job. Generation changes on live-document updates, restarts or server
refreshes. supported=false requests lexical fallback. C# analysis must first be
enabled; this read operation cannot activate it. Opened SDK references retain their
read-only scope. Delta/range requests and modifier styling are not exposed yet.

language_symbols accepts query (up to 256 bytes), language (cpp by default or csharp), typesOnly, includeExternal and
limit (up to 1000). language_document_symbols accepts path and expectedRevision,
including previously opened SDK references. Results contain names, LSP kinds,
container/detail, locations, truncation and UTF-16 encoding metadata; document results
also carry the source path/revision.

language_rename accepts path, expectedRevision, position and newName; language_format
accepts path, expectedRevision, optional range, tabSize and insertSpaces. Both prepare
plans without editing. The result includes id/title/state/files/editCount; each file
records its expectedRevision and edit count. changes_preview accepts id/path and
returns before/after text and validated edits. changes_apply and changes_undo accept
id and require workspace.write. They prevalidate all documents before changing any
live buffer; they never save files. Fetch fresh document revisions after applying.
Concurrent edits/disk changes reject the transaction. See [Semantic editing](Semantic.md)
for bounds, undo rules and C++/Unreal limitations.

Example parameters for op_language_definition:

```json
{"parameters":{"path":"main.cpp","expectedRevision":7,"position":{"line":12,"character":8}}}
```

Semantic requests have a 15-second deadline including server initialization, and
expired requests send LSP cancellation. clangd's available index may still be
building; reference results are not a completeness guarantee. Accurate project
compilation flags or compile_commands.json remain necessary. See the
[LSP position specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position).

Cancellation is cooperative: a Core handler drains the native reply before releasing
its reservation. It does not promise immediate native build cancellation. Native
search has a 15-second deadline and build processes a three-minute deadline.
The SDK's local HTTP request body is capped at 1 MiB, which also constrains large
document replacement through the shared MCP route. The GUI file limit is 4 MiB.

For automated bridge tests, cycode_cli WORKSPACE --serve DESCRIPTOR exposes the same
native Engine without an SDL window. tool.py can also run standalone SDK CLI/MCP
commands with CYCODE_BRIDGE pointing at that descriptor. Standalone MCP connections
have independent runtimes; the desktop shared service is the multi-connection path.

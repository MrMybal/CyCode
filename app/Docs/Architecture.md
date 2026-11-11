# Native Core architecture

The progressive-capacity stage adds [paged native text and storage](LargeProjects.md):
streamed compiler databases, incremental checkpoint pages and disk-backed native
transactions. Paged documents now add sparse line/UTF-16 reads, exclusive Save as
and explicit durable checkpoint preview/restoration through Apply/Undo. The public
SDK catalogue has 249 operations. Graphical/LSP adapters and automatic per-keystroke
recovery remain separate; ordinary GUI/LSP buffers retain their compatibility policy.
No hardware-unlimited or exhaustive Unreal claim follows.


The native semantic-preparation owner also maintains opt-in traversal metadata
under `.cycode/analysis-progress`. OS leases and shared atomic publication protect
its writer; restoring rebinds the current pass and downgrades historical compiler
observations. Source text and SDK scheduling remain with their existing owners.
See [persistent analysis progress](AnalysisProgress.md).


The repository now provides the native Core and CLI. The graphical application
has moved to [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE). Read
[Core extraction and evidence](CoreExtraction.md) for the current dependency
graph, service contract, build commands and remaining migration stages.

The following architectural history is preserved for existing application links.

# Architecture

IncludeDirective reads bounded logical C/C++ directives while retaining physical
header offsets, inline-comment context and immediate escaped-newline components.
NavigationUI reuses the joined related-file future for an always-choose inspector,
cached action rows and explicit index-refresh/retry; ordinary queries stay scan-free.
Path/report copying and contextual CyDoc help retain source guards. workspace.files
pagination now exposes and compares both watcher revision and rebuild generation,
so a manual refresh cannot silently mix pages with the same watcher revision.
See [Includes](CppIncludes.md), [Related results](CppCounterparts.md#inspect-related-results)
and [Workspace](Workspace.md).

RelatedFiles combines bounded local metadata probes with same-stem offsets into
Engine’s current workspace path cache. The secondary lookup is rebuilt with that
cache, never starts a scan and uses nonblocking snapshots plus watcher-revision /
cache-generation revalidation. Missing/busy/stale/partial/changed coverage remains
explicit. Engine releases its mutex around metadata probes and rechecks source
lifetime/revision. NavigationUI owns a joined async query, cheap tab/Engine guards
and cached CyUI Commands rows; incomplete observations never auto-open.
GUI/SDK/ABI2 share workspace.counterparts (233 public operations), without another
process, LSP, recursive enumeration or source-content read. See
[C++ companions](CppCounterparts.md).

Selection capture copies a guarded live document range under Engine ownership,
then releases its mutex for personal JSON drafting. It rechecks source revision/
generation before returning. UserSnippets reuses PreparedSnippet.literal escaping,
adds one final caret/unique name and validates the complete bounded draft. No source
or catalogue write occurs; the GUI reuses the personal CodeEditor with a distinct
saved baseline and dirty generated text. SDK/ABI2 share snippets.userDraft; see
[Selection capture](SnippetCapture.md).

UserSnippets owns an optional fixed installation catalogue and its consistency
guard; only the trusted embedding supplies its directory. The GUI and installed
CLI opt in; default Engine and desktop test/smoke sessions do not. Fresh bounded
reads and exact BLAKE2b-256 revisions guard issuance/preparation. A cached local
generation lets UI palettes/acceptance observe known changes without per-frame
file reads. NativeFileLease and the shared atomic writer protect explicit saves.
The dedicated JSON modal reuses CyUI CodeEditor for draft Undo/Redo, with joined
I/O futures, confirmed discard/reload/quit and no independent source editor model.
GUI/SDK/ABI2 share read/save; settings.write controls personal persistence. The
SDK now has 233 operations; existing source edits still use workspace.write.
See [Personal snippets](UserSnippets.md).

ABI2 snippet catalogues use immutable declarative definitions, validated through
SnippetCatalogue's existing parser at registration and query. Plugins supplies a
bounded active snapshot under its own recursive lock; CompletionEngine acquires it
only after releasing Engine's mutex, avoiding reverse callback lock order. Atomic
plugin revision guards bind issuance/preparation and desktop acceptance. Workspace
items precede stable order/ID-sorted plugin items; provenance stays native and no
callback executes while querying/rendering. CompletionSession stores the registry
revision; concrete ChangePlans retain existing apply guards. SnippetsUI borrows the
issued item array instead of deep-copying bodies per frame. Plugin inventory builds
summary metadata directly; only snippet queries copy the bounded catalogue bodies.

Local snippet sessions optionally own an ordered byte selection, derived from
validated UTF-16 coordinates. CompletionSelection is a GUI-free constexpr policy
for endpoint, UTF-8 boundary and 8 KiB checks. Catalogue parsing and wrap preflight
use an immutable source snapshot outside the Engine lock; source/generation and
saved catalogue are revalidated before issuance. Only explicit local requests can
relax the same-line primary replacement rule. Both palette and acceptance guard
the selection anchor as well as caret/revision; native preparation requires actual
TM_SELECTED_TEXT use and exact issued replacement bounds. Wrapping skips automatic
reindentation to preserve literal selected layout. Existing plan/field/Undo owners
remain responsible for the transaction; no dependency or ABI change is needed.

SnippetCatalogue supplies bounded saved project definitions without a second
snippet renderer or language server. CompletionEngine issues local proposals in
the existing session cache, including their catalogue baseline. Local preparation
requires an already-open workspace document, bypasses LSP resolution and reuses the
ordinary completion edit/ChangePlan. Rendering/regex work now releases the Engine
mutex and repeats context/disk checks before storing the result. SnippetsUI uses
CyUI Commands and the existing completion-accept/EditorSnippet transaction owner.
GUI, SDK/MCP and ABI2 share snippets.complete; the SDK has 232 operations, with
unchanged ABI2 layout and existing CyUI modules/dependencies. See
[Project snippets](SnippetCatalogue.md).

CompletionVariables performs bounded application-context substitution before the
unchanged shared numeric Snippet parser. Completion.cpp owns context lookup;
CompletionEngine supplies the guarded native document/workspace and Application
uses the same helper for lazy suggestion previews. CompletionTransforms adds
bounded variable regex matching and simple Unicode case conversion through private
static PCRE2/utf8proc dependencies of cycode_core. Numeric field sessions remain
in the shared Reader library; transforms are computed once in native preparation.
The GUI only validates the format and displays the server label for transformed
items, without executing regex while drawing suggestions. No new document model,
CyUI module or SDK/ABI operation is added. See [Completion variables](CompletionVariables.md).

Shared tool discovery preserves compiler/interpreter aliases and checks POSIX
execute permissions without adding another tool-resolution owner. The build
wrapper provides bounded compiler context and retains complete failure logs.
The CMake-local search now delegates to the same native owner; compiler-cache
identity retains the driver name separately from resolved parent directories.
The editor dependency now pins the published Reader 0.1.3 source; CyUI remains a
single validated modular dependency. See [Toolchain reliability](ToolchainReliability.md).

UpdateHistory adds explicit bounded metadata-only installation/recovery history
within cycode_updates. Shared staging/outcome parsers, work/pending/receipt identity
agreement and numeric guards produce advisory rows without payload hashes or
filesystem mutation. UpdateHistoryUI owns one cancellable/joined future, cached
CyUI filtering, twelve-row pages and snapshot selection. The native report excludes
raw metadata/errors. No SDK/ABI, target, module or dependency is added. See
[Update history](UpdateHistory.md).

RecoveryProgress publishes single-attempt atomic phase/count/write observations
bound to the native review's plan/transaction. UpdateRecovery retains validation,
leases, copying, commit and postflight; internal typed cancellation preserves its
ordinary exception contract. UpdaterRecoveryUI starts and joins that service worker
from a thin system TaskDialog, keeps close requests cooperative and separates final
results from optional technical errors. Native receipts and explicitly copied reports
reuse the recovery owner and never authorize file changes. Only the Windows helper
adds Comctl32/Common-Controls v6; core dependencies, SDK 227 / ABI2 and CyUI modules
stay unchanged. See [Recovery controls](UpdateRecovery.md).

UpdateRecovery now owns bounded prepared-copy reuse through immutable work identity
and source maps, with the existing mutable status as a discovery hint only. Native
review revalidates source ownership/type/size/hash; execution recomputes reuse
headroom under its lease, stages missing originals, rechecks all selected sources
and commits from their verified paths. Mixed-directory maps survive further retries
without a second history owner. Original backups remain required; SDK/ABI, modules
and the core dependency boundary remain unchanged. See [Recovery resumption](UpdateRecovery.md).

UpdateRecovery now derives allocation-aware copy/retention headroom from its own
verified scan. Preview space fields are observations; execution rechecks under
exclusive ownership, stages all bounded copies, repeats the complete baseline and
checks commit headroom before moving targets. Plan-bound space receipts and honest
preparation/commit phases remain with the same recovery owner. EN/FR native helper
feedback adds required/available capacity without SDL/ImGui/SDK dependencies. See
[Recovery preparation](UpdateRecovery.md).

UpdatePackage now uses allocation-aware UpdateBudget installation estimates before
extraction and before replacement. UpdateInstall supplies stable failure categories
shared by helper/history; UpdateFiles removes pending markers through the verified
Windows file handle. Ordinary-failure rollback verifies the full original baseline.
These remain inside existing native/cache/recovery owners, with no SDK/ABI/module
change. See [Installation checks](UpdateInstallation.md).

UpdateBudget supplies bounded download/staging headroom arithmetic, with native
caller-available space probes in the existing worker. UpdateProgress defines
release-bound atomic observations and a main-thread-only sampled rate/ETA meter.
UpdateTransfer publishes preparation/receiving/hash/staging/readiness/failure phases;
UpdatesUI reads counters without per-frame disk/network probes. Installation still
requires full native verification. SDK/ABI/module/target ownership is unchanged.
See [Download observations](UpdateDownloads.md).

Update retry now passes an explicit native resume/fresh mode. UpdateDownload owns
bounded deterministic checkpoint selection; fresh mode bypasses inspection and
uses a new owned directory. DownloadError carries fixed failure categories and
UpdateFeedback supplies EN/FR display keys, independently of technical messages.
UpdatesUI schedules one quiet local inspection after interruption/failure, retaining
diagnostics and discarding replaced-release digests without network retry, restored
consent or modal reopening. The existing owners/dependencies/SDK remain unchanged.
See [Download retry](UpdateDownloads.md).

UpdateDownload owns exact asset/checkpoint binding and bounded partial discovery;
UpdateRange supplies the actual constexpr HTTP response policy. UpdateTransfer uses
native non-truncating file handles, per-copy download exclusion, validated ranges
and one fresh-folder fallback while retaining interrupted data. UpdateCache exposes
observed partial summaries separately from verified Prepared candidates; UpdatesUI
keeps explicit network/install consent. No SDK/dependency/module owner is added.
See [Download resumption](UpdateDownloads.md).

UpdateStorage adds explicit bounded cache inspection and receipt-backed reversible
payload moves to the existing installation owner. UpdateStorageUI owns guarded
asynchronous selection/acknowledgement and cancelled/partial results; no startup
maintenance runs. Shared cache leases exclude download/helper/recovery races, while
the existing file utility owns non-overwriting moves for recovery and storage.
Metadata/history and installation backups remain with their existing owners.
See [Update storage](UpdateStorage.md).

Application updates have an installation-scoped owner outside the workspace Engine
and MCP. UpdateRelease owns typed stable-version/asset/path policy; UpdateTransfer
owns bounded WinHTTP/SHA-256 staging; UpdateCache owns bounded read-only staging discovery, shared candidate verification
and helper outcome persistence through the existing atomic text writer. UpdatePackage owns manifest-checked replacement
and retained transaction backup. UpdateManifest shares strict before/after file-set
validation with UpdateRecovery. Immutable format-2 pending plans bind reviewed
preimages; recovery owns an exclusive lease, exact review guards, non-overwriting
copy/retain operations and final baseline postflight. The existing helper adds
standalone EN/FR OS recovery dialogs, preserving its minimal dependency boundary.
UpdatesUI owns asynchronous futures, atomic
progress/cancellation, startup proposals and explicit post-close handoff. The native
lease implementation is factored into one cycode_native_lease target shared with
the core. cycode_updates and the production CyCodeUpdater avoid IDE/SDL/ImGui/SDK
links. GUI/native-service leases exclude replacement until shutdown; the helper
waits without killing processes. Existing save/session/Undo owners remain. The
SDK stays at 227 operations. See [Application updates](Updates.md).

OccurrenceCatalogue validates/deduplicates document highlights in cycode_core,
using the existing source identity/revision/backend guards and SymbolTextBounds.
OccurrencesUI owns one context-bound future, debounced ready-server queries and a
cached dockable inspector. EditorOccurrences owns independent geometry and draws
unfilled outlines through existing CodeEditor coordinates; it never overwrites
native glyph annotations. EditorTextMap shares UTF-16/glyph conversion with
DiagnosticAnnotations. Explicit multi-selection preflights all ranges before one
SetSelections, preserving source/Undo ownership. GUI/SDK/ABI2 share the native read
(227 SDK operations); no module/dependency/C ABI/target is added. See
[Document occurrences](Occurrences.md).

NavigationLocations now normalizes bounded source Location/LocationLink responses
in the native core, with identity/revision/backend guards and live-source UTF-16
validation. It serves five navigation methods through GUI/SDK/ABI2 (226 SDK
operations). NavigationUI owns ticketed result presentation and cached LocationModel
filter/selection labels, reusing the existing document/history owner and References
dock. No dependency, CyUI module, C ABI, process or Undo owner is added. See
[Source navigation](SourceNavigation.md).

SymbolCatalogue normalizes hierarchical/flat server results, snapshot-local node
relationships, extents and deprecation in the native core. SymbolTextBounds indexes
the validated buffer once for UTF-16 range checks. Engine symbol queries guard
identity/revision and backend configuration epochs without invalidating document
structure for unrelated edits. OutlineModel caches source/alphabetic order and
CyUI-filter ancestor visibility. StructureUI and the existing palette share those
native results, guarded document/history navigation and ticketed async replies.
SymbolSession now retains bounded opaque workspace-symbol data for on-demand
location resolution, with expiry/eviction, reply cache and pre/post workspace/
lifecycle/monitor/backend guards. The palette separates query and selection tickets,
adds same-query refresh tickets and reuses the existing guarded source opener.
SDK language_symbol_resolve and ABI2 language.symbolResolve share that owner.
Existing Core/CodeEditor modules and ABI2 JSON services remain; no dependency,
process owner, C ABI or executable target is added. See
[Symbol navigation](SymbolNavigation.md).

LanguageService now pairs native document identity/revision with connection-wide
monotonic LSP versions, including replacement lifetimes and reload. Editor
annotations and ProblemModel navigation require current ownership; Problems
shares severity/text selection and CyUI's diagnostic list/detail card. The
CompilerLocation core parses bounded caller-supplied lines, resolving existing
workspace files through Engine's guard. GUI log activation, SDK build_location
and ABI2 share that service. No additional process/model owner, UI module,
dependency, C ABI or executable target. See [Diagnostics](DiagnosticNavigation.md)
and [Compiler locations](CompilerLocations.md).

Three related milestones share existing ownership. TaskOutput extracts the bounded
journal in BuildOutput for both BuildOperations and ProjectTasks, retaining separate
reservations/tokens. TaskOutputText frames Cargo display records while preserving
the final parser. Project status/output bypass execution locks and reuse SDK/ABI2.
BuildUI switches sources and protects captured cancellation; workspace switching
observes remote task ownership. ProblemModel owns filtered/cyclic location selection,
and ModuleUI/commands reuse guarded document opening and CyUI Diagnostics. No new
dependency, process owner, CyUI module, C ABI or executable target is added. See
[Project output](ProjectTaskOutput.md) and [Diagnostics](DiagnosticNavigation.md).

BuildOperations owns a bounded, revisioned live output tail on its existing
reservation. Internal ProcessOutputScope binds observation to a synchronous
thread/token; adapters opt in only for user-facing main execution. BuildOutput
repairs display text and rejects stale callbacks without affecting raw process
results or cancellation. BuildUI polls the same lock-independent Engine service
as SDK/ABI2 and uses CyUI Diagnostics. No new owner, module, dependency or C ABI;
see [Live build output](LiveBuildOutput.md).

UnrealArtifacts adds a bounded, read-only assessment of project Editor receipts,
module BuildIds and recorded binary presence. It reuses native Unreal discovery
and remains UI-free. Build results retain actual process data plus artifacts or
postflight uncertainty. The existing debugger's ProfileCheck distinguishes profile
configuration from actual process launch; only run/launch require fresh scoped
artifact readiness. Desktop continuation also requires matching ready outputs.
UnrealArtifactsUI reuses Core controls and virtualized file rows, and SDK/ABI2/
provider 1.5.0 use the same service. No new dependency, module or process owner;
see [Unreal outputs](UnrealArtifacts.md) for excluded dependency/ABI coverage.

Unreal launch settings now persist an optional profile/request binding in the
common debug profile. Debugger's internal ProfileCheck callback delegates bound
configure/run/launch validation to UnrealProject; the core has no UI dependency.
UnrealLaunchUI compiles the bound build context and captures a one-shot desktop
continuation with immutable parameters, draft/saved-profile/build-ID guards and
open document identities/revisions. Pure continuation/document helpers reject
stale or uncertain results before using existing terminal/DAP owners. Saved
edits during a build are covered; unrelated clean document opens remain allowed.
Legacy/null profiles and explicit detachment preserve manual editing. SDK/ABI2
use the same profile model and validator; no new API operation, module, dependency,
process owner or ABI. See [Unreal launch](UnrealLaunch.md).

UnrealLaunch prepares typed UE4/UE5 editor/game/uncooked-server profiles through
the existing Unreal inspector. Map validation is shared with automation. Engine
adds the observed debug profile for compare-and-swap; Debugger validates that
snapshot on its existing job owner and retains atomic disk-baseline saving.
UnrealLaunchUI guards its draft, uses CyUI Core and saves through debug.configure;
actual Run/Debug retain the existing terminal/DAP owners. No extra process owner,
profile store, module, dependency or ABI. SDK/ABI2 and provider 1.5.0 use the same
implementation. See [Unreal launch profiles](UnrealLaunch.md).

UnrealAutomationModel validates typed cook/package/map/container controls and
fixed argv. UnrealProject owns bounded issued previews, descriptor/rule/map/tool
guards, exclusive output intent, actual process/archive inspection and persistent
logs/results. Managed .NET tool discovery is shared with UBT. BuildOperations
owns cancellation/status/events until work stops; Engine guards live documents.
UnrealAutomationUI discards stale previews and reuses the build owner, CodeEditor
logs and CyUI Core controls. SDK/ABI2 and bundled Unreal provider 1.5.0 use that
same owner. No new dependency, target, CyUI module or ABI; acceptance remains
pending. See [Unreal automation](UnrealAutomation.md).

GitReplayModel normalizes single-commit actions and derives start/continue/quit
blockers from the common GitMerge inspector. GitReplay reuses its complete bounded
index/metadata/document observations, verifies actual source objects, guards
workspace metadata, persists exclusive intents and checks actual process/raw
parent/tree/index outcomes. GitReplayUI binds history choices and stale-context
guards to the same SDK/ABI2 service. No new dependency, target, CyUI module or ABI;
acceptance remains pending. See [Git replay](GitReplay.md).

GitMergeModel parses bounded parent/path/tree records and normalizes commit/quit
requests. GitMerge owns operation metadata, full index/flag/intent state, document
guards, reviewed effects, intent receipts and actual process/postflight outcomes
through the VCS mutex. GitMergeUI preserves drafts, discards stale previews and
shares Diff/conflict navigation with the same SDK/ABI2 service. Commit verifies
actual parents/tree/index; quit retains working/index state and is separate from
abort/reset. No dependency, executable target, CyUI module or ABI change. Runtime
acceptance remains pending; see [Merge workflow](GitMergeWorkflow.md).

GitCheckout adds stage-zero/absent-index working-file plans and receipts to the
native VCS owner. Resources owns bounded private binary payloads, file/permission
guards, persistent trash/install journals and restoration, reusing exclusive
moves and new exclusive binary creation. Engine document locking protects commit
and closed source/trash roots. Generic resource calls cannot consume private Git
plans; SDK/UI/ABI2 share native preview/index guards. Common events refresh image
views. No dependency, executable target, CyUI module or ABI change; runtime/POSIX
acceptance remains pending. See [Git working files](GitWorkingFiles.md).

GitConflictModel parses bounded complete NUL index records and validates explicit
portable path choices into fixed Git argv. GitConflicts owns selected-repository,
HEAD/branch/index guards, blob inspection, deterministic previews and actual
postflight through the existing VCS mutex. GitConflictsUI uses the same service as
the typed SDK and four ABI2 exports, rejects stale async contexts, and preserves
the existing three-way text editor. Resolution changes the index only; working
files and document ownership/Undo remain untouched. No new target, dependency,
CyUI module or ABI change is introduced. Native runtime acceptance is pending;
see [Git conflicts](GitConflicts.md).

Three Git milestones extend existing ownership. Optional versioned Git selection
metadata joins the existing per-instance WorkspaceSession and validates repository
identity before restoration. GitWorktreeMaintenance shares Git selection/catalogues
and the VCS mutex, but owns portable requests, local previews, pointer/index/HEAD/
directory guards and one move/repair process. Repair audits the complete linked
set and common metadata; postflight preserves actual/uncertain outcomes for both
target and reviewed siblings. GUI/SDK/ABI2 use the same service with stale-preview
discard and explicit read/mutation permissions. No executable target, third-party
dependency, CyUI module or C ABI change is introduced. See
[Git worktrees](GitWorktrees.md) and [Workspace sessions](WorkspaceSessions.md).

Submodule settings reuse the common address validator and add fixed URL/branch/
remote-default argv planning. The Engine binds both working and indexed metadata,
parent branch context and relevant directories/pointers to a separate preview.
An ordered config remainder verifies unrelated records independently of the one
target key; the index and checkout are preserved. Unsafe old values are redacted,
effective branch overrides are reported and already-configured branches are not
dispatched. UI/SDK/ABI2 retain one owner and real/uncertain postflight outcomes.
No dependency, executable target or CyUI module is added. See
[Git submodules](GitSubmodules.md).

Submodule deinitialization extends the same UI-free model and Engine owner with
complete bounded index/flag validation, an explicit preview revision and one
offline/no-force Git action. Parent-owned absorbed metadata, native documents,
normal .gitmodules and complete cleanliness including ignored files are checked
again before dispatch. Actual retained metadata and unregistration/removal are
verified after Git, preserving uncertainty even on exit zero. UI cancellation and
stale async results cannot apply a preview. ABI2 and SDK share the same service;
no dependency, executable, CyUI module or source Undo transaction is added. See
[Git submodules](GitSubmodules.md).

Submodule addition extends the existing model/Engine with normalized fixed argv,
local preview and a distinct clone/stage operation. Catalogue, parent HEAD/reference,
destination/metadata directories and normal indexed .gitmodules bytes bind the
preview revision; inputs/context are checked again before dispatch. UI discards
cancelled/stale asynchronous previews. SDK and ABI2 share permission and process
ownership, with no new dependency/executable/CyUI module. See
[Git submodules](GitSubmodules.md).

GitSubmoduleModel parses NUL-delimited path/index records and builds fixed argv.
Engine owns catalogue fingerprints, filesystem/document guards and actual Git
processes under its existing VCS mutex. GUI, real SDK/MCP and ABI2 plugins share
one selected-repository service; raw URL/config values stay private. Update uses
the recorded gitlink commit and preserves explicit uncertainty/refresh results.
No dependency, executable target or CyUI module is added. See
[Git submodules](GitSubmodules.md).

RunToCursor owns one revisioned temporary source-breakpoint lease, independent
of persisted project breakpoints. Engine supplies native document/disk guards;
the existing DAP worker arms/verifies the complete replacement, resumes once and
restores the original list on an actual stop. Tracked pending replies, actual
stop confirmation and explicit guarded repair prevent blind continuation or
restoration after uncertain outcomes. GUI, SDK/MCP and ABI2 plugins share this
owner; no executable, dependency or CyUI module is added. See
[Run to cursor](RunToCursor.md).

DebugTargets now owns both issued target families through one implementation.
Goto queries use the actual selected frame source, privately preserve/compare
bounded adapterData and expose only filtered metadata. Separate catalogues and
markers expire through the existing DAP worker. Targeted goto reuses execution
acknowledgements/epochs but awaits an actual stopped event with pendingStop,
invalid inspection and a bounded unconfirmed deadline, without inventing resume
or automatic retry. StepInTargets is a compatibility alias; both views share one
stable target table. See [Set next statement](GotoTargets.md).

StepInTargets owns one bounded, pause/frame-bound catalogue with private signed
adapter IDs and monotonic public handles. Debugger serializes queries and targeted
stepIn on its existing DAP worker, rechecks response context, publishes the current
catalogue ID and expires choices on context changes or execution submission.
Targeted actions reuse DebugExecution acknowledgement/event ordering with public
handle-only metadata. GUI, SDK and ABI2 requests share that owner; no dependency,
executable or CyUI module is added. See [Targeted step-in](StepInTargets.md).

InstructionBreakpoints owns a bounded session-only replacement list, revisions,
enabled-to-wire mapping, actual verification and deferred breakpoint events.
Debugger retains DAP/session/execution ownership and explicit context guards.
DebugInspection provides common verification parsing reused by data breakpoints;
their existing bounds remain compatible. Disassembly views capture their pause,
and instruction drafts retain stable UI row IDs. GUI, SDK and ABI2 host exports
share this owner, with no persisted process addresses, dependency or new CyUI
module. See [Instruction breakpoints](InstructionBreakpoints.md).

DebugExecution validates bounded forward/reverse/frame-restart plans, observed
contexts and actual acknowledgement metadata without a UI dependency. Debugger
serializes them on its existing DAP owner and records applying/accepted/unconfirmed
outcomes. Queued legacy context is pinned; new operations require explicit guards.
An execution epoch protects rapid new stopped events from a late acknowledgement
without confusing ordinary cache invalidation with program execution. GUI controls,
menus/keymaps, SDK and ABI2 host requests share this service, with no dependency or
CyUI module added. See [Execution controls](DebugExecution.md).

DebugMutations validates bounded value/expression/byte operations and adapter
results without UI dependencies. Debugger keeps those operations on its existing
worker with explicit session/stop/frame guards, a separate execution epoch,
expected-memory preflight/read-back and reference invalidation after attempts.
DebugMutationsUI is only a captured-context form and byte preview. SDK/MCP and
ABI2 host exports share the same process.execute service and retain its result
after submission. No dependency or CyUI module is added. See
[Value and memory editing](DebugValueEditing.md).

DataBreakpoints owns bounded pause-bound inspection handles, private DAP identities
and reported watchpoint configuration without UI dependencies. Debugger retains
transport/session ownership, revision-guards full-list replacement, marks uncertain
mutations and adapts actual verification/invalidation events. UI, SDK and ABI2
host requests share that owner; data targets are session-only and the SDK mutation
is noncancellable after submission. See [Data breakpoints](DataBreakpoints.md).

Java Hot Code Replace uses the existing Java language-service owner for a fixed
incremental JDT build request and the same DAP connection for redefineClasses.
Debugger owns per-call cancellation flags/IDs, bounded reply/custom-event parsing,
session/backend checks and inspection invalidation; Engine supplies saved workspace
document identity/revision snapshots. UI, SDK and ABI2 host exports share those
services. The SDK retains its reservation until the native result after matched
cancellation; imports, autobuild and VM restart remain explicit user choices.
Runtime acceptance remains pending. See [Java debugging](JavaDebugging.md).

JavaDebug validates typed Java launch options and converts literal argv to the
Java DAP wire format. It also validates persistent JDWP endpoints and builds
attach requests containing only endpoint/source context. ProjectTasks prepares actual main/runtime-classpath profiles;
LanguageService issues the fixed JDT LS session command. Debugger retains DAP,
breakpoint/inspection/terminal ownership through LanguageClient's framed transport
and a bounded LoopbackStream connection. Java backend epochs invalidate sessions;
native/plugin stdio owners remain unchanged. See [Java debugging](JavaDebugging.md).

The reserved javaAttach profile field retains host/port/timeout separately from
launch options. The existing Debugger performs launch or attach through the local
JDT LS adapter, records requested attach targets, refuses target-terminal reverse
requests for attached Java sessions and disconnects without terminating existing
JVMs. Legacy profiles retain local defaults; SDK schemas preserve the native PID
route and optional legacy outputs. Registration/configuration do not resolve DNS
or connect to the endpoint. Runtime/network acceptance remains pending.

RustToolchain validates symbolic installed-toolchain names and supplies literal
Rustup argv/environment plans. Cargo tasks and the existing language server owner
consume the same helpers, without a discovery subprocess, installer, shell or
second process owner. RustAnalysis retains unresolved names in local preferences
and derives compatible sysroot/env settings. See [Rust toolchains](RustToolchains.md).

RustAnalysis supplies bounded typed Cargo-to-analysis profiles and derives both
initialization and configuration settings without changing the user's base launch.
ProjectTasks inspection exposes the data-only projection; LanguagePreferencesEngine
validates enabled paths/settings before saves, and LanguageService retains server
process/epoch ownership. Desktop, SDK and ABI2 plugins share the existing configure
operation and preference locks. See [Rust analysis](RustAnalysis.md).

CargoBuild validates typed Cargo options and adapts actual format-version 1
workspace metadata and compiler-reported executable identities. ProjectTasks
retains process/cancellation/cache ownership; ProjectTasksUI adapts the shared
model with CyUI Core. SDK and ABI2 plugin services forward to the same native
adapter without a TOML parser, second process owner or new ABI. See
[Cargo workspaces](CargoBuilds.md).

ChangeAnnotations validates passive LSP annotation data, normalizes server IDs and
retains only effective text-edit groups. The existing change store binds IDs to
issued plans and checks required acknowledgements before mutation. SemanticUI
adapts the same plan metadata with CyUI Core; SDK/native-plugin requests share
the Engine guards and Undo without another parser or ABI. See
[annotated changes](ChangeAnnotations.md).

SessionViewsUI captures and restores the shared editor's wrap, public folded-line
state and document scroll anchors without another folding engine. WorkspaceSession
validates optional versioned editor/image metadata; ImageView adapts the existing
CyUI canvas after asynchronous decode/viewport availability. Source fingerprints,
capture budgets and input precedence guard replay. No source, Undo or process state
is serialized. See [workspace sessions](WorkspaceSessions.md).

SemanticTokens validates and reconstructs LSP full/delta integer arrays and owns
bounded immutable LRU bases. LanguageService keeps that cache on the existing
server worker; LanguageClient negotiates support and tracks server refreshes
separately from document invalidation. Highlighting retains Unicode/range guards
and the existing desktop/SDK/plugin output. See [semantic updates](SemanticTokenUpdates.md).

CMakePresetActions extends the existing native reader with test/package and ordered
workflow metadata. BuildSystem supplies reviewed plans and CTest/CPack/CMake argv
execution under BuildOperations; CMakePresetsUI adapts the same services using
CyUI Core. The real SDK and native plugin host share them without a Python parser,
another process owner or C ABI layout change. See [Preset actions](CMakePresetActions.md).

Hierarchy validates the LSP call/type hierarchy data contract without a transport
or UI dependency. HierarchyEngine owns bounded issued sessions, opaque server
items and document/notification/backend guards; LanguageService retains the
server processes and request queue. HierarchyUI adapts these read-only services
with CyUI Core, and the real SDK/native plugin host expose the same operations.
See [Hierarchies](Hierarchies.md).

GitRepositories supplies bounded filesystem discovery and explicit repository
selection without another Engine/document model. GitWorktrees owns Git's
NUL-delimited catalogue, identity/revision guards and bounded native mutations;
GitWorktreesUI adapts those same services through CyUI Core. Documents and SDK
file paths stay workspace-relative, while Git receives repository-relative literal
pathspecs. No Python Git backend or new native-plugin ABI is introduced.
See [Git repositories and worktrees](GitWorktrees.md).

`DocumentReference` distinguishes issued untitled references from filesystem paths.
`DocumentDrafts` keeps draft text/revisions in the existing Engine document model
and commits Save as through the shared text writer. `DocumentDraftsUI` adapts the
existing CodeEditor without replacing its Undo/window identity. Recovery and
session metadata retain their current owners; the real SDK and ABI2 host route to
the same native mutations. See [Untitled documents](UntitledDocuments.md).

`PluginPackages` owns signed local extension installation, publisher trust,
immutable versions and guarded selection/rollback/cleanup outside workspaces.
`PluginPackageData` provides bounded metadata/path/streaming-integrity operations
with pinned Monocypher Ed25519/SHA-512. `PluginPackagesUI` uses common Core widgets;
the SDK forwards to the same native service. Shared/exclusive `NativeFileLease`
ownership prevents cleanup while a host retains a version. The loader and manager
reuse `PluginManifest` validation; no UI, scheduler or plugin ABI is duplicated.
See [Plugin packages](PluginPackages.md).

`DebugInspection` validates capability-gated DAP memory/disassembly, paged
variables and function/exception breakpoint contracts. The existing Debugger owns
transport, stop generations, saved profiles and process lifecycle; the desktop,
MCP and plugin host share those services. See [Advanced debugging](AdvancedDebugging.md).

`JvmBuild` supplies UI-free Java argv launchers and bounded real Maven/Gradle model
readers to `ProjectTasks`. Build/test/run retain the common cancellation/deadline,
workspace guards and terminal owner. See [JVM builds](JvmBuilds.md).
`ShaderBuild` supplies typed DXC/glslang argv, bounded artifact checks and
workspace-mapped diagnostics to that same ProjectTasks process owner. Its desktop
panel also works as an explicit secondary task view for C++/Unreal projects;
see [Shader compilation](ShaderBuilds.md).

`WorkspaceSession` stores bounded per-instance tab/panel/layout metadata outside
project sources; `SessionUI` adapts the common editor and ImGui models.
`NativeFileLease` prevents concurrent session ownership and debug-profile writes.
Text recovery and Undo retain their existing owners. See [Workspace sessions](WorkspaceSessions.md).

`cycode_process` shares argv execution, bounded child ownership and installed-tool
discovery between the native core and bundled SVN/Perforce/Mercurial providers. SVN uses the
same XML library, provider descriptors/root mapping, live/disk guards and shared
Diff/read-only Review views without linking IDE/UI services into its DLL. See
[Subversion](Subversion.md).
Perforce uses tagged JSON and the same project provider routes, with checked
client mappings and file-scoped submit/revert. Typed SDK Git handlers explicitly
select the built-in Git provider and an explicit workspace repository; generic plugin requests retain their
own permissions/schema and longer transport wait. See [Perforce](Perforce.md).
Mercurial reuses those provider routes and process utilities with structured JSON,
exact file selectors, local repository identity and clean-update checks. Its DLL
also remains independent of IDE/UI services; see [Mercurial](Mercurial.md).
Diversion also reuses those routes/process services, with documented CLI text,
local/cloud sync metadata and guarded actions rather than Git staging/push/pull.
See [Diversion](Diversion.md) and the [custom-provider contract](CustomVersionControl.md).

`DebugAdapters` validates declarative ABI2 stdio-DAP definitions and request options.
Engine publishes one active-plugin service snapshot to the existing LSP/DAP owners.
The Debugger retains native transport/process ownership, explicit provider IDs,
queued-launch epochs and revocation across disable/change/shutdown. GUI/MCP/plugin
requests share this worker; no plugin callback owns a debugger process. See
[Plugin debuggers](PluginDebuggers.md).

The native language owner maps versioned diagnostic publications back to live
document revisions. `DiagnosticsUI` checks those revisions and uses the common
CodeEditor squiggle/tooltip hook through `DiagnosticAnnotations`; the core keeps
its UI-free boundary. See [Editor diagnostics](EditorDiagnostics.md).

`Events` owns a bounded metadata journal with epoch/cursor paging. Engine emits
committed changes; ABI2 subscribers run on a serial native worker outside document
locks, with manifest permissions, read-only snapshot handshakes and error
suspension. Shutdown joins event delivery before releasing plugins/services.
See [Workspace events](Events.md) for ordering, retention and native-code limits.

`XmlSolution` maps `.slnx` folders/projects/configuration rules into the shared
solution/MSBuild model through a pinned UI-free XML reader. `CMakePresets` resolves
bounded metadata, include/inheritance graphs and workspace binary paths; actual
preset execution remains in CMake. GUI, MCP and plugins use those native services.
See [Project formats](ProjectFormats.md).

Application-owned `LanguagePreferences` stores unresolved per-workspace LSP launch
and activation choices outside project configuration. OS locks, checked revisions
and the shared atomic text writer guard multi-instance edits. Engine routes GUI,
MCP and plugin controls to that same store and existing server owners; inspection
does not activate saved servers. See [Language preferences](LanguagePreferences.md).

`BuildOperations` coordinates CMake, MSBuild and explicitly dispatched Unreal work
with one native reservation and a matching cancellation ID. `BuildEngine` routes
shared GUI/MCP/plugin requests. Status/cancel bypass execution locks; process
ownership remains held through termination. GUI background Unreal analysis retains
its independent token. See [Build cancellation](BuildCancellation.md).

Installed plugin manifests are checked against the native host version and
process platform before library loading. ABI2 retains its original host prefix,
adds optional feature bits and serial activation notifications with no host
requests. Guarded project activation commits precede notification; enabled dormant
dependents initialize after their dependencies become active. Libraries stay
pinned until reverse-order shutdown. See [Plugins](Plugins.md) for the ABI and
the distinction from native-code isolation.

Configured CTest discovery/execution, issued Git hunks, regex searches of unsaved
buffers and plugin-owned LSP definitions share the UI-free native Engine. The
language registry is workspace-local, with host-owned server workers and routing
epochs; it does not modify the common syntax catalogue. See
[Development workflows](DevelopmentWorkflows.md) for process ownership, cancellation,
limits, GUI/MCP/ABI2 contracts and remaining scope.

The desktop selects Core, Code/CodeEditor, Diff, Commands, Diagnostics,
DocumentViews, Inspector, Assets and Images/Canvas from modular CyUI. The Engine/core/CLI have no UI
dependency. See [CyUI migration](CyUI-Migration.md) for exact pins, view ownership,
source adapters and limits.

Project creation uses the UI-free `ProjectTemplates` service. The desktop wizard
renders native plans through CyUI Core/Code; MCP and native plugins call the same
Engine routes within their workspace boundary. The desktop explicitly selects
its parent directory without changing those permissions. The shared
`FileTransactions` primitive provides exclusive renames for resources and project
publication. See [Project templates](ProjectTemplates.md).
ABI2 template contributions contain data only. `TemplateDefinitions` checks and
expands bounded UTF-8 files/options; `Plugins` supplies an atomic active-registry
snapshot and holds its activation lock through creation. Snapshot reads happen
before the plan mutex; commit always locks the plugin host before the plan store,
including requests from a plugin's callback. No plugin callback executes during
template expansion or file publication.

Image tabs use the app-owned `ImageLoader` worker, bounded native decoders and
deferred OpenGL textures, rendered by CyUI::Images. They are separate from Engine
text documents and retain the shared tab/docking lifecycle. See [Images](Images.md).

CyCodeReader lives in its [own repository](https://github.com/MrMybal/CyCodeReader).
Its editor/ source module provides the same cycode_editor and cycode_fileicons
targets to the IDE, at the revision in app/editor-source.lock.json or an explicit
CYCODE_EDITOR_ROOT development checkout. CyCode does not build the Reader app.
The Reader hosts dockable file editors without an
Engine, language server, build adapter or Core companion. cycode_files owns UTF-8
validation, newline encoding, bounded reads, path conversion and guarded atomic
saves used by both applications. The IDE retains workspace/revision permissions;
Reader uses files selected directly by its user and keeps per-document disk
baselines across directories. Its application-specific extensions are documented
in the Reader repository.

Interactive execution uses the shared native Terminals and Debugger services.
Terminals owns bounded libvterm screens/history and real ConPTY/POSIX PTY process
trees, with separate read/write/supervisor threads. Only visible grid rows are copied
for rendering. Debugger has one owner thread for DAP and a bounded command queue;
the framed JSON transport is shared with LanguageClient without LSP initialization.
Adapter reverse terminal requests create native terminals with explicit argv/env.
DebugRuntime detects native/managed outputs and selects a compatible .NET host.
CodeLLDB and NetCoreDbg share this DAP owner, inspection state and persistent profile;
NetCoreDbg output uses the debug console, while interactive managed Run uses a PTY.
Existing attached processes detach on stop; launched processes terminate. Persistent
profiles/breakpoints use atomically replaced workspace metadata. See [Execution](Execution.md).

CyCode starts with C/C++ and keeps language support, project/build adapters, editor
views and revision-control providers as separate extension points. A workspace can
eventually contain several project types. CMake, MSBuild and UnrealBuildTool are
executable adapters; solution inspection does not evaluate MSBuild. Explicit MSBuild
evaluation reads real target properties/items, produces managed clangd metadata and
tracks local solution/project/import inputs. Builds share the same native adapter
with MCP; source diagnostics feed the Problems panel.

ProjectTasks supplies Cargo metadata/compiler JSON, npm scripts and standalone
javac compilation to both GUI and MCP. It owns one cancellable native task with
bounded output/deadline and uses Terminals for persistent Run. The private IPC
bridge accepts a bounded set of concurrent connections so cancellation remains
receivable while a compiler is running. CyToolsCore continues to own scheduling,
permissions and identities; matching native operation ids only correlate process
cancellation. See [Project tasks](ProjectTasks.md).

```text
CyUI / SDL3 desktop                 External MCP clients
        |                                  |
        |                           official MCP SDK (stdio)
        |                                  |
        |                        CyToolsCore authenticated HTTP
        |                        runtime, jobs, sessions, permissions
        |                                  |
        +-------- native Engine <--- private loopback command bridge
                     |
             documents / search / Git / CMake / Unreal / trusted plugins
                     |
             LanguageService router
                +-- C++ owner thread <---- LSP transport ----> clangd
                +-- C# owner thread  <---- LSP transport ----> csharp-ls/Roslyn
```

The native Engine has no UI dependency. Documents carry monotonically increasing
revisions, UTF-8 content, original BOM/newline convention and disk baseline. Saves
reject stale revisions and external modifications before atomic replacement.
Workspace paths are canonicalized and checked by path components, including symlink
resolution. New files use exclusive creation. A task cleanup never removes source files.

Resources owns bounded filesystem snapshots and issued plans, exclusive moves,
guarded inverse actions and persistent project trash. Engine retains the live
document model and gives text documents stable identities across path changes.
Filesystem mutations emit bounded cursor events; the GUI rebases tabs/navigation
without replacing editor Undo or native window identity. Deleted open buffers remain
available with a missing marker. CyUI::Assets supplies the physical browser and its
action requests. These operations are distinct from semantic symbol/file rename;
see [Resources](Resources.md) for the SDK, limits and remaining integration gaps.

Documentation and signature requests use those same live document/revision guards.
LanguageInfo normalizes display-only sections and UTF-16 signature parameter offsets
into bounded UTF-8 labels. The GUI keeps one information request in flight and
discards responses for obsolete cursors; it follows the visible panel with a small
debounce. It never executes backend Markdown, HTML or documentation links.

The desktop polls document metadata, copies text on changes, indexes project paths
off the UI thread and delegates content search to ripgrep. WorkspaceMonitor owns
recursive Windows directory notifications with bounded buffers and overflow
reconciliation; other systems currently use a bounded directory polling fallback.
It publishes counters, never callbacks into UI or document services. Background
workspace.poll reloads clean buffers and preserves dirty, missing or invalid files
with a disk-conflict marker. A two-second reconciliation also covers open files in
excluded directories. Replacing a search cancels its native process, and generation
IDs suppress obsolete results. Dirty buffers supersede disk text in literal and
regex searches; regex uses bounded private snapshots and native ripgrep cancellation.
The path index is cached in memory, invalidated by structural/ignore-file events,
and paged for clients (up to 200,000 indexed paths). Persistent path/symbol indexes,
incremental symbol queries and large-file editing remain future work. See
[Workspace monitoring](Workspace.md).

The extension host supports ABI1 and the instance-based C ABI2. It resolves manifest
dependencies before loading, preserves libraries until callbacks have stopped, and
exposes JSON descriptors for commands, menus, panels, syntax, language servers,
project templates and VCS/project providers.
Application owns the UI and CyUI syntax-registration lifetimes. Project activation
and VCS root mappings have revision/disk guards and independent workspace metadata.
Git remains built in; selecting another provider changes the available actions and
does not silently fall back to Git if that provider disappears. The bundled Unreal
extension uses existing native services rather than another project parser/build
engine. The original Unreal project panel and backend remain in the application.
See [Plugin SDK](Plugins.md) for contracts and remaining extension boundaries.

Every native text editor is owned by CyUI::CodeEditor behind native document services. The GUI
build prepares a patched copy in its build directory using prepare_texteditor.py;
the checksum-pinned downloaded source is unchanged. The small adapter adds function,
macro and reflection-attribute roles, a context tokenizer and a read-only token query
for tests. EditorLanguage supplies bounded lexical C/C++/Unreal and C# rules; the
widget retains incremental multi-line comment/string state. Preprocessor directives
and their strings/comments are tokenized separately. Built-in keywords use one blue
role; Unreal naming conventions supply types and reflection macros/metadata have
dedicated roles. Function calls/declarations are recognized lexically. These fast
rules remain available while project analysis loads or a backend is unavailable.
EditorTheme owns the shared palette for editors,
change previews and VCS views, with contrast checked against both editor grounds.

Highlighting.cpp requests full LSP semantic tokens through the existing C++/C# owner
threads. It decodes each server's negotiated legend and relative UTF-16 positions,
validates single-line/non-overlapping ranges and surrogate boundaries, and returns
UTF-16 positions plus separate Unicode glyph indexes. The same revision-checked
language.highlight operation serves GUI and MCP. Results are capped at 50,000 spans
or a 4 MiB token payload, with explicit truncation. Edits to any live document,
server restart and semantic refresh requests invalidate the backend generation.
The GUI debounces requests by 300 ms and runs at most two color queries concurrently,
prioritizing the active and visible detached editors. Closed-tab queries retire
asynchronously rather than blocking tab closure.

The widget stores semantic display colors independently from lexical glyph roles.
Its transaction version disables obsolete spans immediately, including after undo;
normal keyword/string/comment state, identifiers and completion stay intact. Colors
map actual aliases/classes, functions/methods, variables/parameters/fields, macros
and enum members; clangd's inactive-code comment spans use a readable muted role.
Unknown/dependent classifications retain lexical colors. C# uses the same adapter
with Roslyn's legend. Delta/range streaming, advanced modifier styling and broader
raw-string/preprocessor fallback coverage remain pending.

External buffer replacements are applied as editor transactions to retain undo. The LSP
transport queues writes and delivers replies on a dedicated LanguageService owner
thread. One clangd process serves both UI completion/navigation and MCP queries;
Engine document opens, changes and closes synchronize its unsaved overlays. Requests
never wait on the UI thread. A maximum of 32 outstanding requests and a 15-second
deadline bound the queue, including initialization; expired requests send LSP
$/cancelRequest. Restart/shutdown fails in-flight requests and joins the child.
The transport bounds queued writes by count and bytes; document synchronization
retries under backpressure and completes before semantic queries are sent.
Engine checks the source revision and live-document generation before accepting
semantic replies; UI completion also checks its generation and cursor. The editor
uses 32-bit Unicode codepoints; LSP uses zero-based UTF-16 positions, and ripgrep
columns are byte offsets. Navigation converts those coordinates explicitly and
decodes percent-escaped file URIs. Results outside the canonical workspace are
counted but omitted by default. With includeExternal, registered engine/compiler SDK
roots issue opaque read-only source references; they do not extend workspace write
permissions. This initial adapter launches clangd; other servers need their
own launch settings, project configuration and integration validation.

Semantic.cpp normalizes workspace/document symbols and maintains bounded rename
and formatting plans. TextEdits validates UTF-16 ranges, surrogate boundaries and
non-overlapping replacements. Plans snapshot live revisions and text; apply/undo
validate every affected buffer and its current disk baseline before changing any
buffer. Saves remain explicit. The desktop and MCP use these same operations.
SDK documents live in a separate registry, can be read only through issued handles,
and cannot be passed to document write/refactor operations. See [Semantic editing](Semantic.md).

Commands are registered once for menus, the keyboard settings and action palette.
The toolkit-independent Keymap resolves profile bindings, contexts, custom overrides
and multi-stroke chords. The UI consumes matched keys before the editor widget's
fixed shortcuts, preserving ordinary typing/caret handling. This prevents Rider's
Ctrl+Y/Ctrl+B from invoking the widget's redo or the former build shortcut.
Initial completion waits for the document's first diagnostics publication: clangd's
initialized status alone can still produce unrelated index-only completions while
its first preamble is being built.

BuildSystem owns tool discovery, CMake execution, cache compatibility checks,
profile persistence and File API inspection. UI and MCP use the same native
operations. Configure requests codemodel/toolchain replies and selects the native
compilation database or derives one from File API metadata for Visual Studio.
The active database directory initializes the shared clangd; successful reconfigure
reloads it while retaining live overlays. Local profile state is .cycode/build.json;
generated analysis databases stay in the build tree. See [Build services](Build.md).

Solution inspection accepts directories, .sln and .uproject entries and keeps the
canonical document root at the project directory. Missing/external references remain
metadata rather than new document permissions. UnrealProject discovers the associated
engine and invokes UBT using argv. Database generation reloads the same LanguageService
used by the GUI/MCP. See [Unreal integration](Unreal.md) for tested scope.

SDL3 and the ImGui OpenGL3 renderer provide native platform viewports. Each document
is a dockable window; detached editors and panels own distinct SDL/OS windows and GL
contexts. Rendering restores the main context afterwards. Closing a panel hides it;
View/sidebar actions restore it. Editor closes retain unsaved-document checks.
Independent processes own their engines, buffers, descriptors and jobs. Layout
filenames are keyed by canonical project entry; global preferences use unique temporary
files and atomic replacement. File icons use original vector drawings, semantic
palette colours and DPI-aware geometry without OS symbol-font assets.

CyToolsCore is installed as a real wheel into runtime/python. One companion per GUI
instance shares the Core scheduler across MCP connections. Each instance has unique
private native/Core descriptors. The native token protects local IPC; Core owns
assistant authentication and operation permissions. The bridge is application IPC,
not another MCP parser, scheduler or job registry. The service exits when its native
descriptor is removed. Jobs and artifacts use the SDK lifecycle.

Source document paths are workspace references, not file-import fields: importing a
copy would disconnect edits from the open editor. Temporary generated reports belong
in JobContext.path; explicit source saves remain persistent operations.

Git mutations are serialized by the native Engine. Vcs.cpp provides structured
repository metadata and guarded working-tree changes. Changed clean documents
reload through the same revision/LSP service; dirty buffers survive concurrent
operations. VcsUI consumes structured responses asynchronously and keeps conflict
editing in normal document tabs. Providers continue to use the existing C ABI.

CyUI is an external configurable checkout, not a vendored fork. Platform-specific
code includes process/IPC, Windows engine registration and the Unicode entry point;
SDL3 supplies window/input/render integration. Portability of source is not a claim
that Linux/macOS builds have run.

The shared editor library owns per-document incremental Tree-sitter parsers for
Rust/Java/JS/TS/TSX/HTML/CSS, with display spans separate from lexical and semantic
overlays. Both applications and refactor/conflict previews use this same layer.
The SDL-free core routes language requests to independent server owners for C++,
C#, Rust, Java, TypeScript/JavaScript, HTML and CSS. Initialization options and
workspace configuration are separate; the service respects advertised methods and
maps LSP workspace-edit versions to native document revisions. See
[language support](Languages.md) for runtime prerequisites and deliberate limits.

CompletionEngine.cpp holds bounded issued-item sessions tied to document revision,
workspace generation and backend epoch. Resolve receives only cached server items;
prepare turns the primary edit and additional imports into the existing ChangePlan
transaction after checking UTF-16 ranges, overlaps and disk baselines. The MCP
surface is read-only until changes_apply. The build-local editor patch intercepts
acceptance without inserting a partial symbol. The GUI prepares asynchronously,
checks cursor/revision/generation again, applies the native plan and mirrors it as
one tab-preserving undo transaction. A headless actual widget test verifies the
acceptance hook; complete desktop acceptance remains separate.

Snippet.cpp is an SDL-free bounded parser/expander linked by the core and editor.
Completion preserves field byte offsets through indentation and additional imports.
EditorSnippet converts them once to glyph coordinates and uses the widget's detailed
transaction callback to track linked fields without rescanning document text for
every keystroke. The build-local adapter adds validated multi-selections and a
pre-key handler; ordinary multicursor edits supply atomic mirror typing. Finishing
a session collapses only its recorded cursor history so undo/redo cannot revive
temporary linked cursors. The widget's text extraction also preserves exact final
newlines rather than implicitly appending one to nonempty files.

CodeActions.cpp owns separate bounded issued-action sessions and lazy resolution.
LanguageService adds overlapping published diagnostics to code-action context,
matches decoded file URI identities and maps versioned edits to native revisions.
LanguageClient invalidates diagnostic eligibility for actions on edits while
retaining the last displayed diagnostics and fast completion; versionless
publications remain advisory. New target overlays must be synchronized before a
fresh query, preventing an edit from adopting an unrelated disk baseline. Shared
workspace-edit parsing and ChangePlan validation serve rename and context actions.
GUI and MCP use those same plans; no code-action command is executed. Only the
pinned TypeScript server's verified empty acknowledgement is ignored. Resource
operations and annotated/snippet action edits remain explicitly unavailable.

RecoveryStore is UI-free and keeps per-instance, OS-locked UTF-8 checkpoints under
.cycode/recovery. Engine owns a joined background worker that snapshots dirty
document revisions and writes immutable payloads before an atomic index. Restore
validates checkpoint/target/disk guards, installs a live buffer and transfers its
backup before retiring the source. The GUI uses CyUI Code for bounded previews
and the existing editor replacement transaction for Undo. Native documents remain
the only mutable model shared with MCP; checkpoints never save project files.
See [Recovery](Recovery.md) for quotas, failure handling and explicit limitations.

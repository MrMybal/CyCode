# Roadmap

The progressive-capacity stage adds [paged native text and storage](LargeProjects.md):
streamed compiler databases, incremental checkpoint pages and disk-backed native
transactions. Paged documents now add sparse line/UTF-16 reads, exclusive Save as
and explicit durable checkpoint preview/restoration through Apply/Undo. The public
SDK catalogue has 249 operations. Graphical/LSP adapters and automatic per-keystroke
recovery remain separate; ordinary GUI/LSP buffers retain their compatibility policy.
No hardware-unlimited or exhaustive Unreal claim follows.


## Durable compiler traversal — 2026-11-11

Native analysis now offers explicit disk checkpoints, discovery and restoration,
including after an in-process document change. Restored observations become
unknown historical metadata. Isolated source failures no longer stop a read-only
traversal, and current-file timing makes slow work observable. Real production
SDK/MSBuild/clangd checks cover 162 files, cancellation and restart; the real UE
campaign continues a 37,557-unit inventory. See [persistent progress](AnalysisProgress.md).
Full Unreal semantics, advanced resource refactorings and Linux/macOS execution
remain separate work; no whole-engine completion is inferred from this stage.


## Core quality stage — 2026-11-11

Shared atomic publication, readable MSBuild preparation, native cancellable
clang-tidy/Clang Static Analyzer inspection and guarded clangd command-to-preview
transformations are implemented. Explicit real SDK/compiler/debugger/JetBrains
comparison runners distinguish evidence from declared capability. See
[Core quality](CoreQuality.md). Full Unreal/Blueprint analysis, complete semantic
transformations, wider debugger/remote coverage and graphical/platform acceptance
remain separate stages, with no claim of Rider/CLion parity.


Local C/C++ include navigation now handles inline directive comments and immediate
backslash-LF/CRLF continuation components, retaining the physical header range.
The shared result inspector adds explicit retry/index-refresh, path/report copying
and contextual local help; file-page assembly guards both revision and generation.
These are implemented source/build milestones. Real filesystem/cache races,
clipboard/modal keyboard/DPI and SDK/plugin acceptance remain unexecuted during
the native-run suspension. Compiler search roots, macro/condition evaluation,
nonspliced multiline directive comments and semantic include resolution remain.
See [Includes](CppIncludes.md) and [Related results](CppCounterparts.md#inspect-related-results).

C/C++ header/source switching now combines siblings and nearest conventional
Unreal Public/Private/Classes or include/src layouts with same-stem paths from
the current workspace index. It discovers namespaced/other-layout matches through
bounded offset lookup, reports missing/busy/stale/partial/changed coverage and
guards cache generation without starting a scan. GUI and read-only SDK/ABI2 share
source lifetime/revision guards and explicit disambiguation. Different-stem naming,
semantic/clangd pairing, module interfaces and native/GUI/POSIX acceptance remain.
See [C++ companions](CppCounterparts.md).

Selected source can now become a literal personal snippet draft through Edit /
Find Action and snippets_user_draft. Unique naming, source revision/generation,
escaping/body/catalogue bounds and explicit guarded Save reuse the native owner.
Native/GUI/Undo/POSIX acceptance remains pending; see [Capture](SnippetCapture.md).

Personal snippets now have a fixed installation-owned catalogue, a shared JSON
draft editor and explicit revision/lease/atomic save. Merge order is project,
personal, then plugins; sessions recheck saved content and the UI observes local
refresh generations. SDK 233 / ABI2 read/save share the same owner, with settings.write
separate from source edits. Real store/hash/lock/thread/GUI/Undo and Linux/macOS
acceptance remains unexecuted during the antivirus suspension. See
[Personal snippets](UserSnippets.md).

## Current milestone

Project insertion snippets now use a saved, bounded .cycode/snippets.code-snippets
catalogue with scopes, multiple prefixes and multiline bodies. Edit / Find Action
and Rider/Visual Studio bindings open the shared CyUI Commands palette. Local
proposals work without LSP, use the common cache/plan/field/Undo owners and guard
saved catalogue/source/workspace/disk state. SDK 233 / ABI2 share the same service, including personal storage read/save.
Rendering/regex preparation releases the Engine mutex and revalidates before plan
storage. Enabled ABI2 plugins now contribute inline catalogues with provenance,
stable ordering, bounded merging and registry revision/activation guards. Unreal
1.6.0 contributes four C++ definitions through that same feature. Native/server/
keyboard/GUI/Undo and POSIX acceptance, structured catalogue management,
OS-account-wide storage/synchronization, file-path imports, automatic server merging, file templates and smart reindentation
remain. Explicit single-selection wrapping now uses TM_SELECTED_TEXT with the
issued range, selection-anchor guards, literal layout and Rider Ctrl+Alt+J. See [Project snippets](SnippetCatalogue.md).

Completion now substitutes fourteen document/workspace/position variables and
explicit defaults in both GUI previews and the existing guarded native plan.
Literal escaping and bounded preprocessing preserve shared numbered fields,
choices, metadata and Undo ownership. Native/server/widget/platform acceptance,
further variable families, interactive placeholder transforms and OS-account-wide catalogue storage
remain. See [Completion variables](CompletionVariables.md).

Variable regex transforms now compute captures, conditional formats and seven
simple Unicode case methods through bounded native PCRE2/utf8proc preparation.
The popup keeps regex work off the GUI thread and shows the server label instead.
Actual matching/case/worker/Undo acceptance is still unexecuted under the antivirus
suspension. This is a documented regex/format subset, not full VS Code compatibility.

Tool discovery now preserves compiler/interpreter aliases, with POSIX executable
filtering, including CMake's previously separate resolver. Explicit compiler-cache
checks distinguish driver names even when they resolve to one binary.
Build failures retain compiler notes in a bounded preview, Windows CI
matches the Visual Studio 2022 generator, and failed CI builds preserve logs.
The shared editor source is updated to Reader 0.1.3 and CyUI supplies 102 syntax
modes including Verse. Windows production compilation and data-only reporting
checks pass. Actual POSIX alias execution, the revised remote matrix, GUI input
and UEFN/compiler/LSP integration remain to validate. See
[Toolchain reliability](ToolchainReliability.md) and [Languages](Languages.md).

Detailed update history now combines explicit metadata-only inspection, cached CyUI
text/category filters with twelve-row pages, and selected recovery details/copying.
Native reports exclude raw metadata; new receipts have informational timestamps.
Scan/row/byte limits and omitted-record notices remain explicit. Actual native/GUI/
clipboard/cancellation/theme/DPI acceptance, pagination beyond the bounded snapshot,
exhaustive/append-only auditing and Linux/macOS updates remain. See
[Update history](UpdateHistory.md).

The staged recovery helper now has visible EN/FR phase/count progress, cooperative
Stop/Escape/close requests and a joined worker that keeps the dialog alive until
work actually finishes. Result dialogs distinguish cancellation before/after this
attempt's installed-file moves, offer an explicitly copied report and retain an
exclusive best-effort plan-bound receipt. These are source/build/data checked;
real Windows UI/clipboard/close/blocked-I/O/partial-commit interruption and receipt
failure acceptance remain, alongside unattended/legacy and Linux/macOS updating.
See [Recovery controls](UpdateRecovery.md).

Recovery retries now reuse verified prepared originals from this active transaction,
with bounded exact ownership/source maps, mixed-directory references and fresh
capacity accounting. Missing/changed/foreign/legacy hints remain on disk and fall
back to verified original backups. Mutable phases do not authorize completion.
Actual resume/low-space/lock/copy/interruption/native-helper/GUI acceptance, space
reservation, power-loss durability and Linux/macOS updating remain. See
[Recovery resumption](UpdateRecovery.md).

Recovery now checks its own allocation-aware copy/retention headroom, stages and
verifies all originals before moving targets, and rechecks the complete file
baseline and metadata space before commit. Preview guidance and plan-bound
receipts distinguish insufficient/unknown capacity and preparation/commit failure.
Real low-space/quota/lock/copy/cancellation/native-helper/GUI acceptance, space
reservation, power-loss durability and Linux/macOS updates remain. See
[Recovery preparation](UpdateRecovery.md).

Installation now adds allocation-aware extraction/pre-replacement budgets, retained
space receipts, compatible typed failure history and complete-baseline rollback
verification. Pending-marker removal is guarded for install/rollback/recovery.
Source/build/data checks are separate from actual low-space/quota/filesystem/helper
and GUI acceptance. Space reservation, power-loss durability
and Linux/macOS automatic updates remain. See [Installation checks](UpdateInstallation.md).

Download/staging now checks additional space before transfer, full fallback and
preparation, with caller quotas and conservative helper/catalogue/journal/spare
allowances. Release-bound phase/counter observations and a sampled network-only
rate/ETA distinguish received bytes from verified readiness. Thirty actual policy
assertions are compiler-evaluated. Real low-space/quota/HTTP/GUI acceptance and Linux/macOS updating remain. See
[Download observations](UpdateDownloads.md).

Update retry now adds an explicit fresh-download action that preserves old data,
category-based EN/FR guidance and one quiet local refresh after failed/cancelled
attempts. Resume/fresh share native integrity/ownership guards, and a refresh does
not restore a proposal or network consent. Windows source/build/data verification
is distinct from pending real HTTP/locking/GUI/platform acceptance. See
[Download retry](UpdateDownloads.md).

Interrupted downloads now retain exact asset checkpoints, resume through validated
byte ranges and safely fall back to a fresh folder when Range is ignored. Full
SHA-256 remains mandatory; partial summaries are explicitly unverified. Nineteen
compiler-evaluated response assertions pass without native test execution. Actual
private/network/filesystem/GUI acceptance, power-loss guarantees and automatic
Linux/macOS transport remain; see [Download resumption](UpdateDownloads.md).

Update storage now adds explicit bounded inspection, acknowledged reversible
retirement of obsolete owned downloads, and guarded receipt-based restoration.
History/backups remain; this does not free disk space. Actual native/GUI acceptance,
permanent pruning, legacy/incomplete caches and pagination remain. See
[Update storage](UpdateStorage.md).

Guided format-2 update recovery now adds immutable preimages, bounded full review,
explicit EN/FR helper confirmation and resumable baseline restoration that retains
original backups. Native crash/filesystem/GUI acceptance, unattended restoration,
legacy journals and power-loss durability remain; see [Update recovery](UpdateRecovery.md).

Verified download rediscovery, bounded attempt history/reporting and cancellation/
retry states now extend the application updater. Guided recovery and explicit
partial-transfer resume extend these services; real helper/cache/GUI acceptance
remains pending.

Application updating now covers startup stable-GitHub discovery, explicit proposal,
verified download and post-close Windows x64 portable replacement/restart, plus
matching release archive authoring. Installation-scoped leases/manifest checks and
ordinary-failure rollback preserve local ownership. Real HTTP/private/helper/
filesystem/GUI acceptance, unattended/legacy recovery, signing, installers and automatic
Linux/macOS transport/packages remain. See [Application updates](Updates.md).

Actual document highlights now share bounded native Text/Read/Write normalization,
source/backend provenance and SDK/ABI2 access. CodeEditor draws independent
palette outlines; a dockable inspector adds kind filtering, navigation and atomic
reported-range selection. GUI/server/SDK/plugin/POSIX acceptance and multi-anchor
highlight/import-use parity remain pending. No exhaustive semantic-reference claim
or new backend annotation layering is made. See [Document occurrences](Occurrences.md).

Source navigation now adds actual declaration/type-definition/implementation
requests, bounded validated Location/Link normalization and precise source/backend
ownership for all five definition/reference variants. The result dock adds cached
filtering, cyclic keyboard selection/reveal, copy, stale inspection and explicit
refresh at the active caret. SDK/ABI2 share the native owner (226 SDK operations).
Native/server/index/SDK/plugin/GUI/POSIX acceptance is pending; no independent
analysis, merged index or guaranteed current other-file target positions are
claimed. See [Source navigation](SourceNavigation.md).

Symbol navigation now includes on-demand resolution of URI-only workspace symbols,
issued bounded sessions/cached replies and pre/post workspace/lifecycle/monitor/
backend guards. The palette waits for actual locations, isolates query/selection
replies and explicitly refreshes the same search. SDK/ABI2 share the native resolve
operation (226 SDK operations). Structure and built-in/plugin server choice remain.
Actual server/SDK/plugin/GUI/POSIX acceptance is pending; no merged cross-language
index or guaranteed fresh workspace-index positions is claimed. See
[Symbol navigation](SymbolNavigation.md).

Three diagnostics milestones now pair publication/document identities and
revisions across close/reopen/reload, add severity filters and full selected
message details, and navigate supported compiler output through shared
build.location / build_location (226 operations). No language/analysis engine is
added. Native/GUI/SDK/plugin/POSIX acceptance stays pending; current CyUI still
has no forced diagnostic-row scroll/selection API. See
[Diagnostics](DiagnosticNavigation.md) and [Compiler locations](CompilerLocations.md).

Three connected milestones now cover project task status/live output, Cargo
protocol display framing and filtered diagnostic keyboard navigation. Native and
project builds share TaskOutput's implementation but keep their existing owners.
GUI captured-ID cancellation and workspace guards include remote tasks; SDK/ABI2
expose project_status/project_output (226 total operations). Problems commands
reuse existing build/LSP data and guarded document opening, without a new analysis
engine. Runtime pipe/keyboard/SDK/plugin/GUI/POSIX acceptance remains pending; see
[Project output](ProjectTaskOutput.md) and [Diagnostics](DiagnosticNavigation.md).

Explicit native builds now expose a bounded, revisioned live output tail through
the Build panel, SDK and ABI2, reusing existing process/cancellation ownership.
Adapters opt in to main user-facing execution, excluding structured discovery.
The manifest has 226 operations. Runtime pipe/SDK/plugin/GUI/POSIX acceptance
remains pending; see [Live build output](LiveBuildOutput.md).

Ordinary Unreal Editor output inspection now checks target receipt context/type,
module BuildIds and receipt/manifest-listed project files through native UI/SDK/
ABI2/provider 1.5.0. Bound actual run/launch and prebuild continuation require fresh
scoped readiness, while configuration remains available before compiling. Native
build results retain process/postflight uncertainty; file lists are virtualized.
The SDK has 226 operations. Full engine/plugin dependency, ABI/architecture,
closed-source freshness and nonstandard layout validation remain outside this
assessment; real Unreal/GUI/POSIX acceptance is pending. See [Unreal outputs](UnrealArtifacts.md).

Unreal launch profiles now preserve their build context and normalized options.
Desktop prebuild uses the bound target/configuration, with captured parameters and
saved-profile/draft/build-ID/open-document guards before automatic debug launch.
Native configure/run/launch validate linked fields; explicit detachment supports
manual editing and legacy profiles remain usable. The current SDK has 226 operations;
provider 1.5.0 and ABI2 use the same core. Actual persistence/build/debug/GUI/POSIX
acceptance remains pending. See [Unreal launch](UnrealLaunch.md).

UE4/UE5 launch profile preparation now connects editor, standalone game and
uncooked server modes to the shared Debugger/Terminals. Typed map/listen/port/
resolution/log/entry/build options, DebugGame and observed-profile save guards
are exposed in CyUI/SDK/ABI2 and provider 1.5.0. Actual launch/debug/GUI/POSIX
acceptance remains pending; PIE/Blueprint/Live Coding/editor RPC and cooked
binary/session orchestration remain separate work. See [Unreal launch](UnrealLaunch.md).

Reviewed UE5 cooking/packaging now shares native/UI/SDK/ABI2 ownership, game target/
map/container controls, one-attempt input/tool/output guards, per-run staging/
archive/logs and actual cancellable build outcomes. The Unreal provider exposes
both new methods. Windows compilation and schema checks are distinct from
pending actual UAT/UBT/artifact/cache/cancellation/plugin/GUI/POSIX acceptance.
Editor/Live Coding integration, Blueprint/asset tools, advanced packaging profiles/
platforms/signing/deployment and complete package validation remain separate work;
see [Unreal automation](UnrealAutomation.md).

History actions and the Replay view now provide single-commit cherry-pick/revert,
source patch preview, resolved continuation and quit tracking that retains local
work/index. Native UI, three SDK methods and ABI2 exports share the common merge
inspector, guards, persistent intents and actual postflight. Windows compilation/
schema checks are distinct from pending real Git/SDK/plugin/GUI/POSIX acceptance.
Multi-commit/ranges/mainline, interactive rebase and abort/skip/reset recovery
remain separate work; see [Git replay](GitReplay.md).

The Git Merge view now inspects real blockers/parents/conflicts, opens staged Diff
review and previews either an unsigned merge commit or quit-tracking while
retaining local files/index. Native UI, four typed SDK operations and ABI2 host
exports share index/metadata/document guards, persistent intent receipts and
actual raw parent/tree/index postflight. Hooks are an explicit reviewed choice.
Windows compilation and schema checks pass; real Git/hook/receipt/concurrency,
SDK/plugin/GUI/POSIX acceptance remains pending. Signed/autostash/octopus
completion, abort/reset recovery and interactive rebase are still separate work;
see [Merge workflow](GitMergeWorkflow.md).

CyCodeReader is now an independent repository owning the shared editor source
module. It adds its own native/data extensions, language definitions, encoding
conversion and real CyToolsCore/MCP over open documents. Its project services,
folder search, macros and unsaved-document recovery remain separate work; see
https://github.com/MrMybal/CyCodeReader. CyCode builds only the IDE and consumes
the shared module at the revision in app/editor-source.lock.json.

Native desktop, CyUI themes/docking, UTF-8 editing, revision-safe saves, file lookup,
background cancellable text search, Git's local workflow, toolchain detection,
saved CMake profiles/targets and compilation database integration, C++ LSP
completion/diagnostics, definitions/references shared with MCP, native command/VCS
extension ABI and live MCP integration. Native multi-window documents/panels,
independent instances, solution inspection and UnrealBuildTool generation/build/index
are implemented, with UE 5.3 executed on Windows. Executable commands share Rider,
Visual Studio and VS Code keyboard profiles with editable bindings/chords. This
catalogue covers the current services, not all actions provided by those IDEs.
Actual server code actions now share issued sessions, lazy resolution and guarded
preview/apply/undo through the native core, GUI and MCP. Native acceptance covers
C++ typo fixes, TypeScript imports, Java import organization and Rust rewrites;
full desktop acceptance remains pending. See [Languages](Languages.md).
C++ symbol/type/member search, registered read-only SDK/engine source browsing,
multi-file rename previews/apply/undo and .clang-format formatting are implemented
through shared native services. Blueprint/asset-aware rename is not provided.

C# completion/diagnostics, definitions/references, types/members, semantic rename
and Roslyn formatting share these document/revision services through an independent
server. Actual .NET project references and unsaved buffers are exercised on Windows.
See [C#](CSharp.md); managed DAP debugging is implemented for the tested .NET 8
console workflow. Broader framework and language workflows remain pending.

Interactive PTYs and real DAP run/debug/attach services are implemented, with source
breakpoints, stack/locals/watches, stepping and program-terminal I/O exercised on
Windows. See [Execution](Execution.md) for the tested boundaries.
Git branches, remotes, fetch/fast-forward pull/push, stashes, history/blame and
text conflict inspection/resolution are implemented. See [Version control](VersionControl.md).

## Next: daily C/C++ use

Project creation now provides six built-in models: C/C++ CMake, Rust Cargo,
C# .sln/.NET, standalone Java and web, with exact previews and guarded exclusive
creation. Installed ABI2 plugins can add bounded declarative templates and typed
options. The bundled Unreal extension now adds a C++ game with module and
Game/Editor targets. See [Project templates](ProjectTemplates.md). Standalone
data-pack installation, binary/conditional templates, automatic SDK selection
and broader framework/test models remain pending.

General MSBuild solution/project build, clean/rebuild/restore, configuration mapping,
evaluated artifacts and C++ indexing are implemented. Windows C++ and SDK C# builds
are executed. See [MSBuild](MSBuild.md) for the supported scope.

- Richer command contexts, macOS keymaps and broader semantic search/index coverage;
  continue implementing the remaining Rider workflows as real services/actions.
  Actual LSP call/type hierarchy, lazy branch/site navigation and shared
  SDK/native-plugin sessions are implemented; runtime/GUI/backend acceptance
  remains pending. See [Hierarchies](Hierarchies.md).

- .slnx inspection and shared MSBuild/C# opening are implemented; new runtime
  acceptance remains pending. Broader project-system/toolset/custom-target coverage, multiple solution roots
  and advanced MSBuild compiler/analysis flags.
- Broader engine/SDK versions and roots, Blueprint/asset tools,
  Unreal-aware refactoring, Live Coding and editor/debugger integration. Normalize
  compiler-specific analysis flags further (UE 5.3/clangd 18 still reports an engine
  intrinsic compatibility diagnostic despite successful member completion/navigation).
- CMake configure/build presets now use native metadata and actual CMake execution;
  test/package/workflow previews and native execution now share that build/cancel
  service, with native/GUI acceptance pending; see [Preset actions](CMakePresetActions.md).
  Broader custom/cross toolchains,
  compiler wrappers, configuration variants
  and broader validation of GCC, modules/PCH/unity/framework workflows.
- Command-based/resource-edit refactorings and broader code-action coverage remain.
  Annotated text-edit previews and per-plan confirmations now share the native
  service, SDK and plugin host; native/GUI acceptance is pending. See
  [Annotated changes](ChangeAnnotations.md).
  Pull-diagnostic/unversioned provider coverage and framework-specific tests/coverage beyond the implemented
  configured CTest discovery/selection/cancellation;
  broader external-header navigation beyond registered SDK roots.
- Versioned diagnostics now have CodeEditor squiggles/tooltips with native revision
  guards and theme roles. New runtime/graphical acceptance remains pending; see
  [Editor diagnostics](EditorDiagnostics.md).
- Semantic full/delta transport, bounded server bases and one full fallback are
  implemented in the shared language worker; native/backend/GUI acceptance and
  performance measurements remain pending. See [semantic updates](SemanticTokenUpdates.md).
  Range/viewport requests, advanced modifier styles and broader raw-string/
  preprocessor fallback coverage remain.
- Windows recursive filesystem notifications, conflict-aware external-change reload
  and an in-memory paged path index are implemented. Native Linux/macOS watchers,
  persistent path/symbol indexes,
  full selection/horizontal-scroll/hidden-fold restoration, historical Undo recovery,
  large-file editing and broader Unicode benchmarks remain. Unsaved UTF-8 buffers
  now have per-instance crash checkpoints and guarded preview/restore with editor
  Undo/Redo; see [Recovery](Recovery.md).
  Workspace-relative tabs/cursors, document scroll anchors, wrap/visible folds,
  image camera/filter settings, recent files, panel visibility and docking
  layout now restore from bounded per-instance metadata with live-instance leases,
  source fingerprints and image-dimension guards;
  runtime/graphical acceptance remains pending. See [Workspace sessions](WorkspaceSessions.md).
  Engine-owned untitled drafts, first-save/Save as to new files, same-editor Undo,
  empty-draft recovery and explicit session-coordinate restoration are now implemented
  and Windows compiled. Native/GUI acceptance, overwrite review/external destinations,
  automatic draft-text restoration and full persisted Undo remain pending; see
  [Untitled documents](UntitledDocuments.md).
- Physical file/folder create/move/delete, guarded previews, inverse actions and
  persistent project trash are implemented through Engine/GUI/MCP/plugin services.
  Semantic file/resource refactoring, project/include/breakpoint rewriting, richer
  multi-selection/drag/drop/copy workflows, cross-volume moves and persisted file
  Undo/Redo remain. See [Resources](Resources.md).
- CMake/MSBuild/explicit Unreal work now shares native cancellation, operation IDs
  and a reservation retained until work stops, with GUI/MCP/plugin controls. New
  runtime regressions remain suspended after the antivirus interruption; see
  [Build cancellation](BuildCancellation.md). Semantic-request cancellation,
  and background-analysis streaming remain pending. Explicit native builds and
  project tasks now have separate bounded live tails with one shared implementation;
  see [Live build output](LiveBuildOutput.md) and [Project output](ProjectTaskOutput.md).
  Main build/generation deadlines are now
  configurable through desktop preferences and shared native/MCP/plugin requests.
- Broader terminal compatibility/IME/fonts, shell integration and split sessions;
  remote/core-dump workflows and further
  backend coverage. Capability-gated function/exception breakpoints, paged scopes,
  register scopes, read-only memory and disassembly are implemented and Windows
  compiled; their new runtime acceptance remains pending. See [Advanced debugging](AdvancedDebugging.md).
  Data breakpoint inspection/full-list replacement now uses actual capabilities,
  pause/session handles, revisions and verification through the shared UI/SDK/plugin
  owner. Native/hardware/GUI acceptance, range controls and persistent targets
  remain pending; see [Data breakpoints](DataBreakpoints.md).
  Paused variable/expression setters and memory writes now share the same native
  owner, with observed session/stop/frame guards, expected-byte preflight,
  read-back and explicit partial/unconfirmed outcomes. Native target, adapter,
  concurrent-client and graphical acceptance remain pending; see
  [Value and memory editing](DebugValueEditing.md).
  Granularity/single-thread options, reverse stepping/Continue and selected-frame
  restart now use capability/context-guarded native requests with actual event
  handling and noncancellable SDK outcomes. Real backend recording/restart,
  side effects, concurrent thread states and GUI acceptance remain pending; see
  [Execution controls](DebugExecution.md). Session-only instruction breakpoint
  replacement, conditions/disabled entries, actual verification/events and guarded
  disassembly drafts now share the native/SDK/plugin owner; real adapter/GUI
  acceptance and mode/symbol re-resolution remain pending. See
  [Instruction breakpoints](InstructionBreakpoints.md). Actual step-in target
  queries, issued pause/frame-bound choices and explicit targeted execution now
  share the native/SDK/plugin owner; real adapter/GUI acceptance remains pending.
  See [Targeted step-in](StepInTargets.md). Set next statement now shares that
  target implementation and the native/SDK/plugin owner, with actual source
  guards and pending relocated-stop handling; runtime acceptance is pending.
  See [Set next statement](GotoTargets.md). This skips code. Run to cursor now
  uses a verified temporary source-breakpoint lease, guarded Continue and original
  list restoration through the native/SDK/plugin owner. Document/disk/context and
  pending-reply guards are implemented; real adapter/cleanup/concurrency/GUI
  acceptance remains pending. See [Run to cursor](RunToCursor.md). Full non-stop
  per-thread inspection, virtual-source run-to-cursor and force-run behavior remain.

## Complete version control and plugins

- Multiple/nested Git roots, selected-repository hunks and linked-worktree
  catalogue/create/lock/unlock/clean removal are now implemented in the shared
  Engine/GUI/SDK. Windows compilation is checked; native/GUI acceptance remains
  pending. See [Git worktrees](GitWorktrees.md). Session-backed repository selection,
  reviewed linked-worktree move and intact-pointer repair after manual relocation
  are now implemented in native/UI/SDK/ABI2, with runtime acceptance pending.
  Cross-instance document coordination, arbitrary main/bare/custom-layout repair
  and worktree pruning remain. Configured top-level
  submodule inspection and guarded init/sync/recorded-commit update now share the
  native/SDK/plugin owner and a reviewed GUI, with actual Git/runtime/GUI acceptance
  pending. Separate local addition preview and reviewed clone/stage are implemented,
  with catalogue/HEAD/index/destination/metadata guards and pending runtime/GUI
  acceptance. Separate reviewed deinitialization now protects ignored/untracked
  files, normal index flags, native documents and retained parent-owned metadata;
  actual removal/runtime/GUI acceptance is pending. Existing-repository adoption,
  permanent removal and recursive bulk workflows remain. URL/tracking-branch
  editing now has reviewed native/SDK/plugin/UI operations, preserving metadata
  edits/index/checkout and reporting effective overrides; real Git/GUI acceptance
  remains pending. Local override editing and custom Git-directory layouts remain;
  see [Git submodules](GitSubmodules.md).
- Credential UI, interactive rebase, multi-commit/mainline replay, automatic rename topology,
  Git attribute/filter/EOL checkout semantics and arbitrary staging-line selection
  remain. Separate per-file raw stage-zero installation/absent-index removal now
  uses native/UI/SDK/ABI2 ownership, persistent trash and guarded Undo; actual
  filesystem/Git/GUI/POSIX acceptance is pending. See
  [Git working files](GitWorkingFiles.md). Structural
  conflict catalogue/inspection and reviewed index-only binary/missing-side/grouped
  rename-path choices now share native/UI/SDK/ABI2 ownership. Local files and
  buffers are retained; rename topology is explicit, symlink/gitlink conflicts
  remain external, and real Git/GUI acceptance is pending. See
  [Git conflicts](GitConflicts.md).
  Issued text hunks now support guarded staging/unstaging with the common Diff view.
- Project/root-specific VCS selection, provider-aware menus/custom actions, extension
  activation/dependencies, native ABI2 instances, panels, commands and declarative
  CyUI syntax extensions are implemented. The bundled Unreal adapter exercises
  project contributions against existing services.
- The bundled SVN provider now implements status/diff/history/blame and guarded
  add/revert/commit/update, with shared alternative-provider Review tabs. Real SVN
  acceptance remains pending; see [Subversion](Subversion.md).
- A bundled Perforce provider now covers opened-file status, Diff/history/annotations,
  checkout/add/reopen/revert/one-file submit/locks and guarded sync. Windows compiled;
  real P4 client/server validation, advanced changelist/shelf/stream/resolve work and
  GUI acceptance remain pending; see [Perforce](Perforce.md).
- A bundled Mercurial provider now covers status/review, exact-file add/forget/revert,
  commit, clean update, bookmarks/named branches and configured-alias pull/push.
  Windows compiled; real Hg, native/MCP/GUI acceptance, advanced merge/rewrite/
  shared/subrepository workflows and performance benchmarks remain pending; see
  [Mercurial](Mercurial.md).
- A bundled Diversion provider now covers status/review, commits/file reset,
  branch creation, clean checkout/update and locks with automatic tracking/sync.
  Windows compiled; real client/agent/text-format/native/MCP/GUI acceptance,
  shelves/tags/reviews/merge/conflict/submodule management and performance work
  remain pending; see [Diversion](Diversion.md). Future independent systems use
  the existing [generic provider contract](CustomVersionControl.md); no backend
  protocol or implementation is invented for the owner's planned system.
- Richer provider-specific diff/conflict/status contracts.
- Manifest OS/architecture/host-version checks and ABI2 activation/deactivation
  notifications are implemented. Enabled dormant dependents resume when their
  dependency is activated. Native regression execution for this change remains
  suspended after the antivirus interruption; see [Plugins](Plugins.md).
- Signed local install/update/remove/rollback and guarded unused-version cleanup
  now use the native package service, explicit publisher trust and immutable
  versions with host leases. Native/GUI acceptance remains pending; see
  [Plugin packages](PluginPackages.md). Network discovery/marketplace, automatic
  updates and process isolation for untrusted extensions remain.
- Plugin language-server registration now routes actual services through independent
  host-owned processes. Python/Pyright and GLSL Analyzer are integrated; HLSL has a
  configurable backend awaiting runtime validation. See [Development workflows](DevelopmentWorkflows.md).
- Native workspace/document/resource/build events and automatic ABI2 subscriptions
  are implemented, with bounded replay, snapshot handshakes, manifest permissions
  and failure suspension. Windows compiles; new runtime acceptance is pending.
  See [Workspace events](Events.md).
- ABI2 stdio-DAP registration now uses the common native debugger, explicit profile
  selection, permission-checked host/MCP requests and revocation. The Unreal plugin
  contributes CodeLLDB. Windows compilation/source checks passed; native runtime
  acceptance remains pending. See [Plugin debuggers](PluginDebuggers.md).
- Built-in Java launch and JDWP attach now use JDT LS's Microsoft java-debug add-on,
  a bounded loopback DAP connection, actual task-generated launch profiles and
  separate persistent local/remote JVM endpoints. Runtime/network/GUI acceptance
  is pending. Manual HCR now performs a guarded incremental JDT build/redefine,
  with explicit result/cancellation handling; automatic synchronization of external
  build outputs and further Java-specific workflows remain. See
  [Java debugging](JavaDebugging.md).
- Additional real language/debug engines, generic TCP/child-session integrations and
  specialized GPU editor APIs; extraction of remaining built-in project UI into extensions.
- Generic panel properties have commit/revision contracts; arbitrary extension-owned
  data still needs its own Undo/history implementation. There is no automatic global
  Undo for every plugin operation.

## More languages and editor views

- Per-workspace language-server launch/settings and activation preferences are
  implemented in the native core, GUI, MCP and plugin host. Defaults remain
  unresolved until launch; instances use revision and file locks. New runtime
  validation remains suspended; see [Language preferences](LanguagePreferences.md).

- C# broader managed debugging (frameworks, exceptions, async and web workflows),
  broader Roslyn code actions beyond the shared edit-only transport,
  further snippet variable families, interactive field transforms and OS-account-wide catalogue storage,
  metadata/generated source views,
  Razor, rich documentation rendering/mouse hover and broader SDK/framework coverage beyond the implemented
  Roslyn language services and MSBuild builds. See [C#](CSharp.md).
- Cargo workspace/member/features/custom-profile and bin/example/lib/test/bench
  controls now share native build/test planning, metadata/cache and package/kind
  executable guards. Windows compiled; expanded Cargo/GUI acceptance remains
  pending. Named installed Rustup selection now shares Cargo and analysis launch
  planning; Windows compiled, actual runtime/GUI acceptance pending. Installed
  toolchain listing/management, arbitrary paths/cross-target runners, advanced Rust debugging and framework
  test discovery remain; see [Cargo workspaces](CargoBuilds.md). Explicit Cargo
  manifest/features/offline/locked analysis snapshots now use local preferences
  and the shared Rust server owner; Windows compiled, actual backend/GUI acceptance
  pending. Exact package/target/custom-profile analysis parity and automatic
  adoption of task changes remain outside this mapping; see [Rust analysis](RustAnalysis.md)
  and [Rust toolchains](RustToolchains.md).
  Maven/Gradle build/test/run now use real tools and runtime
  dependency models; Windows compiled, actual JVM runtime acceptance pending.
  Maven reactor/import parity, Gradle Tooling API, structured framework tests,
  advanced Java debugging and JPMS/Android remain. The JDT LS launch/attach path is
  implemented with runtime/network acceptance pending; see [Java debugging](JavaDebugging.md)
  and [JVM builds](JvmBuilds.md).
  Analysis defaults disable Rust build scripts/procedural macros/check-on-save and
  automatic Java Maven/Gradle imports.
- Web framework/code-action coverage, advanced snippets and browser/debugger
  integration beyond the connected JS/TS/HTML/CSS servers and npm task/dev-server
  terminal execution.
- HLSL/GLSL compilation now uses typed DXC/glslang requests, guarded output and
  real-tool diagnostics through shared ProjectTasks; Windows compiled, tool/native/
  GUI acceptance pending. GPU preview, reflection/resources/materials, renderer
  permutations and Unreal shader environments remain. See [Shader compilation](ShaderBuilds.md).
- Mixed-project workspaces and broader per-project activation beyond the implemented
  language-server preferences.

## Public distribution

- Project license selected: GPL-3.0-only. Provide recipients access to the exact
  corresponding source, including pinned CyUI/shared-editor dependencies and build
  scripts, alongside binary distributions. Dependency licenses remain applicable.
- Run actual Windows/Linux/macOS build and runtime matrices, including Unicode/IME,
  filesystem differences, DPI and portable icon/font coverage.
- Windows portable release-based updates and ordinary-failure rollback are implemented; Windows per-user Setup and edition-aware application updates are implemented and packaging CI passes; actual installer/update acceptance remains pending. Unattended/legacy recovery, automatic Linux/macOS updates, clean-machine validation, signing and contribution docs remain.
- Reproducible end-user packages with complete third-party notices.

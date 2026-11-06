# CyCode

<p align="center">
  <img src="icons/CyCode.png" alt="CyCode" width="160" height="160">
</p>

[Documentation](https://mrmybal.github.io/CyCode-docs/#/en/overview) · [Documentation en français](https://mrmybal.github.io/CyCode-docs/#/fr/overview)

CyCode includes stable GitHub release checks, explicit Windows portable updates,
resume/fresh downloads, SHA-256 verification and categorized EN/FR feedback.
Download/staging space checks precede transfer, full fallback and preparation;
separate byte/hash/staging phases and sampled rate/transfer-time estimates avoid
confusing a complete transfer with an installable update. Interrupted files,
history and recovery backups are retained, with reviewed reversible cache moves.
Installation uses the ordinary save/close flow and all-instance ownership guards.
Allocation-aware installation/recovery space checks complement verified rollback
outcomes and guarded pending-marker removal. Recovery prepares and verifies every
needed original copy before moving installed targets. Retries reuse verified
prepared originals from the active transaction and retain changed earlier copies.
The staged helper shows phase/count progress, accepts cooperative stop requests and
offers a copied result report while retaining plan-bound receipts when possible. See
[Installation checks](app/Docs/UpdateInstallation.md).
An explicit [detailed history](app/Docs/UpdateHistory.md) inspection adds metadata-only
installation/recovery results, shared text/category filters, dates, pages and copied
selected reports, with visible scan limits and no payload verification claim.
Actual HTTP/helper/filesystem/GUI and Linux/macOS acceptance remains pending.
See [Application updates](app/Docs/Updates.md), [Download controls](app/Docs/UpdateDownloads.md),
[Recovery](app/Docs/UpdateRecovery.md) and [Storage](app/Docs/UpdateStorage.md).

Three connected occurrence milestones add actual document-highlight requests,
read/write/text outlines independent of diagnostics, and a dockable inspector with
kind filters, cyclic navigation and atomic multi-selection. GUI, SDK/MCP and ABI2
share the same guarded native owner (227 SDK operations). Existing search and
lexical occurrence shortcuts remain; Rider adds Ctrl+Shift+F7 for explicit
highlighting. Windows production/source/schema checks are recorded separately
from pending server/GUI/platform acceptance. See [Document occurrences](app/Docs/Occurrences.md).

The preceding source-navigation milestone added declaration, type definition and implementation to the
existing definition/reference commands. A shared bounded native catalogue validates
Location/LocationLink ranges and source ownership; the result dock provides cached
filtering, keyboard selection, copy and guarded refresh/activation. GUI/MCP/ABI2
share the same service (226 SDK operations at that milestone), without a new dependency or CyUI
module. Real server/GUI/platform acceptance remains pending; see
[Source navigation](app/Docs/SourceNavigation.md).

Three connected diagnostics milestones strengthen document lifetime/version
checks, add severity filtering and selected-message details in Problems, and link
compiler output rows to guarded workspace source locations. The native parser is
shared with SDK/MCP build_location and ABI2 plugins (222 operations). No new UI
module, dependency or executable target is added. Runtime acceptance remains
pending; see [Diagnostics](app/Docs/DiagnosticNavigation.md) and
[Compiler locations](app/Docs/CompilerLocations.md).

CyCode is a native C++20 IDE built with CyUI, Dear ImGui and SDL3. Development
focuses on C/C++ and Unreal Engine projects, with C#/.NET services and a shared
CyToolsCore MCP integration.

Project tasks now have live output/status shared by GUI/MCP/plugins, readable
Cargo compiler messages and captured cancellation IDs. Problems gains filtered
keyboard navigation through current-file or reported workspace locations. Actual
runtime acceptance remains pending; see [Project output](app/Docs/ProjectTaskOutput.md)
and [Diagnostics](app/Docs/DiagnosticNavigation.md).

The Build panel now follows explicit CMake/MSBuild/Unreal output during execution,
with a bounded live tail shared by MCP and plugins. Runtime acceptance remains
pending; see [Live build output](app/Docs/LiveBuildOutput.md).

Unreal Editor output inspection now checks receipts, module build IDs and recorded
project binaries before bound Run/Debug, with bounded reports and virtualized lists.
Actual runtime acceptance remains pending; see [Unreal outputs](app/Docs/UnrealArtifacts.md).

Unreal launch profiles now retain their build context and restore their options.
Compilation before debugging uses the bound target/configuration and blocks
automatic launch after concurrent profile/document changes or uncertain builds.
Native configure/run/launch validate the binding; explicit detachment preserves
manual profiles. Actual runtime/GUI acceptance remains pending.



Unreal editor, standalone game and uncooked-server launch preparation now shares
the native Debugger profile, with typed map/network/window controls and guarded
saving. Actual UE/GUI acceptance remains pending. See
[Unreal launch profiles](app/Docs/UnrealLaunch.md).

Unreal cooking/packaging now has reviewed game/map/container/deadline controls,
separate staging/archive/logs and shared native/UI/SDK/plugin cancellation.
Intents and actual results persist after errors; actual UE/GUI acceptance remains
pending. See [Unreal automation](app/Docs/UnrealAutomation.md).

History actions now open a reviewed single-commit cherry-pick/revert workflow,
including conflict continuation and quit tracking with retained local work/index.
UI, SDK/MCP and ABI2 plugins share native guards, persistent intents and actual
postflight. Real Git/GUI acceptance remains pending; see
[Git replay](app/Docs/GitReplay.md).

Git merges now have a dedicated view for blockers, conflicts and staged Diff
review, followed by a guarded unsigned commit or quit-tracking that retains
local files/index. UI, SDK/MCP and ABI2 plugins share the same native owner and
persistent intent receipts. Windows compilation/schema checks pass; real Git/
GUI acceptance remains pending. See [Merge workflow](app/Docs/GitMergeWorkflow.md).

Working-file synchronization now follows Git staging with explicit raw-byte
installation/removal, persistent backups and guarded Undo, shared by UI, SDK/MCP
and ABI2 plugins. The index stays intact; actual filesystem/GUI acceptance remains
pending. See [Git working files](app/Docs/GitWorkingFiles.md).

Git conflict workflows now include actual stage catalogues, binary-safe inspection,
reviewed missing-side/index deletion and explicit grouped rename-path choices.
Native UI, SDK/MCP and ABI2 plugins share one guarded service; working files and
buffers stay intact. Real Git/GUI acceptance remains pending. See
[Git conflicts](app/Docs/GitConflicts.md).

Instruction breakpoints now connect Disassembly, reviewed replacement drafts and
the shared SDK/plugin service. They preserve actual verification and uncertain
failures without persisting process addresses. Real-adapter/GUI acceptance remains
pending; see [Instruction breakpoints](app/Docs/InstructionBreakpoints.md).

Debugger execution controls now include capability-gated line/instruction
stepping, selected-thread execution, reverse actions and frame restart through the
shared UI/SDK/plugin service. Actual backend/GUI acceptance remains pending; see
[Execution controls](app/Docs/DebugExecution.md).

Paused debugger values and memory now have explicit desktop/SDK/plugin editing
through the shared native owner, including stale-context guards and memory
read-back. Real-adapter/GUI acceptance remains pending; see
[Value and memory editing](app/Docs/DebugValueEditing.md).

Java/Maven/Gradle main targets now connect to the shared debugger through JDT LS
and a separately installed Microsoft java-debug add-on. Windows compilation and
schema checks are distinct from pending Java/GUI acceptance. See
[Java debugging](app/Docs/JavaDebugging.md).

JDWP attach now uses explicit local/remote JVM host/port settings and the same
debugger views. Saving an endpoint does not connect; Stop detaches existing JVMs.
Runtime/network/GUI acceptance remains pending in the native-test suspension.

Annotated language-server text edits now retain labels, file groups and required
per-plan confirmations through the shared preview/apply/undo and SDK/plugin path.
See [Annotated changes](app/Docs/ChangeAnnotations.md) for scope and pending acceptance.

Workspace sessions now retain editor wrap/visible folds and image framing/filter
settings through the existing shared views, with source and dimension guards.
See [Workspace sessions](app/Docs/WorkspaceSessions.md) for limits and pending acceptance.

Semantic coloring now negotiates full/delta language-server responses with bounded
native caches and full-request fallback, shared by the editor and SDK/plugins.
See [Semantic updates](app/Docs/SemanticTokenUpdates.md) for scope and pending acceptance.

CMake test/package/workflow presets now share native preview, execution and
cancellation with the desktop, SDK and plugin host. See
[Preset actions](app/Docs/CMakePresetActions.md) for controls and pending acceptance.

Call/type hierarchy adds capability-gated callers/callees, base/derived types
and lazy call-site navigation through the shared native service, SDK and plugin
host. See [Hierarchies](app/Docs/Hierarchies.md) for controls and pending acceptance.

Version control now selects nested Git repositories and manages linked worktrees
through the common native service and SDK. See [Git worktrees](app/Docs/GitWorktrees.md)
for supported operations and the pending native/GUI acceptance.

File > New text document creates an in-memory draft; Save as persists it to a new
workspace file with the same editor Undo/window identity. Empty drafts also have
guarded Recovery checkpoints. See [untitled documents](app/Docs/UntitledDocuments.md)
for the shared MCP/plugin APIs, limits and pending runtime acceptance.

[CyCodeReader](https://github.com/MrMybal/CyCodeReader) is the independent companion code/text editor. It
shares CyCode's editor and coloration, with file tabs, detached native windows,
find/replace and guarded saves, without loading IDE project services.
Its repository owns the shared `editor/` library; CyCode consumes a pinned source
revision from app/editor-source.lock.json. Select a development checkout with
--editor-root or CYCODE_EDITOR_ROOT. The CyCode build now produces only the IDE.

The current implementation includes:

- [Signed local plugin packages](app/Docs/PluginPackages.md): desktop publisher
  trust, install/update/remove, retained-version rollback and guarded cleanup,
  with six shared SDK operations. Versions stay pinned for the process lifetime;
  workspace switching does not replace loaded code. Native/GUI package acceptance
  remains unexecuted; marketplace and untrusted-code isolation remain separate work.

- [HLSL/GLSL compilation](app/Docs/ShaderBuilds.md) through installed DXC/glslang,
  typed task options, bounded DXIL/SPIR-V outputs and Problems diagnostics.
  Windows compiled; real compiler/GUI acceptance remains pending. GPU preview
  and Unreal-specific shader environments are separate work.

- Per-instance [workspace sessions](app/Docs/WorkspaceSessions.md) for relative
  tabs/cursors, recent files, panel visibility and native docking layout, with
  separate guarded dirty-text recovery.
- [Advanced DAP inspection](app/Docs/AdvancedDebugging.md): function/exception
  breakpoints, paged scopes/registers, read-only memory and disassembly through
  the same desktop/MCP/plugin owner.
- [Maven/Gradle build/test/run](app/Docs/JvmBuilds.md) using actual CLI tools and
  runtime dependency models. These three additions are Windows-compiled; their
  new native, real-tool and graphical acceptance remains pending.

- A bundled [Diversion provider](app/Docs/Diversion.md) for status/review, commits,
  guarded clean checkout/update, branch creation and file locks. It follows
  Diversion's automatic tracking/sync model, with no invented Git staging or
  push/pull. Windows compiled; real client/agent/GUI acceptance remains pending.
  The same [generic provider SDK](app/Docs/CustomVersionControl.md) can host a future
  custom revision-control system without changing the IDE's provider routing.

- A bundled [Mercurial provider](app/Docs/Mercurial.md) with shared status/review,
  exact-file add/forget/revert, commits, clean updates, bookmarks/named branches
  and configured-alias pull/push. An installed Hg client is required; Windows
  compilation passed, real-client and graphical acceptance remain pending.

- A bundled SVN provider plugin with project/root mappings, adaptive menus and
  shared Diff/Review, alongside built-in Git. A separate SVN client is required;
  Windows compilation passed, runtime acceptance is pending. See
  [Subversion](app/Docs/Subversion.md).

- A bundled [Perforce provider](app/Docs/Perforce.md) for opened-file review,
  checkout/changelists, guarded one-file submission/revert, locks and project sync.
  It uses an installed P4 CLI; real server/GUI acceptance remains pending.

- Plugin stdio-DAP adapters with explicit profile selection, shared native
  transport/process ownership and disable/revoke handling. The Unreal extension
  contributes CodeLLDB. See [plugin debuggers](app/Docs/PluginDebuggers.md) for scope
  and the currently unexecuted native acceptance checks.

- Automatic native plugin subscriptions for workspace/document/resource/build
  events, plus shared read-only MCP paging. See [workspace events](app/Docs/Events.md).
- Version-checked CodeEditor diagnostic squiggles and hover messages, with UTF-16
  mapping and theme roles. See [editor diagnostics](app/Docs/EditorDiagnostics.md).
  New runtime checks for these additions remain suspended after the antivirus interruption.

- Native `.slnx` solution inspection/opening and CMake configure/build presets,
  shared by the desktop, MCP and plugins. See
  [project formats](app/Docs/ProjectFormats.md) for scope and pending runtime checks.

- Per-project language-server settings and activation choices, with local persistence,
  defaults/reload/forget controls and shared MCP/plugin APIs. See
  [language preferences](app/Docs/LanguagePreferences.md); new runtime acceptance
  remains pending during the antivirus interruption.

- Configured CTest discovery/selected execution, per-test results and cancellation;
  issued Git hunk staging/unstaging and regex search in unsaved buffers.
- ABI2 plugin language-server contributions and actual Python/Pyright and GLSL
  analysis. See [development workflows](app/Docs/DevelopmentWorkflows.md) for setup,
  commands, SDK/MCP contracts and the remaining language/editor/plugin scope.

- Direct Visual Studio solution and Unreal project opening, CMake/MSBuild/Unreal
  build adapters, and project compilation settings for clangd.
- C++/C# completion, diagnostics, navigation, semantic colors, rename previews
  and formatting through clangd and Roslyn.
- Shared Rust/Java/JS/TS/HTML/CSS syntax, JSX/TSX and embedded HTML regions;
  independent language servers with [documented capabilities](app/Docs/Languages.md).
- Native symbol documentation and callable signatures through those services,
  with read-only MCP operations and a compiled Code information panel; current
  GUI acceptance remains pending as recorded in validation.
- Actual language-server quick fixes and context refactorings, including lazy
  resolution and guarded preview/apply/undo through the shared native core/MCP;
  Alt+Enter/Ctrl+. UI compiles, with desktop acceptance pending.
- [Cargo, npm and standalone Java tasks](app/Docs/ProjectTasks.md): real compilation,
  tests where supported, terminal execution and Cargo binary debugging.
- [Cargo workspaces](app/Docs/CargoBuilds.md): workspace/features/custom-profile and
  bin/example/lib/test/bench controls with native executable guards; Windows compiled,
  expanded runtime/GUI acceptance pending.
- [Rust analysis](app/Docs/RustAnalysis.md): explicit Cargo manifest/features/
  offline/locked snapshot, local preference save/removal and preserved raw settings;
  actual server/GUI acceptance pending.
- [Rust toolchains](app/Docs/RustToolchains.md): named installed Rustup selection
  for Cargo and analysis, without requesting installation; Windows compiled,
  actual toolchain/component/GUI acceptance pending.
- Project search with ripgrep, document search, file/symbol lookup, and configurable
  Rider, Visual Studio and VS Code keyboard profiles.
- Native detached windows, independent application instances, themes and EN/FR UI.
- Git workflows, interactive terminals, C++/.NET debugging, and native plugin APIs.
- Shared live documents and native services for the GUI and MCP clients.

This is an active development milestone. It does not yet cover all Rider features.
Windows x64 has been tested locally; Linux/macOS execution remains unverified.
CyCode is licensed under [GNU GPL v3 only](LICENSE) (`GPL-3.0-only`).
Copyright (c) 2026 Cyberalien; see [COPYRIGHT](COPYRIGHT) and
[third-party notices](app/THIRD_PARTY_NOTICES.md).

## Version 0.2.0

See the [release notes](CHANGELOG.md) and [GitHub releases](https://github.com/MrMybal/CyCode/releases).
The Windows x64 archive includes the IDE, CLI and ripgrep; extract the complete ZIP
to a writable folder. Visual C++ 2022 x64 runtime and OpenGL 3.3 are required.
Language servers, compilers, debugger adapters and the optional private MCP runtime
are installed separately. Clean-machine installation and Linux/macOS binaries are
not validated for this milestone.

The official [CyUI repository](https://github.com/MrMybal/CyUI) is available on GitHub.
CyUI is MIT-licensed: [license at the pinned revision](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE)
or [included local copy](app/licenses/CyUI.txt). This license applies to CyUI;
CyCode's original code is covered by the GPL v3 notice above.

## Build

Use Python 3.11+, CMake 3.24+ and a C++20 toolchain. Windows builds use the Visual
Studio C++ workload. The GUI resolves the exact CyUI revision in `app/cyui-source.lock.json`,
or an explicit `--cyui` path. The optional MCP runtime also requires the actual
CyToolsCore wheel.

```sh
python app/scripts/fetch_dependencies.py
python app/scripts/build.py
```

For the native core without GUI dependencies:

```sh
python app/scripts/fetch_dependencies.py --core-only
python app/scripts/build.py --core-only
```

Build scripts fetch the pinned dependencies and create local build/runtime files.
Ordinary builds exclude native test executables and do not run tests.
Use `python app/scripts/verify_sources.py` for source-only checks; native suites
are explicit opt-ins described in [Testing](app/Docs/Testing.md).
See the [developer guide](app/README.md) for platform prerequisites, MCP setup,
run arguments and tests.

## Documentation

- [Guide en français](LISEZ-MOI.md)
- [Implemented capabilities](app/Docs/CapabilityCoverage.md)
- [Project tasks](app/Docs/ProjectTasks.md)
- [Architecture](app/Docs/Architecture.md)
- [Validation and current limits](app/Docs/Validation.md)
- [Roadmap](app/Docs/Roadmap.md)
- [Third-party notices](app/THIRD_PARTY_NOTICES.md)

CyCode now selects explicit CyUI modules for editing, diffs, commands, diagnostics,
Markdown documentation and settings. See [the migration notes](app/Docs/CyUI-Migration.md)
for exact dependency pins, build instructions and retained IDE adapters.

Image files open in read-only tabs through CyUI Images/Canvas, with background
decoding, transparency, zoom/pan and reload. See [image previews](app/Docs/Images.md)
for supported formats and remaining scope.


Local illustrated English/French help is generated and bundled with GUI builds.
See [CyDoc documentation and build instructions](app/Docs/Documentation.md).

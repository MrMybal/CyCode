# CyCode

Navigate > Open included file follows supported literal C/C++ directives in the
live buffer, including inline comments and immediate backslash-newline
continuations. Find related C/C++ files... always shows the counterpart list.
Its shared actions provide Retry, explicit index refresh/retry, path/report
copying and contextual local help (default Shift+F1). Ordinary queries remain
scan-free; paged file results guard both revision and generation.
See [Included files](Docs/CppIncludes.md) and [Related results](Docs/CppCounterparts.md#inspect-related-results).

Navigate > Switch header/source adds bounded local C/C++ companion discovery,
including Unreal Public/Private and same-stem paths from the current workspace
index, with Rider Alt+O and Visual Studio Ctrl+K, Ctrl+O.
One complete result opens directly; multiple/partial matches use the shared palette
with the cache status. Navigation does not start or wait for an index refresh.
The same read-only metadata service is available through SDK/MCP and ABI2.

Personal insertion snippets are shared by projects in this portable installation.
Use Edit > Edit user snippets... for the JSON draft, Undo/Redo and explicit guarded
Save/Reload. The insertion palette merges project, personal and enabled-plugin
definitions. See [Personal snippets](Docs/UserSnippets.md).

Edit > Create user snippet from selection... copies selected source literally into
an unsaved personal JSON draft. Review its name/prefix/scope and Save explicitly;
the original source is retained. The same read-only capture is exposed through SDK/MCP.

The development build also accepts declarative snippet catalogues from enabled
ABI2 plugins, with source labels and activation/revision guards. The bundled Unreal
provider supplies UE_LOG, Blueprint declarations and WITH_EDITOR selection wrapping.
See [Project snippets](Docs/SnippetCatalogue.md) for scope and limits.

[Documentation](https://mrmybal.github.io/CyCodeReader/cycode/Documentation.html#/en/overview) · [Documentation en français](https://mrmybal.github.io/CyCodeReader/cycode/Documentation.html#/fr/overview)

Completion snippets support document/workspace variables and bounded native regex
and Unicode case transforms, with the original linked-field/Undo ownership.
The standard dependency fetch includes pinned static PCRE2/utf8proc sources;
see [Build](Docs/Build.md) and [Completion variables](Docs/CompletionVariables.md)
for syntax, budgets and remaining runtime validation.

[Project insertion snippets](Docs/SnippetCatalogue.md) add saved JSON definitions,
scopes and multiple prefixes through Edit / Find Action, including without an LSP.
Rider uses Ctrl+J and Visual Studio uses Ctrl+K, Ctrl+X. Local proposals reuse the
same guarded native completion/Undo path, also available through SDK/MCP and ABI2.
Surround with snippet (Rider Ctrl+Alt+J) captures one selection for TM_SELECTED_TEXT,
including multiple lines. It preserves literal layout and refuses bodies that
would discard the selection. Native/GUI acceptance remains pending.

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
[Installation checks](Docs/UpdateInstallation.md).
An explicit [detailed history](Docs/UpdateHistory.md) inspection adds metadata-only
installation/recovery results, shared text/category filters, dates, pages and copied
selected reports, with visible scan limits and no payload verification claim.
Actual HTTP/helper/filesystem/GUI and Linux/macOS acceptance remains pending.
See [Application updates](Docs/Updates.md), [Download controls](Docs/UpdateDownloads.md),
[Recovery](Docs/UpdateRecovery.md) and [Storage](Docs/UpdateStorage.md).

Three connected occurrence milestones add actual document-highlight requests,
read/write/text outlines independent of diagnostics, and a dockable inspector with
kind filters, cyclic navigation and atomic multi-selection. GUI, SDK/MCP and ABI2
share the same guarded native owner (232 SDK operations). Existing search and
lexical occurrence shortcuts remain; Rider adds Ctrl+Shift+F7 for explicit
highlighting. Windows production/source/schema checks are recorded separately
from pending server/GUI/platform acceptance. See [Document occurrences](Docs/Occurrences.md).

The preceding source-navigation milestone added declaration, type definition and implementation to the
existing definition/reference commands. A shared bounded native catalogue validates
Location/LocationLink ranges and source ownership; the result dock provides cached
filtering, keyboard selection, copy and guarded refresh/activation. GUI/MCP/ABI2
share the same service (226 SDK operations at that milestone), without a new dependency or CyUI
module. Real server/GUI/platform acceptance remains pending; see
[Source navigation](Docs/SourceNavigation.md).

Three connected diagnostics milestones strengthen document lifetime/version
checks, add severity filtering and selected-message details in Problems, and link
compiler output rows to guarded workspace source locations. The native parser is
shared with SDK/MCP build_location and ABI2 plugins (222 operations). No new UI
module, dependency or executable target is added. Runtime acceptance remains
pending; see [Diagnostics](Docs/DiagnosticNavigation.md) and
[Compiler locations](Docs/CompilerLocations.md).

Native C++20 IDE foundation using CyUI and Dear ImGui, with SDL3 windowing/rendering,
a syntax-aware text editor, cancellable ripgrep search, Git, native plugins, clangd
completion/diagnostics, definition/reference navigation and a CyToolsCore MCP
companion. Project license: [GNU GPL v3 only](../LICENSE), SPDX `GPL-3.0-only`.
Copyright (c) 2026 Cyberalien; see [COPYRIGHT](../COPYRIGHT).

Project tasks now have live output/status shared by GUI/MCP/plugins, readable
Cargo compiler messages and captured cancellation IDs. Problems gains filtered
keyboard navigation through current-file or reported workspace locations. Actual
runtime acceptance remains pending; see [Project output](Docs/ProjectTaskOutput.md)
and [Diagnostics](Docs/DiagnosticNavigation.md).

The Build panel now follows explicit CMake/MSBuild/Unreal output during execution,
with a bounded live tail shared by MCP and plugins. Runtime acceptance remains
pending; see [Live build output](Docs/LiveBuildOutput.md).

Unreal Editor output inspection now checks receipts, module build IDs and recorded
project binaries before bound Run/Debug, with bounded reports and virtualized lists.
Actual runtime acceptance remains pending; see [Unreal outputs](Docs/UnrealArtifacts.md).

Unreal launch profiles now retain their build context and restore their options.
Compilation before debugging uses the bound target/configuration and blocks
automatic launch after concurrent profile/document changes or uncertain builds.
Native configure/run/launch validate the binding; explicit detachment preserves
manual profiles. Actual runtime/GUI acceptance remains pending.



The Unreal panel prepares editor, standalone game and uncooked-server profiles,
including typed map/listen/port/resolution/log options and DebugGame. Reviewed
settings save to the shared Debugger profile with a concurrent-change guard;
Run/Debug retain native terminal/DAP ownership. SDK/ABI2/provider 1.5.0 share the
same service. Actual UE/GUI acceptance remains pending; see
[Unreal launch profiles](Docs/UnrealLaunch.md).

Unreal cook/package review now provides typed maps/container controls, separate
staging/archive/logs, retained intents/results and common build cancellation,
shared with SDK/ABI2 and the bundled Unreal provider. Actual UE/GUI acceptance
remains pending; see [Unreal automation](Docs/UnrealAutomation.md).

The Replay view now reviews one cherry-pick/revert, continues resolved conflicts
or quits tracking while retaining local work/index. History context actions,
native guards, persistent intent and actual postflight are shared by SDK/ABI2.
Actual Git/GUI acceptance remains pending; see [Git replay](Docs/GitReplay.md).

An explicit Merge view now inspects actual conflicts/staged paths and reviews
completion into an unsigned merge commit, or quitting merge tracking while
retaining working files/index. Repository/state/message/hooks guards, persistent
intent receipts and parent/tree/index postflight are shared with SDK/ABI2. Real
Git/GUI acceptance remains pending; see [Merge workflow](Docs/GitMergeWorkflow.md).

Git working-file synchronization now previews and installs exact staged bytes or
removes an absent-index local file, preserving the original in persistent project
trash. Separate guarded Undo retains displaced bytes, and common resource events
refresh affected image views. GUI/SDK/ABI2 share one owner; documents must be
closed and real runtime/GUI acceptance remains pending. See
[Git working files](Docs/GitWorkingFiles.md).

Git conflicts now have a binary-safe stage catalogue, absent-side/deletion previews
and explicit grouped index resolution for rename paths. GUI, SDK/MCP and ABI2
plugins share repository/index/preview guards and actual postflight checks. This
stages reviewed versions while preserving working files and live buffers; it does
not check out or physically rename/delete files. Real Git/GUI acceptance remains
pending; see [Git conflicts](Docs/GitConflicts.md).

Git repository selection now restores through desktop sessions with an identity
check. Linked worktrees have separate reviewed move and manually-moved link repair
workflows, shared by the native UI, SDK/MCP and ABI2 plugins. Local files/index are
retained; repair audits the entire linked set because Git can update other linking
files. Real Git/session/GUI acceptance remains pending. See
[Git worktrees](Docs/GitWorktrees.md).

Git submodules now have a selected-repository catalogue and explicit init/sync/
recorded-commit update, plus a separate preview/clone-and-stage addition, through
the common native, SDK/MCP and plugin service. A distinct reviewed deinitialization
preview/action now removes a clean checkout while preserving the gitlink,
.gitmodules and absorbed Git repository. Ignored/untracked files, hidden index
flags, nested gitlinks and open native documents block that removal.
Reviewed URL/tracking-branch editing now shares this service, preserving existing
metadata edits/index state, reporting local overrides and hiding unsafe old values.
Reviewed revisions, clean/open-document guards and optional clone/fetch protect
the workflow; real Git/GUI acceptance remains pending. See
[Git submodules](Docs/GitSubmodules.md).

Run to cursor now arms a verified source breakpoint in a saved editor document,
resumes once and restores the original breakpoint list at the next actual stop.
It shares native, SDK/MCP and plugin ownership, with revisioned recovery for
uncertain replies. Rider uses Alt+F9 and Visual Studio Ctrl+F10. Real adapter/GUI
acceptance remains pending. See [Run to cursor](Docs/RunToCursor.md).

Set next statement now queries actual destinations in the selected frame source
and applies an explicit adapter-supported move. It shares the native target
catalogue implementation with step-in and waits for the actual relocated stop;
this skips intervening code and does not run to the cursor. Real adapter/GUI
acceptance is pending. See [Set next statement](Docs/GotoTargets.md).

Targeted step-in now reads actual adapter targets for the selected paused frame,
lets the user choose a function and executes through the common DAP/SDK/plugin
owner. Issued handles expire with the captured context or after submission; real
adapter/GUI acceptance remains pending. See [Targeted step-in](Docs/StepInTargets.md).

Instruction breakpoints now have a shared native/SDK/plugin service and a
disassembly-to-draft workflow, with conditions, disabled entries, revision guards
and actual verification/uncertainty. Addresses remain session-only; real adapter/
GUI acceptance is pending. See [Instruction breakpoints](Docs/InstructionBreakpoints.md).

Debugger Execution controls now share optional line/instruction stepping,
single-thread execution, reverse actions and selected frame restart with the
Run menu, SDK and plugins. Availability depends on actual adapter capabilities;
backend/GUI acceptance remains pending. See [Execution controls](Docs/DebugExecution.md).

Paused variable/expression editing and explicit memory writes now use the shared
DAP owner, with captured-context guards, expected-byte checks and actual
partial/unconfirmed outcomes. See [Value and memory editing](Docs/DebugValueEditing.md)
for the UI/SDK/plugin contracts and pending adapter/GUI acceptance.

Manual data breakpoints now use advertised adapter capabilities, paused target
inspection, read/write modes and guarded full-list replacement. UI, SDK and plugins
share actual verification and uncertain failure state. See
[Data breakpoints](Docs/DataBreakpoints.md) for limits and pending acceptance.

Java/Maven/Gradle debug targets now prepare a shared DAP launch through JDT LS
with a separately installed Microsoft java-debug add-on. See
[Java debugging](Docs/JavaDebugging.md) for setup and pending runtime acceptance.

Java also supports explicit JDWP attach to an existing local/remote JVM, with
separate saved endpoint fields and shared inspection. Stop requests detach without
terminating that JVM; actual network/runtime/GUI acceptance remains pending.

Manual Java Hot Code Replace now compiles the imported JDT workspace before
requesting class redefinition, with session/document guards, matched cancellation
and explicit unchanged/error results. Matching JDT output and VM support are
required; actual JVM/GUI acceptance remains pending. See [Java debugging](Docs/JavaDebugging.md).

Cargo tasks and Rust analysis can select a named installed Rustup toolchain
without requesting installation. Existing native process owners and local
preferences retain launch/cancellation/activation behavior. See
[Rust toolchains](Docs/RustToolchains.md) for setup and pending runtime acceptance.

Cargo task options can now be applied explicitly to Rust analysis, with a saved
local snapshot or session-only configuration. The native server owner keeps raw
settings separate and supports removing the override. See
[Rust analysis](Docs/RustAnalysis.md) for mapping and pending runtime acceptance.

Language-server text-edit annotations now use the shared native preview/apply/undo
path, with visible labels/file groups and per-plan required confirmations. The
desktop, SDK and native plugins share those guards. See
[annotated changes](Docs/ChangeAnnotations.md) for limits and pending acceptance.

Workspace sessions also retain wrap/visible folds, document scroll anchors and
image zoom/center/filter settings through the common editor and CyUI canvas.
Source fingerprints and image dimensions guard replay; new native/GUI acceptance
remains pending. See [workspace sessions](Docs/WorkspaceSessions.md).

Semantic coloring now negotiates full/delta server responses with bounded native
bases, refresh/restart invalidation and full-request fallback. The existing
desktop/SDK/plugin service and Unicode/revision guards remain shared. See
[semantic updates](Docs/SemanticTokenUpdates.md) for limits and pending acceptance.

View > CMake preset actions adds reviewed CTest/CPack/workflow execution with
the common build reservation, cancellation and SDK/plugin surface. Read-only
inspection never runs project tools. See [Preset actions](Docs/CMakePresetActions.md).

Call/type hierarchy now shares actual LSP preparation/expansion, guarded issued
sessions and per-call-site navigation across the desktop, real SDK and native
plugin host. See [Hierarchies](Docs/Hierarchies.md) for controls, limits and
pending runtime acceptance.

File > New text document now creates an Engine-owned untitled draft. Save as writes
a new workspace file while retaining the live document/editor identity and Undo;
empty drafts use the existing guarded Recovery workflow. Three SDK/native-plugin
operations share those services. See [untitled documents](Docs/UntitledDocuments.md)
for destination restrictions and pending native/GUI acceptance.
Extensions also manages [signed local packages](Docs/PluginPackages.md), explicit
publisher trust, updates/removal, retained-version rollback and guarded cleanup.
The native service is shared with six SDK operations; installed versions apply to
new processes, and workspace changes retain the process selection. Monocypher
4.0.3 supplies Ed25519/SHA-512. Native package and graphical acceptance is pending.
Workspace tabs/cursors, recent files, panel visibility and docking layout now
restore through independent per-instance metadata; text recovery remains a
separate guarded operation. See [workspace sessions](Docs/WorkspaceSessions.md).
The debugger adds capability-gated function/exception breakpoints, paged scopes,
register scopes, memory and disassembly views through shared native/MCP/plugin
services. See [advanced debugging](Docs/AdvancedDebugging.md).
Maven/Gradle build/test/run use real Java launchers and runtime dependency models
in the existing project-task panel. See [JVM builds](Docs/JvmBuilds.md).
These additions compile on Windows; their new native/GUI acceptance remains
unexecuted during the antivirus interruption. Java debugging remains separate.
HLSL/GLSL also have typed DXC/glslang build tasks, real-tool diagnostics and bounded
DXIL/SPIR-V outputs. See [shader compilation](Docs/ShaderBuilds.md) for tool
requirements and the unimplemented GPU/Unreal shader environments.
The bundled [Subversion plugin](Docs/Subversion.md) adds SVN provider actions and
shared review views through the same project/root mapping as Git. Install the SVN
client separately; this addition is compiled on Windows but not runtime-validated.
The bundled [Perforce plugin](Docs/Perforce.md) uses the same native provider UI
for checked-client review, checkout/changelists, one-file submit/revert, locks and
guarded sync. It requires an installed P4 CLI; actual server acceptance is pending.
The bundled [Mercurial plugin](Docs/Mercurial.md) adds JSON status/history, shared
Diff/annotations, exact-file add/forget/revert, commit, clean update,
bookmark/named-branch creation and configured-alias pull/push. Install Hg separately;
Windows production compilation passed, real-client/GUI acceptance remains pending.
The bundled [Diversion plugin](Docs/Diversion.md) uses the same mapping/forms for
status/review, commits, clean checkout/update, branch creation and locks. It follows
Diversion's automatic tracking/synchronization model. Install and authenticate the
client separately; real client/agent/GUI acceptance remains pending. See
[custom providers](Docs/CustomVersionControl.md) for the independent VCS SDK contract.
The supplied logo and application/document icons are in `../icons/`; keep this
folder beside the executable. See [branding](Docs/Branding.md) for integration.
Images open in dockable read-only tabs with background decoding, transparency,
zoom/pan, actual-pixel view and reload. PNG, JPEG, static WebP, GIF first frame,
BMP, TGA, composited PSD, Radiance HDR and binary PGM/PPM are supported.
See [image previews](Docs/Images.md) for controls, permissions and format limits.
File → New project creates C/C++ CMake, Rust Cargo, C# .sln/.NET, standalone Java
and web projects from exact file previews. Generated projects use the existing
native build adapters. See [project creation](Docs/ProjectTemplates.md) for tools,
permissions and template limits.
Installed ABI2 plugins can extend that wizard with declarative templates and typed
options. The bundled Unreal extension adds a C++ game with its module and
Game/Editor targets, using the existing UBT generation/build services.
Type/symbol search, file structure, C++ rename previews and project-aware formatting
use the same clangd service. Navigation can open registered Unreal/SDK sources in
read-only tabs, with separate access checks from editable project documents.
Shared language-server code actions provide issued fixes/refactorings, lazy
resolution and preview/apply/undo through the native core and MCP. Alt+Enter /
Ctrl+. opens the compiled GUI panel; its full desktop acceptance remains pending.
Installed-tool detection, saved CMake profiles, target builds and compilation
database synchronization with clangd are provided by the shared native core.
Visual Studio solutions and Unreal .uproject files open directly. UnrealBuildTool
generation/build/index operations share native services with MCP. Floating panels
and documents use real SDL/OpenGL system windows.

XML `.slnx` solutions share the native hierarchy/configuration/MSBuild services.
The CMake panel also selects configure/build presets, with shared MCP/plugin
discovery, inheritance/path guards and actual CMake execution. See
[project formats](Docs/ProjectFormats.md) for limits and pending runtime validation.

Plugins can subscribe to committed workspace/document/resource/build metadata
through the shared native event journal, with manifest permissions, snapshot
handshakes and error suspension. See [workspace events](Docs/Events.md).
Versioned language diagnostics also feed the common editor's squiggles/tooltips;
see [editor diagnostics](Docs/EditorDiagnostics.md) for revision guards and limits.
ABI2 extensions can also contribute stdio-DAP adapters, selected in the Debugger's
Launch configuration and shared with MCP/plugin requests. The host owns their
processes and revokes sessions when a provider is disabled or changed. See
[plugin debuggers](Docs/PluginDebuggers.md) for options, permissions and pending
runtime validation; the bundled Unreal extension adds a CodeLLDB entry.

Unsaved text buffers receive background recovery checkpoints every second.
View > Recovery offers inactive checkpoints with a shared CyUI code preview,
guarded restoration into memory and editor Undo/Redo. Project files are written
only by explicit Save. See [Recovery](Docs/Recovery.md) for quotas and limitations.

View > Files adds the common CyUI Assets browser, including empty directories.
Project context menus prepare guarded file/folder creation, rename/move and deletion.
Deletion uses persistent project trash; open buffers/window identities and text Undo
are preserved. See [file operations](Docs/Resources.md) for restoration, shared MCP
and plugin services, limits and the distinction from semantic refactoring.

Non-Unreal solutions and direct .vcxproj/.csproj projects use native MSBuild build,
clean/rebuild/restore, configuration mappings, evaluated artifacts and explicit C++
index preparation. Windows C++ and SDK C# builds are exercised. See [MSBuild](Docs/MSBuild.md).

Native interactive terminals and DAP debugging are integrated. PowerShell/cmd use
ConPTY on Windows; POSIX PTYs are implemented for Linux/macOS but remain unverified.
Source breakpoints, conditional/log breakpoints, threads/stack/locals, watches,
expression evaluation, stepping, run and attach/detach share native services with
MCP. See [Run, terminal and debugging](Docs/Execution.md).

C#/.NET run/debug uses the same panels and MCP operations through a pinned
NetCoreDbg adapter. Runnable managed assemblies and apphosts are detected from
their metadata; portable PDB breakpoints, variables, watches, stepping and
attach/detach are exercised on .NET 8. Roslyn language services are described in
[C#](Docs/CSharp.md).

Git includes branches, remotes, fetch/pull/push, stashes, history/blame and a
three-way text-conflict view with explicit resolution. See [Version control](Docs/VersionControl.md).
The repository selector follows the active file or an explicitly selected nested
Git root; Worktrees adds guarded catalogue/create/lock/unlock/clean removal through
the same native Engine and SDK. New native/GUI acceptance is pending; see
[Git worktrees](Docs/GitWorktrees.md).

Native plugins can declare host-version and OS/architecture constraints, checked
before library loading. ABI2 activation notifications support stopping background
work on disable and resuming per-instance state; enabled dormant dependents start
when their dependency becomes active. See [the plugin SDK](Docs/Plugins.md).

View > Tests discovers and runs selected configured CTest tests with per-test
results/output and process cancellation. Version control > Hunks stages/unstages
issued text blocks. Global regex search includes unsaved buffers. ABI2 plugins
can add host-owned LSP servers; Python/Pyright and GLSL Analyzer use the same
native services. See [development workflows](Docs/DevelopmentWorkflows.md) for
installation, controls, MCP/SDK contracts, exact limits and remaining scope.

Rust, Java, JavaScript/TypeScript (including JSX/TSX), HTML and CSS now have
shared incremental syntax parsers and independent language servers. See
[language support](Docs/Languages.md) for capabilities, prerequisites and remaining
project/build/debug workflows. CyCodeReader shares their syntax parsers.
Problems can remember language-server settings and activation choices per project,
restore installed defaults, reload or forget saved settings. GUI, MCP and plugins
share the native store with revision/file-lock guards. See
[language preferences](Docs/LanguagePreferences.md) for activation and validation limits.
Cargo, npm and standalone Java projects have native build/test/run tasks and an
integrated target/script panel. Cargo binaries use the existing native debugger.
Cargo also exposes workspace/features/custom-profile and bin/example/lib/test/bench
selection with package/kind executable guards. See [Cargo workspaces](Docs/CargoBuilds.md)
for controls, path boundaries and pending expanded runtime acceptance.
See [Project tasks](Docs/ProjectTasks.md) for exact scope and remaining adapters.

## Build

Build > Cancel build covers CMake, MSBuild and explicit Unreal operations, with
shared MCP/plugin controls and operation IDs. See
[Build cancellation](Docs/BuildCancellation.md) for scope and validation limits.

The companion [CyCodeReader](https://github.com/MrMybal/CyCodeReader) now has its
own repository and build. It owns the shared editor, syntax palette and native
file services consumed at the revision in editor-source.lock.json. The ordinary
build produces only CyCode. Use --editor-root PATH with fetch_dependencies.py and
build.py to select an explicit development checkout. --no-install retains outputs
under runtime/ without copying a root executable.

Requirements: Python 3.11+ for build scripts, CMake 3.24+, a C++20 compiler,
Git. The build fetches a checksum-pinned ripgrep binary into runtime/tools.
Windows: Visual Studio 2022 C++ workload. Linux/macOS: Ninja and
the SDL3 platform development libraries. The native application runs without Python;
the optional integrated MCP service uses its private Python environment.
The GUI requires an OpenGL 3.3 driver (3.2 on macOS).

CyUI is resolved at the exact revision in app/cyui-source.lock.json. The default
build uses a separate dependency cache; an explicit clean checkout must match that
revision. See [modular CyUI integration](Docs/CyUI-Migration.md) for migrated views,
module selection and remaining adapters:

Official CyUI source: [MrMybal/CyUI](https://github.com/MrMybal/CyUI), currently
private. [CyUI MIT license](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE)
is also included as [licenses/CyUI.txt](licenses/CyUI.txt).

```sh
python app/scripts/fetch_dependencies.py
python app/scripts/build.py --no-install
# Optional matching checkout: --cyui /absolute/path/to/CyUI
```

After a Windows Release build, create the allow-listed development ZIP with:

```sh
python app/scripts/package_windows.py --build runtime/build-vs
```

The package has a source/dependency manifest and SHA256SUMS.txt. Prepare the
redistributable MCP first with prepare_mcp_runtime.py; see Docs/Build.md. New
Windows packages include the app-local MSVC runtime, verified ripgrep and pinned
Python/CyToolsCore/MCP environment. An OpenGL 3.3 driver is still required.
Language servers, compilers and debugger adapters remain separate prerequisites.
Development venvs, credentials, user settings and dependency source trees are
excluded. Public releases through 0.2.3 predate the bundled MCP environment.

Downloaded archives are pinned by revision and SHA-256 in dependencies.lock.json.
The fetch script never modifies CyUI or CyToolsCore. Build output is under runtime/;
the build script copies the desktop executable to the installation root.
For a core/CLI/plugin build without CyUI: python app/scripts/build.py --core-only.
Ordinary builds do not compile or launch native tests. See [local testing](Docs/Testing.md)
for source-only checks, isolated test builds and the current antivirus interruption.

GitHub Actions resolves the exact shared-editor and SDK source commits. Public
source dependencies need no additional credential; a private Reader checkout can
use CYCODE_READER_READ_TOKEN with read access. Never commit that token or include
it in a package. Finalization.md records actual completed platform matrices;
configuration alone does not establish a pass.

For the source/development runtime (or older packages), obtain the actual CyToolsCore 0.10.1 wheel:

```sh
python app/scripts/setup_runtime.py --core-wheel /absolute/path/to/cytools_core-0.10.1-py3-none-any.whl
```

The default wheel location is the sibling CyToolsCore/dist checkout. This is a local
development dependency, not an invented public package repository. No SDK source,
environment or machine-specific D: path is copied into the repository.

## Run

Windows: CyCode.exe. Linux/macOS after compilation: ./CyCode.sh.
Arguments: --workspace PATH, --open RELATIVE_FILE, --theme KEY, --language en|fr,
--keymap rider|visualstudio|vscode.
`--line` and `--column` select a one-based UTF-16 location in `--open`.
For an automated render: --capture ABSOLUTE_FILE.bmp --theme navy.
Use --show-cmake to open the CMake settings panel at startup.
Use Ctrl+O for .sln/.uproject, or pass the project path directly. --show-unreal
opens the Unreal panel. File > New window starts another independent instance.
Drag document tabs or panels out to create native windows and dock them back.
Closed panels reopen from View or the sidebar; layouts are stored per workspace.
Graphite is the default neutral dark theme, with original vector file-type icons.
The five original CyUI themes remain selectable. Default text size is increased 15%.
Keyboard profiles and command overrides are under Options > Keyboard shortcuts.
Ctrl+Shift+F searches the workspace, Ctrl+F the document, Ctrl+Alt+F file names;
double Shift opens files/actions search. See [Commands](Docs/Commands.md).
In the Rider profile, Ctrl+N finds types, Ctrl+Alt+Shift+N finds symbols, Ctrl+F12
opens file structure, Shift+F6 previews a rename and Ctrl+Alt+L formats using the
project's .clang-format. Rename previews show wrapped before/after code with subtle
change backgrounds and readable syntax in both dark and light themes. Applying a
rename updates live buffers; Save all explicitly persists them. See [Semantic editing](Docs/Semantic.md).
Capture mode ignores the saved layout, applies explicit theme/language overrides,
does not save preferences and does not start Python or clangd.

## Check

The default check launches no native executable:

```sh
python app/scripts/verify_sources.py
python app/scripts/build.py --plan --no-install
```

Native binaries are opt-in: `build.py --build-tests` compiles them without running;
`build.py --run-tests` also executes CTest serially and stops on the first failure.
These use separate caches and do not copy over the root executable. The suite
includes real language servers, debuggers and generated-program execution; it is
currently suspended locally following reported Kaspersky deletions. See
[Testing](Docs/Testing.md) before explicitly launching native integration checks.
Set CYCODE_TEST_CLI to the absolute built cycode_cli path, then:

```sh
python -m unittest discover -s app/tests -p test_mcp.py -v
```

Use the private runtime/python interpreter for those tests. They exercise the native
process, SDK permissions, official MCP stdio transport and shared HTTP service.
scripts/verify_gui.py renders all themes and EN/FR without editing project files.
scripts/verify_navigation.py checks real desktop navigation, Unicode cursor positions
and References renders in EN/FR and dark/light themes using owned temporary projects.
scripts/verify_cmake.py --cli ABSOLUTE_CLI configures/builds an owned project and
verifies restored profiles, Unicode paths and the CMake panel in all five themes.
scripts/benchmark.py records repeatable search/index baselines, not performance promises.
scripts/verify_shortcuts.py sends real key events to verify profiles, editing, chords
and searches with a minimal Windows PATH. scripts/verify_unreal_completion.py checks
automatic UHT preparation and actual Unreal completions in the native editor.
scripts/verify_semantic.py exercises actual symbol navigation, multi-file rename,
formatting/undo and refactoring renders in all six themes and French.
scripts/verify_unreal_navigation.py opens AActor's engine header from the owned
Unreal completion fixture as a read-only SDK document.
scripts/verify_csharp.py exercises C# completion/insertion, project-reference
navigation and Roslyn formatting through actual desktop commands and captures.

## Architecture and scope

- [Architecture](Docs/Architecture.md)
- [C# language services](Docs/CSharp.md)
- [Run, terminal and debugging](Docs/Execution.md)
- [Toolchains and CMake](Docs/Build.md)
- [Solutions and Unreal Engine](Docs/Unreal.md)
- [Commands and keymaps](Docs/Commands.md)
- [Plugin SDK](Docs/Plugins.md)
- [Workspace monitoring and paged file index](Docs/Workspace.md)
- [MCP integration](Docs/MCP.md)
- [Implemented capability coverage](Docs/CapabilityCoverage.md)
- [Validation](Docs/Validation.md)
- [Roadmap](Docs/Roadmap.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

CyUI and CyToolsCore must be made accessible with their licenses and fixed revisions
before this repository can be built publicly. Do not publish runtime/, data/ or
connection descriptors. Native plugins are trusted code, not sandboxed extensions.

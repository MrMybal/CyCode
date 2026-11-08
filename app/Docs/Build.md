# C/C++ toolchains and CMake

RelatedFiles.cpp joins cycode_core; its fixture joins the existing opt-in core test
target. NavigationUI reuses existing Core/Commands. No executable target, external
dependency or CyUI module is added; ordinary builds keep BUILD_TESTING=OFF.

Selection capture reuses the existing CompletionEngine, UserSnippets, catalogue
escaping and personal JSON UI sources; no build dependency, source target or
CyUI module is added. See [Capture](SnippetCapture.md).

UserSnippets.cpp joins the native core and UserSnippetsUI.cpp joins the UI. They
reuse existing Monocypher, NativeFileLease, shared text-file code and CyUI CodeEditor;
no external dependency, module or executable target is added. The new fixture
joins the existing opt-in completion target, excluded by BUILD_TESTING=OFF. Regenerate
manifest/locales with the commands below. Personal data is not redistributed.
See [Personal snippets](UserSnippets.md).

Plugin snippets extend existing core/UI/provider sources and add
plugins/unreal/CodeSnippets.h. No dependency, CyUI module, executable or test target
is added. Reconfigure to copy the Unreal 1.6.0 manifest, then build the ordinary
production targets with BUILD_TESTING=OFF. Existing ABI2 size/feature checks allow
the provider to omit its optional catalogue on an older host. Shared Reader/CyUI
pins and numeric snippet ownership are unchanged.

Selection wrapping extends existing sources with CompletionSelection.h and one
fixture source in the existing opt-in completion target. It adds no library,
module, executable or test target. BUILD_TESTING=OFF keeps native fixtures excluded.
Regenerate schemas/locales with the same commands below.

SnippetCatalogue.cpp joins cycode_core and SnippetsUI.cpp joins cycode_ui. Existing
CyUI Commands/Core and native JSON/completion/ChangePlan services suffice. The new
regression source joins the opt-in completion target, excluded by BUILD_TESTING=OFF;
no new executable target, third-party library, module or notice is added. Regenerate
app/CyTool.json with `python app/contracts.py` and app/locales/ui.json with
`python app/scripts/write_locales.py`. See [Project snippets](SnippetCatalogue.md).

Completion variable transforms add source-pinned PCRE2 10.49 and utf8proc 2.12.0
to app/dependencies.lock.json. `python app/scripts/fetch_dependencies.py --core-only`
fetches and verifies their exact archives as part of the usual native dependency
set. CMake builds only pcre2-8-static and utf8proc for the core's private links;
PCRE2 tools, JIT, 16/32-bit libraries, chartable regeneration and both upstream
test suites are disabled. Their excluded, unlinked targets are not built.
BUILD_TESTING=OFF remains required for production. The new regression source joins
the existing completion target only; no native test executable is produced by a
normal build. There is no extra runtime DLL, CyUI module or GUI dependency.
The standalone updater retains its files/lease/updates-only boundary. The Windows
packager includes licenses/PCRE2.txt and licenses/utf8proc.txt through its existing
license-directory copy. See [Completion variables](CompletionVariables.md).

UpdateHistory.cpp joins cycode_updates and UpdateHistoryUI.cpp joins cycode_ui;
UpdateHistory/UpdateHistoryPolicy are their native model/policy. The fixture source
joins the existing opt-in core target only, with BUILD_TESTING=OFF in production.
No new target, external dependency or CyUI module is introduced. Helper references
and its manifest stay unchanged. See [History](UpdateHistory.md).

Recovery progress adds UpdateRecoveryProgress/UpdateRecoveryFeedback to the existing
native/helper sources. The Windows production helper links the system comctl32.lib
and Threads::Threads. Its configured resource embeds Updater.manifest, requesting
Common-Controls 6 and asInvoker, with the MSVC generated manifest disabled to avoid
a duplicate resource. No downloaded runtime, executable target or CyUI module is
added; helper references remain files/native-lease/updates only. BUILD_TESTING stays
OFF. See [Recovery controls](UpdateRecovery.md).

Space/progress adds only the UpdateBudget and UpdateProgress headers to existing
native/update/UI sources; it creates no target, dependency or CyUI module. Windows
uses the existing system Kernel32 API for caller-available space; non-Windows
transport remains manual. Thirty actual policy assertions are compiler-evaluated
without a native test executable. See [Download observations](UpdateDownloads.md).

Retry adds only the internal UpdateFeedback header and extends the existing
UpdateDownload/Transfer/Files and UpdatesUI/StorageUI sources. It adds no target,
dependency, external adapter or CyUI module. Opt-in download/module contracts stay
in their existing targets; production testing remains OFF. See
[Download retry](UpdateDownloads.md).

UpdateDownload joins cycode_updates; UpdateRange is its shared constexpr response
policy. Download regression sources join only the existing opt-in core target.
BUILD_TESTING remains OFF for production. Nineteen actual policy assertions are
compiler-evaluated by /Zs without generating/running native test executables.
There is no new target, external dependency or CyUI module; see
[Download resumption](UpdateDownloads.md).

UpdateStorage joins cycode_updates, UpdateStorageUI joins cycode_ui, and the cache
fixture joins the existing opt-in core target. No target/dependency/module is added.

UpdateManifest/UpdateRecovery join the existing cycode_updates target; the existing
production helper adds UpdaterRecoveryUI. There is no new target, third-party runtime
or CyUI module; the current helper's additional Windows system dependency is described
above. The recovery fixture source joins the opt-in core target only.

UpdateCache is part of the existing cycode_updates library; it adds no target or
third-party dependency. It reuses cycode_files for atomic helper outcome records.

Application updating adds the production CyCodeUpdater on Windows as a dependency
of CyCode. cycode_updates links only the shared file/lease implementation and system
WinHTTP/BCrypt; it does not link the IDE, SDL, ImGui or SDK. NativeFileLease.cpp now
has one cycode_native_lease target reused by the core/updater. Existing CyUI modules
are unchanged. BUILD_TESTING=OFF still excludes all native fixtures.
The Windows packager includes the helper and writes both the normal compressed
ZIP and a stored -update.zip, with manifest format 1 and exact file sizes/hashes.
It reads PE resources instead of launching a CLI, and requires clean source and
matching executable/descriptor versions. See [Application updates](Updates.md)
for release publication, private-access configuration and recovery limits.

Document occurrences add OccurrenceCatalogue to the existing core target and
OccurrencesUI, EditorOccurrences and EditorTextMap to cycode_ui. Diagnostics reuse
the latter coordinate adapter. The new occurrence regression source joins the
existing semantic fixture; existing command/module/lifecycle fixture sources are
extended. No new executable target, dependency, CyUI module, backend copy or notice
is needed. Production keeps BUILD_TESTING=OFF; do not run native tests implicitly.
Regenerate app/CyTool.json via app/contracts.py and locales/ui.json via
scripts/write_locales.py. See [Document occurrences](Occurrences.md).

Source navigation adds NavigationLocations.cpp to cycode_core and NavigationUI.cpp
to cycode_ui, reusing the existing CyUI Core filter and result controls. The
navigation regression source joins the existing opt-in semantic fixture; command,
module and lifecycle sources extend existing targets. Production BUILD_TESTING=OFF
does not build/run those fixtures. No new module, dependency, executable/test
target or license notice is needed. Existing bridge/locale regeneration commands
remain. See [Source navigation](SourceNavigation.md).

Symbol navigation adds SymbolCatalogue to cycode_core and StructureUI to cycode_ui,
using existing CyUI Core, its shared filter/folding and ImGui tree controls. Deferred
symbol resolution extends the same native Semantic/LanguageService and existing
Application/SemanticUI sources. No build integration or dependency change is needed;
regenerate the bridge with `python app/contracts.py` and locales with
`python app/scripts/write_locales.py` after contract/label changes. No new
module or dependency is selected. Source regressions join existing semantic,
module and lifecycle test targets; production BUILD_TESTING=OFF does not build or
run them. No new executable/test target is introduced. See
[Symbol navigation](SymbolNavigation.md).

Compiler source links add CompilerLocation to the existing native core. GUI
reuses BuildUI/ModuleUI and the already selected CyUI Core/Diagnostics modules.
The compiler-location regression source joins the existing opt-in build target;
production BUILD_TESTING=OFF does not build/run it. Diagnostic lifetime tests
extend the existing language fixture. No extra dependency, module or executable
target. See [Compiler locations](CompilerLocations.md).

TaskOutput now provides the shared journal implementation in BuildOutput; the new
TaskOutputText core source frames Cargo display messages. Existing ProjectTasks,
ModuleModels/ModuleUI, Commands and BuildUI provide status and diagnostic navigation.
Existing CyUI Diagnostics/Core suffice. No new dependency or executable target;
regression sources join the existing BUILD_TESTING-only build/project/module targets.
Ordinary production remains BUILD_TESTING=OFF. See [Project output](ProjectTaskOutput.md).

Live build output adds BuildOutput to the existing core, extending the internal
process observer and BuildOperations reservation. BuildUI reuses the selected
CyUI Diagnostics module. No new library, module or executable target is required.
Its regression source joins the existing build fixture, excluded from ordinary
BUILD_TESTING=OFF production builds. See [Live build output](LiveBuildOutput.md).

Instruction breakpoint support adds InstructionBreakpoints to the native core and
InstructionBreakpointsUI to the UI, reusing DebugInspection, JSON and CyUI Core.
Its source assertions join the existing debugger target; normal BUILD_TESTING=OFF
production builds do not generate or run that test executable. No extra library,
adapter, runtime executable or dependency is introduced.

Advanced execution adds DebugExecution to the existing native core and
DebugExecutionUI to the UI. It reuses JSON, the DAP worker, commands and CyUI Core.
The new regression source joins the existing debugger test target. Production
BUILD_TESTING=OFF does not build/run it or a new test executable. No recording,
debugger or reverse-execution dependency is installed by this feature.

Debugger value/memory editing adds DebugMutations to the native core and
DebugMutationsUI to the existing UI target. It reuses JSON, the DAP worker and
CyUI Core widgets; no new library, module or executable is required. Its regression
source joins the existing debugger test target, which stays excluded from ordinary
BUILD_TESTING=OFF production builds. See [Value editing](DebugValueEditing.md).

Java debugging adds only JavaDebug/LoopbackStream core sources, reusing existing
JSON, threading and Windows ws2_32 dependencies. No additional CyUI module, native
test executable, Python backend or external library is compiled. JDT LS and its
separately installed Microsoft java-debug JAR are runtime prerequisites, not
build dependencies. `CYCODE_JAVA_DEBUG` supplies the JAR to default server
initialization, with explicit remembered profiles remaining authoritative. See
[Java debugging](JavaDebugging.md); ordinary builds retain `BUILD_TESTING=OFF`.

JDWP attach reuses those same sources/dependencies. No JDWP client library,
remote helper, new binary or CyUI module is added. Its network connection is
performed by the existing local JDT LS/java-debug owner only on explicit attach.

CMake test/package/workflow presets now have a separate ordered preview and
reviewed execution path, sharing the ordinary native cancellation/deadline
reservation. View > CMake preset actions and the Build menu expose them; see
[Preset actions](CMakePresetActions.md). These project commands are distinct from
this repository's opt-in developer test builds, which remain disabled in ordinary
application builds.

The application build also produces bundled Subversion, Perforce and Mercurial providers. SVN reuses
`cycode_process`, `cycode_files` and the already pinned TinyXML2 dependency without
linking UI/Engine services into its library. `BUILD_TESTING=OFF` keeps the separate
`svn_provider` regression executable out of production builds. Git remains built
in; SVN is an optional installed CLI resolved through PATH or `CYCODE_SVN`.
Perforce links the shared process/file utilities and JSON, without additional
dependencies; resolve its separately installed `p4` through PATH or `CYCODE_P4`.
The separate `perforce_provider` regression also remains opt-in. See
[Subversion](Subversion.md) and [Perforce](Perforce.md).
Mercurial also reuses the shared process/file utilities and JSON, without new
dependencies. Its production target is `cycode_mercurial`; resolve the separately
installed `hg` with PATH or `CYCODE_HG`. The `mercurial_provider` regression is
opt-in behind `BUILD_TESTING`, like the other provider regressions. See
[Mercurial](Mercurial.md). No external VCS client or credentials are packaged.
Diversion adds `cycode_diversion` using the same process/file/JSON dependencies;
its external CLI is resolved through PATH or `CYCODE_DV`. The separate
`diversion_provider` regression is also gated by `BUILD_TESTING`. No external
client is launched as part of compiling it. See [Diversion](Diversion.md).

Configure/build presets are available in the CMake panel and native/MCP/plugin
services. CMake applies their cache/toolchain/environment/condition semantics;
CyCode reads bounded metadata and guards include/binary paths. See
[Project formats](ProjectFormats.md) for inheritance, selection and validation limits.

Visual Studio solution/project builds have a separate native [MSBuild adapter](MSBuild.md).
Unreal solutions retain their UnrealBuildTool workflow.

Open Build > CMake settings, or start the desktop with --show-cmake. The CMake
panel selects source/build directories, toolchain, generator, configuration and
target. Configure applies those settings; Build compiles the selected target, or
the default target set when All targets is selected. Output remains in Build.
Build > Cancel build and the Build panel request owned process-tree termination.
The same services cover MSBuild and explicit Unreal operations, including MCP and
plugin host requests. See [Build cancellation](BuildCancellation.md) for IDs,
reservation lifetime, scope and the current unexecuted regression checks.

Detection runs off the UI thread. It queries CMake capabilities, compiler versions
and installed Visual Studio instances through vswhere. Standard Clang/GCC pairs
are found on PATH; Windows also checks LLVM and CMake/Ninja bundled with Visual
Studio. A generator listed by CMake is a supported generator format, not proof that
every prerequisite for that generator is installed. GCC is detected by the same
portable path but was not installed or exercised on the local Windows machine.
The initial adapter offers installed Visual Studio generators, Ninja (single
configuration), and Unix Makefiles when their build tools are found. Ninja Multi-Config,
Xcode and other generator integrations remain future work.

Automatic selects a discovered compiler for a new tree and preserves the compiler
and generator of an existing tree. On Windows, MSVC uses a Visual Studio generator,
x64 host/target and the detected MSVC tools version. This milestone does not use
MSVC with Ninja: localized showIncludes/dependency handling needs separate validation.
Clang/GCC use Ninja when available, otherwise Unix Makefiles. Presets can supply
custom toolchain/compiler/generator options through CMake. Broader cross-toolchain
validation and automatic MSVC/Ninja environment setup remain work items.

A compiler or generator change requires a new build directory. CyCode does not
delete CMake caches. Existing trees without compiler metadata can be configured
with Automatic first, or replaced by an explicitly chosen new build directory.
Source and build paths must remain in the workspace; in-source builds and build
directories containing the source tree are rejected. Build configuration choices
are Debug, Release, RelWithDebInfo and MinSizeRel. A single-configuration generator
must be reconfigured before building a different configuration.

## Project metadata and code analysis

CyCode requests codemodel-v2 and toolchains-v1 using its own CMake File API client
query directory. It reads targets, artifacts and actual compiler information from
the referenced replies. It never evaluates CMake scripts to inspect a configured
project. Configure/build operations execute trusted project code as normal CMake
operations do. Inspect validates reply references and source/build identities.

For Ninja/Makefiles, the IDE uses CMake's native compile_commands.json. Visual
Studio generators do not export it, so CyCode derives a database from the selected
configuration's CMake File API compile groups: compiler, command fragments,
definitions, includes and reported implicit include directories. Only workspace
C/C++ translation units are exported. The generated database lives under
BUILD/.cycode/clangd/CONFIGURATION and is labelled cmake-file-api in the UI/API.
It describes analysis commands rather than a replacement build system. CMake still
owns the actual build. Advanced module/PCH/unity/framework and cross compilation
workflows have not been validated by this adapter.

The shared clangd receives the selected database directory through its initialization
options. A successful configure restarts it and resynchronizes live unsaved buffers,
including when flags change at the same database path. CyCode does not copy a database
over the workspace's root compile_commands.json or edit the user's .clangd.
Failed configure operations keep the previously active profile/database selection.

Successful operations save the active local profile in .cycode/build.json, schema
version 1. It contains source, buildDirectory, configuration, toolchain, generator
and target. Toolchain IDs may contain machine paths; this is local state, ignored
in CyCode's own repository. Invalid profiles are reported and preserved. They can
be repaired or removed explicitly; CyCode will not overwrite an unrecognized format.
The build tree gets a .gitignore if none exists, and .cycode metadata is excluded
from project text/file search. Existing ignore files are preserved.

Primary references: [CMake File API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html),
[compilation database export](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html),
[clangd compilation settings](https://clangd.llvm.org/extensions#compilation-commands).

The advanced DAP, JVM build-model, shader-task and workspace-session services introduce no
new downloaded library. Maven XML reuses the pinned UI-free TinyXML2 target;
the native lease uses Windows/POSIX system APIs. JVM distributions/JDKs remain
separate tools; DXC/glslang are also separate installed prerequisites. Opening a
project does not install them. Contract regression sources join the existing
opt-in core, debugger, project-task and extension test targets.
Ordinary app builds retain `BUILD_TESTING=OFF`; MSVC `/Zs` checks those sources
without generating a test executable. See [Testing](Testing.md).

Signed local packages add Monocypher 4.0.3, pinned by release/archive checksum in
dependencies.lock.json. `fetch_dependencies.py --core-only` now includes its sources.
`cycode_crypto` is a private static C99 dependency of the native core, with hidden
ELF/Mach-O visibility; no extra runtime crypto DLL is packaged. Its optional
Ed25519/SHA-512 source is compiled with the library, without upstream tests/demos.
The full third-party notice is in licenses/Monocypher.txt. Package authoring's
Python `cryptography` dependency is separate and optional; it is not required by
the app's native verifier. See [Plugin packages](PluginPackages.md).

The expanded Cargo workspace/feature/profile/target adapter adds no build dependency
or CyUI module. Cargo is a separate installed/private tool used only by explicit
project tasks, not to compile CyCode. Existing application builds keep native tests
disabled; the new regression source joins the opt-in project-task target. See
[Cargo workspaces](CargoBuilds.md) for exact controls and validation limits.

RustAnalysis adds no external dependency, runtime or CyUI module. It derives a
typed overlay inside the native language service and uses the existing server
process owner, preference store and project-task inspection. Regression sources
join the existing opt-in lifecycle target, not the production build. See
[Rust analysis](RustAnalysis.md).

Named Rustup selection is optional at runtime, not a dependency for compiling
CyCode. Install/configure Rustup separately (PATH or CYCODE_RUSTUP) and use an
already installed toolchain/component. RustToolchain adds only native planning
helpers; no installer, proxy executable, downloaded library or CyUI module is
packaged by this change. See [Rust toolchains](RustToolchains.md).

Installation headroom/failure policies use existing updater sources and Windows
volume/handle APIs; no new dependency or CyUI module is selected. The new regression
source joins the existing opt-in core target. Keep ordinary builds/testing OFF; see
[Installation checks](UpdateInstallation.md).

Recovery headroom/staging extends existing updater sources and Windows volume/handle
utilities, without a dependency/module/target change. Tests stay OFF; existing opt-in
core sources carry budget and guarded-staging contracts. See
[Recovery preparation](UpdateRecovery.md).

Prepared recovery resumption stays in existing UpdateRecovery/UpdateBudget sources,
with the already-linked atomic text utility and no new target, dependency or CyUI
module. Immutable work/source records are application metadata, not build inputs
or published payload. Production tests stay OFF; see [Recovery](UpdateRecovery.md).

GUI builds also generate the bundled offline manual with Node.js 22+ / npm and
the pinned CyDoc package. See [Documentation](../.CyDocBuild/README.md) for cache,
checkout, regeneration and validation options. Core-only builds do not need Node.

## Offline interface fonts

The UI target uses CyUI Core's fallback-folder contract with the checked-in
`app/resources/fonts` resources. Ordinary builds require no font package manager,
network request or new native library. CMake installation and Windows packaging
copy the same allow-listed fonts, inventory and license notices. The UI-free
core/CLI do not load them. Windows can retain its system Segoe faces and glyphs.

`python app/scripts/prepare_fonts.py` verifies the shipped bytes. Resource authors
can explicitly install `fonttools==4.60.1` in their private Python environment and
run `python app/scripts/prepare_fonts.py --regenerate`. That command downloads only
the exact, digest-checked upstream resources, rebuilds the 104-glyph subset and
checks all outputs before replacement. It does not run during build or startup.
The codepoint/semantic mapping and sources are in `font-resources.json`.

CyUI's pinned public `iconsAvailable()` state currently describes its own Windows
font loader. The application merges an open glyph font through the existing
ImGui host contract, so common icon buttons and host resource rows can render
without copying CyUI widgets. Some optional CyUI decorations still consult that
Windows-only state and remain omitted outside Windows. Do not represent that
library limit as complete cross-platform icon parity.

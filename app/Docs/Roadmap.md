# Roadmap

## Current milestone

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

General MSBuild solution/project build, clean/rebuild/restore, configuration mapping,
evaluated artifacts and C++ indexing are implemented. Windows C++ and SDK C# builds
are executed. See [MSBuild](MSBuild.md) for the supported scope.

- Richer command contexts, macOS keymaps and broader semantic search/index coverage;
  continue implementing the remaining Rider workflows as real services/actions.

- .slnx, broader project-system/toolset/custom-target coverage, multiple solution roots
  and advanced MSBuild compiler/analysis flags.
- Broader engine/SDK versions and roots, Blueprint/asset tools,
  Unreal-aware refactoring, Live Coding and editor/debugger integration. Normalize
  compiler-specific analysis flags further (UE 5.3/clangd 18 still reports an engine
  intrinsic compatibility diagnostic despite successful member completion/navigation).
- CMake Presets, custom/cross toolchains, compiler wrappers, configuration variants
  and broader validation of GCC, modules/PCH/unity/framework workflows.
- Command-based/resource-edit refactorings, broader code-action coverage,
  diagnostic squiggles and project tests;
  broader external-header navigation beyond registered SDK roots.
- Semantic token delta/range updates, advanced modifier styles and broader
  raw-string/preprocessor fallback coverage beyond the full-token clangd/Roslyn overlay.
- Filesystem watchers, external-change reload, rename/move/delete and project creation.
- Persistent path/symbol indexes; large-file and Unicode search benchmarks.
- Extend the implemented Cargo/npm/standalone-Java process-tree cancellation to
  CMake/MSBuild/Unreal and semantic requests; streaming output remains pending.
- Broader terminal compatibility/IME/fonts, shell integration and split sessions;
  richer DAP exception/function/data breakpoints, variable paging controls, memory,
  disassembly/register views, remote/core-dump workflows and further backend coverage.

## Complete version control and plugins

- Credential UI, interactive rebase/cherry-pick, binary/delete/rename conflict
  workflows, staging hunks, worktrees and multiple repositories.
- Provider-aware controls, validated third-party SVN/Perforce/Mercurial plugins.
- Plugin settings/dependencies, panel/project/language APIs, packaging/version policy.

## More languages and editor views

- C# broader managed debugging (frameworks, exceptions, async and web workflows),
  broader Roslyn code actions beyond the shared edit-only transport,
  snippet variables/transforms and a user catalogue,
  metadata/generated source views,
  Razor, rich documentation rendering/mouse hover and broader SDK/framework coverage beyond the implemented
  Roslyn language services and MSBuild builds. See [C#](CSharp.md).
- Broader Cargo workspace/toolchain/debug coverage beyond native build/test/run
  and binary debugging. Java Maven/Gradle dependencies, framework tests and Java
  debugging beyond standalone compilation/run. See [Project tasks](ProjectTasks.md).
  Analysis defaults disable Rust build scripts/procedural macros/check-on-save and
  automatic Java Maven/Gradle imports.
- Web framework/code-action coverage, advanced snippets and browser/debugger
  integration beyond the connected JS/TS/HTML/CSS servers and npm task/dev-server
  terminal execution.
- HLSL/GLSL compilation, resources and live GPU preview with renderer-specific targets.
- Mixed-project workspaces and per-project activation of services.

## Public distribution

- Select the project license; publish accessible fixed CyUI/CyToolsCore dependencies.
- Run actual Windows/Linux/macOS build and runtime matrices, including Unicode/IME,
  filesystem differences, DPI and portable icon/font coverage.
- Installers, updates/rollback, clean-machine validation, signing and contribution docs.
- Reproducible end-user packages with complete third-party notices.

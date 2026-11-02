# CyCode

Native C++20 IDE foundation using CyUI and Dear ImGui, with SDL3 windowing/rendering,
a syntax-aware text editor, cancellable ripgrep search, Git, native plugins, clangd
completion/diagnostics, definition/reference navigation and a CyToolsCore MCP
companion. Project license: [GNU GPL v3 only](../LICENSE), SPDX `GPL-3.0-only`.
Copyright (c) 2026 Cyberalien; see [COPYRIGHT](../COPYRIGHT).
Images open in dockable read-only tabs with background decoding, transparency,
zoom/pan, actual-pixel view and reload. PNG, JPEG, static WebP, GIF first frame,
BMP, TGA, composited PSD, Radiance HDR and binary PGM/PPM are supported.
See [image previews](Docs/Images.md) for controls, permissions and format limits.
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

Rust, Java, JavaScript/TypeScript (including JSX/TSX), HTML and CSS now have
shared incremental syntax parsers and independent language servers. See
[language support](Docs/Languages.md) for capabilities, prerequisites and remaining
project/build/debug workflows. CyCodeReader shares their syntax parsers.
Cargo, npm and standalone Java projects have native build/test/run tasks and an
integrated target/script panel. Cargo binaries use the existing native debugger.
See [Project tasks](Docs/ProjectTasks.md) for exact scope and remaining adapters.

## Build

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

The package has a source/dependency manifest and SHA256SUMS.txt. It excludes the
private development runtime, user settings and downloaded dependency source trees;
only the verified ripgrep binary and its notices are copied to tools/. The IDE
detects that directory before its development runtime/tools location. The Microsoft
Visual C++ 2022 x64 runtime and an OpenGL 3.3 driver remain prerequisites. The MCP
SDK, language servers and debugger adapters need separate local installation.

Downloaded archives are pinned by revision and SHA-256 in dependencies.lock.json.
The fetch script never modifies CyUI or CyToolsCore. Build output is under runtime/;
the build script copies the desktop executable to the installation root.
For core/CLI/plugin tests without CyUI: python app/scripts/build.py --core-only.

GitHub Actions needs a repository secret named `CYCODE_READER_READ_TOKEN` with
read access to the private CyCodeReader source repository. The default workflow
token cannot read another private repository. This secret is currently absent;
the local validation recorded for 0.2.0 does not establish that the CI matrix passes.
Never commit the token or include it in a release archive.

For the private MCP runtime, obtain the actual CyToolsCore 0.10.1 wheel:

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

The build script runs CTest, including delayed-response lifecycle tests and real
clangd completion/definition/reference tests when clangd is installed.
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
- [MCP integration](Docs/MCP.md)
- [Implemented capability coverage](Docs/CapabilityCoverage.md)
- [Validation](Docs/Validation.md)
- [Roadmap](Docs/Roadmap.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

CyUI and CyToolsCore must be made accessible with their licenses and fixed revisions
before this repository can be built publicly. Do not publish runtime/, data/ or
connection descriptors. Native plugins are trusted code, not sandboxed extensions.

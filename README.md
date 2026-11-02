# CyCode

CyCode is a native C++20 IDE built with CyUI, Dear ImGui and SDL3. Development
focuses on C/C++ and Unreal Engine projects, with C#/.NET services and a shared
CyToolsCore MCP integration.

[CyCodeReader](https://github.com/MrMybal/CyCodeReader) is the independent companion code/text editor. It
shares CyCode's editor and coloration, with file tabs, detached native windows,
find/replace and guarded saves, without loading IDE project services.
Its repository owns the shared `editor/` library; CyCode consumes a pinned source
revision from app/editor-source.lock.json. Select a development checkout with
--editor-root or CYCODE_EDITOR_ROOT. The CyCode build now produces only the IDE.

The current implementation includes:

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

The official [CyUI repository](https://github.com/MrMybal/CyUI) is currently private.
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

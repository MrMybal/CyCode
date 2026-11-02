# CyCode

CyCode is a native C++20 IDE built with CyUI, Dear ImGui and SDL3. Development
focuses on C/C++ and Unreal Engine projects, with C#/.NET services and a shared
CyToolsCore MCP integration.

The current implementation includes:

- Direct Visual Studio solution and Unreal project opening, CMake/MSBuild/Unreal
  build adapters, and project compilation settings for clangd.
- C++/C# completion, diagnostics, navigation, semantic colors, rename previews
  and formatting through clangd and Roslyn.
- Project search with ripgrep, document search, file/symbol lookup, and configurable
  Rider, Visual Studio and VS Code keyboard profiles.
- Native detached windows, independent application instances, themes and EN/FR UI.
- Git workflows, interactive terminals, C++/.NET debugging, and native plugin APIs.
- Shared live documents and native services for the GUI and MCP clients.

This is an active development milestone. It does not yet cover all Rider features.
Windows x64 has been tested locally; Linux/macOS execution remains unverified.
The project license has not been selected.

## Build

Use Python 3.11+, CMake 3.24+ and a C++20 toolchain. Windows builds use the Visual
Studio C++ workload. The GUI requires a sibling `CyImgui_Interface` checkout,
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
- [Architecture](app/Docs/Architecture.md)
- [Validation and current limits](app/Docs/Validation.md)
- [Roadmap](app/Docs/Roadmap.md)
- [Third-party notices](app/THIRD_PARTY_NOTICES.md)

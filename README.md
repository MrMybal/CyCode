<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://github.com/MrMybal/CyCode/tree/codex/core-intelligence">Native Core source</a><br>
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/en/overview">Documentation · HTML manual</a><br>
      <a href="https://github.com/MrMybal/CyCode">GitHub</a><br><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCode Core" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Project license</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>Published Core package</sub><br><a href="https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1"><strong>0.1.1</strong></a><br><strong>DEVELOPMENT PREVIEW</strong></p>
      <p><a href="https://github.com/MrMybal/CyCode/releases/download/core-v0.1.1/CyCodeCore-0.1.1-windows-x64-setup.exe"><strong>Install CyCodeCore</strong></a><br><sub>Windows x64</sub><br><a href="https://github.com/MrMybal/CyCode/releases/download/core-v0.1.1/CyCodeCore-0.1.1-windows-x64.zip">Portable ZIP</a></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCode Core</h1>

A native C++20 code engine for **AI agents, developer tools and editors**. Use
the same document, project and language services through a C++ library, a
persistent command-line service or the official CyToolsCore SDK/MCP bridge.

> **DEVELOPMENT PREVIEW · Core package 0.1.1**
>
> The published Windows Setup and portable ZIP are for the standalone Core.
> The newer 0.1.2 candidate is being validated locally and is not published yet.
> Read the [release notes and limitations](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1).

## One Core, separate applications

| Project | Role |
| --- | --- |
| **CyCode Core · this repository** | Native documents, project services, compiler-backed analysis, guarded changes and headless automation. |
| [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) | Desktop IDE: windows, panels, commands and the graphical editing workflow. |
| [CyCodeReader](https://github.com/MrMybal/CyCodeReader) | Standalone text/code editor using the lightweight native text profile. |

**Source transition:** `main` still contains the historical desktop source tree.
The published native Core source is on
[`codex/core-intelligence`](https://github.com/MrMybal/CyCode/tree/codex/core-intelligence); use that branch for the Core build below.
The old CyCode 0.2.x downloads belong to the earlier desktop application.

The Core runs without CyUI, SDL, ImGui, OpenGL or an IDE window. Applications
choose the services they need; the standalone text profile does not start
project or language workers.

## What you can do

- **Investigate code:** search workspace files and text, then query definitions,
  references, symbols and diagnostics through a configured language server.
- **Work with real projects:** use CMake, MSBuild/Visual Studio solutions and
  Unreal adapters with the project's actual toolchain and compiler context.
- **Review changes before applying them:** prepare revision-guarded edits,
  preview multi-file transactions, Apply/Undo and save explicitly. Recovery and
  disk-conflict checks preserve ownership of live documents.
- **Automate development services:** expose native build, DAP debugging,
  revision-control and plugin services to an application or an authorised agent.
  Capabilities depend on the installed tools and selected provider.
- **Embed selected components:** use `CyCode::Core` for project services,
  `CyCode::TextSession` / `TextBridge` for standalone text, or individual native
  foundations such as `Files`, `Snippets`, `Process`, `Transport` and `Protocol`.

Compilers, clangd/other language servers, debugger adapters and revision-control
clients remain external prerequisites. Native plugins execute trusted code.
Semantic coverage depends on real compiler commands and generated headers;
exhaustive Unreal coverage and Rider/CLion feature parity are not established.
Large-project scalability work and the full Unreal campaign are in progress.

---

## Installation

1. [Download CyCodeCore Setup for Windows x64](https://github.com/MrMybal/CyCode/releases/download/core-v0.1.1/CyCodeCore-0.1.1-windows-x64-setup.exe) and install it for the
   current user. No administrator account or system Python is required.
2. Optionally select **CyCodeUpdater**. It checks Core releases when opened and
   offers a verified Setup download followed by confirmed installation.
   Checking at Windows sign-in is a separate, initially unchecked option.
3. For a portable installation, [extract the complete ZIP](https://github.com/MrMybal/CyCode/releases/download/core-v0.1.1/CyCodeCore-0.1.1-windows-x64.zip).
   Keep its runtime, `app/`, tools, notices and documentation beside the engine.

The complete package includes the native CLI, app-local MSVC runtime, isolated
CPython/CyToolsCore/MCP, ripgrep, the `cycode-core` agent skill and the offline
English/French CyDoc reader. Packages are unsigned. The published distribution
targets Windows x64; Linux/macOS packaging and the latest changes need separate
validation. See the [installation and update guide](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreInstallation.md).

### Connect an AI agent

Setup can register an existing project. To register one manually, run from the
complete installation directory, replacing the example with your project:

```powershell
.\tools\python\python.exe -B app/scripts/install_core_agents.py --workspace "C:/Projects/MyProject" --access edit --enable
```

This installs the skill and configures the project's Codex MCP connection.
`--access edit` permits source edits and process execution; VCS, plugin and
settings writes remain separate. Reload the client's MCP connection after
registration. Other MCP clients can use the documented stdio command.
The [agent guide](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/Agents.md) explains discovery, jobs, permissions
and attaching to the IDE's shared live service.

## Source, build and integration

<details>
<summary>Build the native Core without graphical dependencies</summary>

Requirements: Git, Python 3.11+, CMake 3.24+ and a C++20 compiler. Windows project
builds need the matching Visual Studio/MSBuild tools. Source dependencies are
pinned; backend toolchains have their own requirements.

```sh
git clone --branch codex/core-intelligence https://github.com/MrMybal/CyCode.git
cd CyCode
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

Use the `core-v0.1.1` tag to reproduce the published preview's source.
Ordinary builds do not run native tests or launch an application. GitHub Actions
is paused. See [native integration](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreExtraction.md) for component
selection, embedding and the persistent stdio/loopback service. The source API is
not a stable C++ binary ABI.

</details>

## Documentation

The [English HTML manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview)
retains the CyDoc reader, search, images and language switching. The hosted
application manual includes historical desktop material; use the current
[Core installation guide](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreInstallation.md),
[native integration guide](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreExtraction.md) and
[method catalogue](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/MCPMethods.json) for the published engine.
The complete package's reader also opens locally through `file://`, without a server.

---

## License

**CyCode's original source, documentation and examples use [GNU GPL v3 only](LICENSE)
(`GPL-3.0-only`).** See [COPYRIGHT](COPYRIGHT). Dependencies retain their own terms.

### Native Core and optional SDK/runtime

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | [MIT](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/nlohmann-json.txt) | Native JSON models and protocols. |
| [TinyXML2](https://github.com/leethomason/tinyxml2) | [zlib](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/TinyXML2.txt) | XML project and provider data. |
| [Monocypher](https://monocypher.org/) | [BSD-2-Clause OR CC0-1.0](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/Monocypher.txt) | Cryptographic primitives and native plugin-package verification. |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | [BSD-3-Clause WITH PCRE2-exception](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/PCRE2.txt) | Regular-expression snippet transforms in the project profile. |
| [utf8proc](https://github.com/JuliaStrings/utf8proc) | [MIT + Unicode data terms](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/utf8proc.txt) | Unicode snippet casing. |
| [libvterm](https://www.leonerd.org.uk/code/libvterm/) | [MIT](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/libvterm.txt) | Terminal screen model in the project profile. |
| CyToolsCore SDK · private distribution | [MIT](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/CyToolsCore.txt) | SDK/MCP runtime, jobs, identities and permissions; supplied in the complete Windows package. |
| [MCP Python SDK](https://github.com/modelcontextprotocol/python-sdk) | [MIT](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/licenses/MCP-Python.txt) | MCP transport in the private Python runtime. |

The complete Windows package also retains CPython's PSF/component notices and
ripgrep's [MIT OR Unlicense terms](https://github.com/BurntSushi/ripgrep/blob/15.2.0/COPYING).
Optional language servers and adapters have separate licenses. The bundled
[CyDoc](https://github.com/MrMybal/CyDoc) reader uses MIT; its generation tools and
media notices are indexed in the [third-party notices](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/THIRD_PARTY_NOTICES.md).

[CyUI](https://github.com/MrMybal/CyUI) retains its [MIT license](app/licenses/CyUI.txt).
Graphical dependencies such as CyUI, Dear ImGui and SDL belong to the desktop
applications and historical source tree; they are not required by the native
Core build. The component table is an index, not a replacement for full notices.

## References

- [Published Core release and checksums](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1) · [All releases](https://github.com/MrMybal/CyCode/releases).
- [Native Core source](https://github.com/MrMybal/CyCode/tree/codex/core-intelligence) · [Issue tracker](https://github.com/MrMybal/CyCode/issues).
- [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) · [CyCodeReader](https://github.com/MrMybal/CyCodeReader).
- [English manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview) · [Manuel en français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview).

<details>
<summary><strong>AI Usage</strong></summary>

- [Start here: Core project instructions](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/AGENTS.md) — ownership and repository conventions.
- [Agent guide](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/Agents.md) — persistent startup, compiler context, MCP jobs and guarded changes.
- [Install and update](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreInstallation.md) — packages, skills and project registration.
- [Integrate the native library](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/Docs/CoreExtraction.md) — components, source APIs and validation scope.
- [Operation schemas](https://github.com/MrMybal/CyCode/blob/codex/core-intelligence/app/MCPMethods.json) — discover supported operations and their actual contracts.

</details>

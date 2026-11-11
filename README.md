<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://github.com/MrMybal/CyCode">GitHub</a><br>
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/en/overview">Documentation · application manual</a><br>
      <a href="app/Docs/CoreExtraction.md">Native integration guide</a><br><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCode Core" width="128"><br>
      <strong>CyCode Core</strong>
    </td>
    <td width="300" align="right" valign="top">
      <p><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p>Native source API <strong>1</strong><br>
      <strong>Development snapshot</strong></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCode Core</h1>

The shared native C++20 engine for CyCodeIDE, CyCodeReader and future code tools.
Core owns documents, project adapters, language analysis, diagnostics, native
plugins and guarded transactions. The CLI/service uses that same engine without
initializing a graphical interface.

CyCodeIDE owns windows, panels and CyUI presentation. Reader uses the lightweight
Core text profile for standalone documents and retains the shared graphical
editor. The historical 0.2.3 [application releases](https://github.com/MrMybal/CyCode/releases)
predate this separation; they are not a newly packaged Core or CyCodeIDE release.

## Standalone Windows preview

[CyCodeCore 0.1.1 Setup and portable ZIP](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1)
provide the native CLI, private Python/CyToolsCore MCP runtime, ripgrep and agent
skill. Setup offers CyCodeUpdater as a separate component, with optional sign-in
checks and confirmed installation of newer GitHub Core releases. Compilers and
language servers remain external prerequisites. Read the
[installation and update guide](app/Docs/CoreInstallation.md).

## Build without graphics

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

CMake 3.24+ and C++20 are required. Native fetch/configuration does not require
Reader, CyUI, SDL, ImGui, OpenGL or Tree-sitter. Ordinary builds never run an
application or native tests. Source revisions and third-party archives are pinned.

## Choose the native profile

| Profile | Targets and ownership |
| --- | --- |
| Project | CyCode::Core · Engine, CMake/MSBuild/Unreal, LSP, DAP, VCS, transactions |
| Standalone text | CyCode::Documents / TextSession / TextBridge |
| Native foundations | CyCode::Files / Snippets / Process / Transport / Protocol |

Text applications can select CYCODE_COMPONENTS and fetch only JSON with
`fetch_dependencies.py --components TextSession TextBridge Process`. The canonical
component resolver reuses targets and refuses mixed Core checkouts. Project and
text profiles share buffer/codecs/I/O primitives while retaining their path,
revision and transaction policies. Reader's native text profile does not start
project/language workers. Source API 1 is not a stable C++ binary ABI.

## Persistent native service

```sh
cycode_cli --disable-plugins <project> --stdio
```

```json
{"protocol":"cycode-core/1","id":1,"method":"service.status","params":{}}
```

One Engine and its backends persist across requests. Replies correlate IDs and
report structured failures. service.status separates lifecycle/context from
language.status backend readiness and revision-bound diagnostics. Shutdown/EOF
never saves dirty buffers. Existing loopback descriptor protocols remain
compatible. CyToolsCore continues to own SDK/MCP jobs, identities and permissions.

## AI agents and MCP

Standalone packages and the reusable AI skill: [Core installation](app/Docs/CoreInstallation.md).

Use [the agent guide](app/Docs/Agents.md) for persistent headless startup,
analysis preparation, asynchronous jobs and guarded changes. The generated
[MCP method catalogue](app/MCPMethods.json) lists all 239 native operations and
their complete schemas. The official CyToolsCore SDK owns MCP and permissions.

## Evidence and limits

Core 84e4c78 passed the 63-case native suite on Windows x64, Ubuntu x64 and macOS
arm64 (MSBuild is skipped on POSIX), including concurrent child ownership checks.
The corrected macOS Git replay also passed three additional repetitions. Reader
671c588 passed 17 service/view cases on those systems and its separate headless
profile. Exact CI links and tested limits are recorded in the integration guide.
The owner separately authorised production-only Core/clangd tests on real
UE 5.5.4: initial three-file rename, Apply/Undo, navigation cache invalidation and
official MCP workflows now pass. See [the scoped results](app/Docs/Benchmarks/unreal55-fixes-20261109.json).
Desktop acceptance is separate from this headless delivery; GitHub Actions remain paused.

The semantic baseline includes actual clangd requests against the real Core CMake
project (121 translation units). Raw samples, corpus identity, memory scope,
exact CI links and remaining work are in [the architecture guide](app/Docs/CoreExtraction.md).
Compilation, a path cache or a nonempty references response does not establish
whole-project semantic coverage. Exhaustive Unreal/real MSBuild acceptance, broader
invalidation, incremental synchronization and advanced transformations remain
further work. No CLion/IntelliJ parity is claimed.

## Applications and documentation

- [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) — desktop host; repository access may be required.
- [CyCodeReader](https://github.com/MrMybal/CyCodeReader) — standalone text/code editor.
- [CyUI](https://github.com/MrMybal/CyUI) — graphical presentation modules, outside the Core.
- [CyDoc](https://github.com/MrMybal/CyDoc) — the canonical bilingual HTML manual remains local/offline and is staged by the IDE.

See [LICENSE](LICENSE), [COPYRIGHT](COPYRIGHT) and
[dependency notices](app/THIRD_PARTY_NOTICES.md). Native project/text plugins keep
their existing C ABIs; they execute as trusted code and are not interchangeable.

[Core quality and reproducible validation](app/Docs/CoreQuality.md): shared atomic I/O, native C++ inspections and guarded compiler transformations.

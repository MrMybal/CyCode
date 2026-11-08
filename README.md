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

## Evidence and limits

Extracted Core 28c455b passed 63 native tests on each Windows/Linux/macOS runner
(MSBuild is Windows-only), with three additional macOS replay passes. Reader
9cbd8d3 passed seven native integration cases on all three systems. Later source
changes need their own acceptance. Local native execution stays suspended during
the existing antivirus investigation; source checks and production compilation
are reported separately.

The semantic baseline includes actual clangd requests against the real Core CMake
project (121 translation units). Raw samples, corpus identity, memory scope,
exact CI links and remaining work are in [the architecture guide](app/Docs/CoreExtraction.md).
Compilation, a path cache or a nonempty references response does not establish
whole-project semantic coverage. Large Unreal/MSBuild baselines, broader
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

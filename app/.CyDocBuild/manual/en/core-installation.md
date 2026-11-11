---
id: core-installation
title: Install standalone Core
description: Local Core packages, AI skill and project MCP registration.
section: API and extensions
parent: mcp
order: 262
revision: 5
---


## Distribution {#distribution}

CyCodeCore is a headless native service with the official CyToolsCore MCP bridge.
It does not need the IDE, CyUI, SDL, OpenGL, a GPU model or a system Python.
Standalone distribution 0.1.2 is versioned independently of the engine's
historical 0.2.3 contract version. This delivery is a development preview.
The native --version response reports the engine contract; the package and
CyCodeUpdater show the standalone distribution version.

The Windows x64 portable package contains `cycode_cli.exe`, its MSVC runtime,
verified ripgrep, isolated CPython/CyToolsCore/MCP, licences, exact method schemas
and the `cycode-core` skill. Compilers, clangd, build tools and other language
servers remain external prerequisites. Linking a Core does not install them.
The separate per-user Setup uses the same payload and can register a selected
project. No package is built by a GitHub workflow.

Version 0.1.1 uses independent sibling temporary names for atomic saves. The
production CLI has passed real CyToolsCore MCP search and clangd navigation on
CyCodeIDE, plus repeated synthetic document saves/checkpoints and revision guards
with Kaspersky active. These scoped Windows observations are not an antivirus
certification or exhaustive Unreal/platform acceptance. The packages are unsigned.

Version 0.1.2 packages the current 252-operation catalogue, shared native
Undo/Redo and commit receipts, paged text/recovery and the consumer-specific
update library. It is built from its exact sourceRevision and ships the same
verified Python/MCP bundle, ripgrep, licences and offline manual in Setup/ZIP.
Production compilation and package integrity are checked without launching the
Core, its MCP host or Setup. New runtime history/installer acceptance is not
claimed; the older scoped observations above retain their original provenance.
An installed/running 0.1.1 remains unchanged until the user upgrades and reconnects.

Download the [Windows Setup or portable ZIP](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.2).
The Setup installs per-user and offers the separate CyCodeUpdater component.

## Register a project {#connect}

After installing a **complete** distribution, run from its root:

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <project-directory> --access edit --enable
```

The script copies the skill to the user's Codex skills directory and registers
`cycode_core` in the project's `.codex/config.toml`. Trust that project in the
client and reload its MCP configuration. For another agent client, use the
equivalent stdio command from [Agents](Agents.md#configuration).

Without `--enable`, the registration stays disabled; this is useful to prepare an
agents-only installation. `--skill-only` installs only the instructions. `--status
--workspace <project-directory>` reads the registration without starting anything.
Use `--python` and `--skill-home` for an explicit compatible host/runtime location. `--config <client-config.toml>` can explicitly register this fixed project in a user-level client config instead.
The current redistributable runtime and Setup target Windows x64; Linux/macOS
source builds and client setup require separate native packaging validation.

Read access is the default. `--access edit` grants `workspace.write` and
`process.execute`, leaving VCS/plugin/settings writes separate. Native plugins
are disabled for this agent host. The script never starts Core, installs a
compiler or changes antivirus protection. Enabling a server lets the MCP client
start it on connection. Registration is not proof that it connected.

Existing client settings are parsed and retained. Only an issued managed block
is updated; changes to that block or skill are preserved for explicit review.
Config backups and connection receipts live in the installation's `agent-state`,
outside source control; do not publish this directory or endpoint descriptors.
The uninstaller does not erase personal skills, project settings or job data.
Before removing an installation, disable/remove its managed project connection.

A standalone agent host owns its own project buffers. To share the IDE's live
unsaved documents, attach to its existing private SDK service as described in
[Agents](Agents.md#connect), rather than assuming two hosts share state.

## Updating a live agent installation {#versioned-installation}

The installer remains the normal user-facing package. For a development update
while a Core is running, a verified portable package can be extracted to a new
versioned directory; keep the running installation intact. Register that exact
new `--home` and private `--python`, passing `--state-dir <previous-home>/agent-state`
to retain the original managed-config ownership receipt. The new home gets its
own receipt and independent job-data root; the active old SDK runtime keeps its
existing data. Later registrations can use the new home without --state-dir. Existing unrelated
client settings and modified skills are preserved. The managed server block must
still match its recorded hash. No running Core is terminated by registration.

Reload the client's MCP connection to use the new catalogue; the existing process
keeps its old binary until disconnected. Check the actual operation catalogue and
a compiler-backed request after reconnecting. Source checkout, installed package,
client catalogue and running process are separate versions.

## Optional update companion {#updates}

CyCodeUpdater is a separate Windows application offered by Setup. Its Start menu
shortcut checks the Core GitHub releases when opened, shows the installed version
and release notes, and offers a verified Setup download followed by installation.
The installer opens only after confirmation; close Core/MCP clients first.
The update check does not start or silently replace the Core.

Setup separately offers an initially unchecked option to check at Windows sign-in.
That mode checks at most once per day, proposes the updater for a newer version,
and exits when there is nothing to offer. There is no permanent Windows service.
Installing Core without the companion remains supported.

The companion accepts `--language fr`, `--check` (JSON-only manual check),
and `--background` (daily notification). For a portable package, launch it with
`tools/python/pythonw.exe -B apps/CyCodeUpdater/main.py`; portable ZIP replacement
remains manual. The companion's installation offer uses the normal Windows Setup.

Only `core-v` releases with the matching Windows Setup and SHA256SUMS.txt are used;
legacy IDE releases, drafts and incomplete releases are ignored. Downloads are
bounded and checked against SHA-256, the advertised size and GitHub's asset digest
when available. Private repository access uses an existing noninteractive GitHub
Git credential or the CYCODE_GITHUB_TOKEN / GH_TOKEN / GITHUB_TOKEN environment
variable. No credential is shipped or saved by the updater. Network/authentication
failures preserve the installed version. The preview channel includes preview
Core releases; it does not certify their feature or platform completeness.

## Build packages locally {#package}

Build native Core separately with tests off. Package an existing, accepted binary
without executing it:

```text
python app/scripts/package_core.py --revision <exact-clean-source-commit> --build <production-build-directory> --executable <production-build-directory>/app/Release/cycode_cli.exe --mcp-bundle <verified-runtime-bundle> --output <new-output-directory> --installer
```

This uses `app/mcp-runtime.lock.json` and the existing verified bundle reader.
Inno Setup 6.7+ is needed for `--installer`. No GUI application or legacy updater
is required. The ZIP and Setup have their own CyCodeCore identity, separate from
historical CyCode application packages. SHA256SUMS.txt accompanies the output.

Preparation without a native binary must be explicit:

```text
python app/scripts/package_core.py --revision <exact-source-commit> --mcp-bundle <verified-runtime-bundle> --output <new-output-directory> --agents-only
```

The archive manifest records `state: agents-only`. Passing `--installer` in this
mode is refused. Missing/changed inputs fail without rebuilding, substitution or
downloads. Publish only complete accepted packages as a runnable release;
preparation artifacts may accompany a clearly marked draft while acceptance is
blocked. Never include project caches, credentials, private job data or settings.

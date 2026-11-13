# Standalone Core and AI agents

## Distribution {#distribution}

CyCodeCore is a headless native service with the official CyToolsCore MCP bridge.
It does not need the IDE, CyUI, SDL, OpenGL, a GPU model or a system Python.
Standalone distribution 0.1.4 is versioned independently of the engine's
historical 0.2.3 contract version. This delivery is a development preview.
The native --version response reports the engine contract; the package and
CyCodeCore Manager shows the standalone distribution version.

The Windows x64 portable package contains `cycode_cli.exe`, its MSVC runtime,
verified ripgrep, isolated CPython/CyToolsCore/MCP, licences, exact method schemas
and the `cycode-core` skill. Compilers, build tools and other language servers
remain prerequisites. The current source Manager adds reviewed private clangd
installation under `<CyCodeCore>/dependencies/clangd/`, without changing Windows
PATH or replacing LLVM. See [dependencies](CoreManager.md#dependencies) for sizes
and semantics. Older published packages require an upgrade to gain this feature.
The per-user Setup uses the same payload in a fresh versioned directory.
Agent/skill configuration is performed separately in CyCodeCore Manager. No package is built by a GitHub workflow.

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

Version 0.1.3 adds per-message validation of language-server responses, a shared
line index and one-pass text-edit assembly, and common Git safety options for
repository operations. Git status no longer inherits a repository fsmonitor
program; diff/show disable external text conversion. These changes retain the
252-operation catalogue and existing engine contract. Production compilation and
package/data checks do not establish new native runtime or installer acceptance.

Download the [Windows Setup or portable ZIP](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.4).
Setup installs per-user and offers the CyCodeCore Manager shortcut. Version 0.1.4
adds multi-client skill/MCP configuration and verified activation of fresh payloads;
running engines and their Python runtimes are never overwritten.

## Register a project {#connect}

Open **CyCodeCore Manager > Agents and skills**. Select applications and the
personal/project scope, choose skill and/or MCP, review **Preview changes**, then
**Apply**. A personal skill alone needs no project; MCP always binds one existing
project. Read-only is the default. Editing/builds must be selected explicitly;
VCS/plugin/settings writes remain separate and native plugins stay disabled.

The [manager guide](CoreManager.md#agents) lists client-specific paths and formats.
Other settings, changed skills and unowned connections are preserved. Private
backups/receipts remain in local application data, outside source/release files.
Reload and trust the connection in the client. Configuration is not evidence of
a loaded skill, running server or successful compiler call.

For legacy scripted Codex registration, run from the selected payload directory:

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <project-directory> --access edit --enable
```

This compatibility command still uses project `.codex/config.toml`, a managed
block, skill ownership guards and `agent-state` receipts. Without `--enable` the
connection stays disabled. `--skill-only`, `--status`, `--python`, `--skill-home`,
`--config` and `--state-dir` remain available for explicit scripted integrations.
No registration command starts Core or installs a compiler.

Two standalone hosts do not share unsaved buffers. Attach to the IDE's existing
private SDK service when its live documents are required; see [Agents](Agents.md#connect).

## Updating a live agent installation {#versioned-installation}

Setup installs under `versions/<version>-<generation>` and verifies the complete
inventory before atomically selecting `current.json`. It normalises the older
version-directory destination back to its root and retains all older payloads.
No running Core, Python/MCP host or manager is killed or overwritten. Reinstalling
the same version also uses a fresh generation; incomplete activation keeps the
previous selection. See [versioned updates](CoreManager.md#updates).

The manager can refresh unchanged owned client configurations after installation,
preserving permissions, disabled state and locally modified skills. Legacy issued
Codex receipts are discoverable. Configuration conflicts are reported separately
from installation success. Each new host invocation has its own SDK job-data root.

Save/checkpoint unsaved edits before reloading MCP clients. Existing processes
keep their old binary and buffers until closed. Source revision, selected payload,
client configuration and running process are distinct states.

## Core manager and update checks {#updates}

CyCodeCore Manager replaces CyCodeUpdater and combines agents/skills with updates.
It checks once at startup and on manual request, shows release notes and offers
the verified Setup. An initially unchecked Setup task enables a daily Windows
sign-in check. No permanent polling service is installed.

Only complete `core-v` releases with matching Setup/checksums are accepted. Size,
SHA-256 and GitHub digest are checked. The manager waits for the visible installer
and verifies that the requested new payload was actually selected before reporting
success. It can then refresh owned connections and reopen its new version.

Private repositories use existing noninteractive Git credentials or the
CYCODE_GITHUB_TOKEN / GH_TOKEN / GITHUB_TOKEN environment variable. No credential
is shipped or saved. Network/authentication failures are not reported as up to date.
The preview channel accepts Core previews without certifying their completeness.

The GUI supports EN/FR and a language button. `--check` prints JSON; `--list` reads
managed connections; `--background` checks for a daily offer. In a portable ZIP,
open `CyCodeManager.cmd`. The old `apps/CyCodeUpdater/main.py` entry delegates to the
manager's Updates view. See [manager workflows and limits](CoreManager.md).

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

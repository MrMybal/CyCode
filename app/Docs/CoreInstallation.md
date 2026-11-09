# Standalone Core and AI agents

## Distribution {#distribution}

CyCodeCore is a headless native service with the official CyToolsCore MCP bridge.
It does not need the IDE, CyUI, SDL, OpenGL, a GPU model or a system Python.
The standalone distribution version starts at 0.1.0 independently of the engine's
historical 0.2.3 contract version. This first delivery is a development preview.

The Windows x64 portable package contains `cycode_cli.exe`, its MSVC runtime,
verified ripgrep, isolated CPython/CyToolsCore/MCP, licences, exact method schemas
and the `cycode-core` skill. Compilers, clangd, build tools and other language
servers remain external prerequisites. Linking a Core does not install them.
The separate per-user Setup uses the same payload and can register a selected
project. No package is built by a GitHub workflow.

**Current delivery limitation:** a controlled production build and installation
were explicitly authorised after the first antivirus incident. The installed
Core passed actual file/text search, MSBuild evaluation, compiler definition and
references, and authored the IDE integration guide through MCP. Kaspersky System
Watcher then deleted that installed CLI at 19:20 Europe/Paris on 2026-11-09.
Its MD5 matches the built payload. MCP has been disabled, no process remains,
and no automatic rebuild/restore/retry follows this second detection. A false
positive has not been established. This is **not a passing full installed-MCP
acceptance**, and binary publication is suspended. Only an explicitly labelled
`agents-only.zip` is currently suitable for a draft: it contains the MCP runtime
and instructions, **without native Core**. It cannot enable the server or be
presented as a working Core installer. See [the evidence](WindowsSecurity.md).

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

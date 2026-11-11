# Local connection

The standalone installation contains the real CyToolsCore MCP adapter, its
isolated Python runtime, native Core, operation catalogue and this skill. It
does not need a CyCodeIDE window. `CYCODE_CORE_HOME` identifies the installation;
`app/Docs/Agents.md` and `app/MCPMethods.json` there describe the exact contracts.

From that installation, register a trusted project without starting the host:

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <project> --access edit --enable
```

The command installs the skill and adds only a managed `cycode_core` MCP block
to that project's `.codex/config.toml`. Read access is the default; edit adds
workspace writes and process execution, not VCS/plugin/settings writes. Native
plugins are disabled. Existing unrelated client settings are preserved. A
modified existing managed block or skill is retained and requires review.
An explicit `--config <client-config.toml>` registers this fixed project in a
user-level client configuration instead; unrelated servers/settings are retained.
The client must trust the project and reload its MCP configuration before tools
are available. Do not claim tool availability from configuration files alone.

`--skill-only` installs only the instructions. Without `--enable`, registration
keeps MCP disabled and can be prepared while the native executable is absent.
This is explicitly an incomplete runtime installation. Check `--status
--workspace <project>` before diagnosing a connection. No installer command
downloads a compiler, starts the Core or changes antivirus settings.

For the IDE's actual unsaved buffers, use its private shared-service connection
as documented in `app/Docs/Agents.md`, rather than starting a second Engine.
Never copy endpoint credentials into a skill, source repository or report.

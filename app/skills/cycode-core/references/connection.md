# Local connection

Current sources compile the pinned CyMCPSDK registry/CLI/MCP natively. No chat,
Python MCP runtime or CyToolsCore is needed. Manager/old launchers use only Python
stdlib. Published packages predating this migration may still use the old SDK.
`CYCODE_CORE_HOME` selects the trusted installation; read its
`app/Docs/NativeAutomation.md` and `app/MCPMethods.json` for the actual version.

```text
cycode_cli --sdk --workspace <project> --connection-state <private-state> --disable-plugins mcp
```

This advertises five compact gateway tools by default. Current source builds default
to Free (`CYCODE_MCP_ACCESS_PROFILE=free`): an HTTP client connects by address alone
and may call every registered operation. Controlled profiles retain host permissions
(`CYCODE_PERMISSIONS`); explicit legacy read/edit choices are preserved. For an existing live
Engine, use `--native-descriptor FILE`; `--bridge-config FILE --access read|edit`
connects to the shared HTTP service. Protected and Token only require credentials.
Do not publish credentials. Native argument, revision and file guards still apply.

From that installation, register a trusted project without starting the host:

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <project> --access edit --enable
```

The command installs the skill and adds only a managed `cycode_core` MCP block
to that project's `.codex/config.toml`. No access choice selects Free; explicit read
selects Local controlled, and explicit edit adds
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

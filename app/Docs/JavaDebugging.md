# Java debugging

The built-in `builtin.java` provider connects the existing native Debugger to
Microsoft's Java Debug Server add-on for Eclipse JDT LS. Launch and JDWP attach
share the native debugger. Windows production compilation and
schema/source verification are recorded in [Validation](Validation.md); actual
Java/DAP and graphical acceptance remain pending while native runs are suspended.

## Prerequisites

Install a compatible JDK, JDT LS and the Microsoft java-debug plugin JAR separately.
CyCode does not download, build, install or redistribute that JAR. Keep its
separate EPL licence and notices when supplying it. Resolve the application JDK
through PATH or `CYCODE_JAVA`; standalone builds also need `CYCODE_JAVAC`/javac.

Enable Java in language settings. For the installed default server, set
`CYCODE_JAVA_DEBUG` to the absolute plugin `.jar` path **before starting CyCode**.
The default Java settings add that file to `$initializationOptions.bundles`.
This lookup reads paths only. An explicitly configured server/settings profile
remains authoritative: edit its existing JSON and include, for example:

```json
{
  "$initializationOptions": {
    "bundles": ["/absolute/path/com.microsoft.java.debug.plugin-version.jar"]
  }
}
```

Merge this into the existing initialization options; retain other settings.
Save/remember the Java profile and restart its backend. Merely changing the
environment after startup does not replace a remembered profile. Java server
configuration is shared by desktop, MCP and the native plugin host.

The add-on integration follows the upstream
[java-debug usage contract](https://github.com/microsoft/java-debug#usage-with-eclipsejdtls)
and [Java launch fields](https://github.com/microsoft/vscode-java-debug/blob/main/package.json).
CyCode checks for the advertised `vscode.java.startDebugSession` command. Missing,
disabled, incompatible or unresponsive servers fail explicitly. No adapter,
compiler or server starts merely by listing adapters or saving a debug profile.

## Build and launch

Select a standalone Java, Maven or Gradle project and its main class in Project
tasks, then Debug / Continue (F5). `project.debug` uses the existing cancellable
build owner, selects one actual class-file main method and returns `debugProfile`.
Maven resolves runtime dependencies for Debug as it already does for Run; Gradle
uses its exported runtime model. This operation prepares a profile without
starting a debugger. The GUI applies its main class, runtime classpaths, Java
executable, working directory and arguments, preserving optional Java VM/source
settings, then uses `debug.configure` and `debug.launch`. Cargo retains its native
executable output and switches back to the native provider. Unsaved buffers must
be saved before building/debugging.

For SDK/plugin clients, take `debugProfile` from a successful `project.debug`
result, configure it, then launch. Check success/cancellation before proceeding.
The existing operation permissions and host service remain in force; no second
Python debugger model or plugin ABI was added. All 170 operations remain present.

Manual profiles select **Java / JDT LS** under Adapter provider. Adapter options
are a bounded JSON object with these supported fields:

| Field | Meaning |
|---|---|
| mainClass | Fully qualified class name, required at launch; up to 4096 bytes |
| classPaths | Explicit runtime directories/JARs, required and nonempty at launch |
| javaExec | Optional actual Java executable path; Project tasks supplies it |
| projectName | Optional JDT project identity to disambiguate sources |
| sourcePaths | Optional additional source roots, with standalone source root supplied by Project tasks |
| vmArgs | Literal JVM arguments as an array |

Each array is limited to 128 entries, each string to 8 KiB, and options to 16 KiB
overall. Unknown/common transport/request fields are rejected. Main class names
cannot be source paths, shell text or contain ASCII whitespace/control characters.
Classpath/source entries are literal paths resolved relative to the working
directory and canonicalized before the request. Additional source roots must
exist. Missing classpath directories (for example Gradle resources with no files)
stay in the array, matching JVM behavior. `$Auto`, wildcards,
variables and JPMS module-path inference are not expanded. Large dependency graphs
over these limits require further work; entries are never silently dropped.

Arguments, environment, cwd, stop-on-entry and terminal selection remain common
profile fields, rather than duplicated Java options. The host sends DAP type
`java`, request `launch`, `noDebug=false` and the integrated terminal or internal
debug console. External terminals are rejected. Run without debugging uses the
existing Project tasks Run command; `debug.run` remains native/.NET.

The host converts argument arrays to the Java server's actual string fields,
using its platform-specific quoting rules, without a shell. Empty strings,
spaces, quotes and trailing backslashes retain their intent. Environment removals
require the integrated terminal: the adapter's internal-console environment path
does not provide the same removal semantics. See the upstream
[wire fields](https://github.com/microsoft/java-debug/blob/main/com.microsoft.java.debug.core/src/main/java/com/microsoft/java/debug/core/protocol/Requests.java)
and [argument parser](https://github.com/microsoft/java-debug/blob/main/com.microsoft.java.debug.core/src/main/java/com/microsoft/java/debug/core/DebugUtility.java).

## Attach to an existing JVM

Start the target with JDWP enabled, using the target project's own launch method.
For example, the following JDK command listens locally and waits for the debugger
before executing the main class (illustrative only, not executed by validation):

```text
java -agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=127.0.0.1:5005 -cp out demo.Main
```

Select **Java / JDT LS** in the Debugger's Launch configuration tab. Set **JVM
host**, **JDWP port** and **Attach timeout (ms)**, then **Attach to JVM**. The
same configured JDT LS/java-debug add-on used for launch is required. The local
adapter connects to the existing JVM; CyCode does not compile a project, launch
that JVM or create a program terminal. Existing source roots and optional
`projectName` come from Adapter options; mainClass, classPaths, javaExec, vmArgs,
program arguments/environment and stop-on-entry are not sent in attach requests.
Source paths still resolve against the guarded workspace working directory.

**Save debug profile** persists the endpoint independently of launch options in
`.cycode/launch.json`. Per-call attach overrides do not implicitly save it.
Existing profiles without an endpoint retain 127.0.0.1:5005 / 5000 ms. Successful
project Debug target preparation preserves this endpoint while replacing its
launch class/runtime options. Profiles and current debug status use the same
typed `javaAttach` object:

```json
{
  "adapterId": "builtin.java",
  "javaAttach": {
    "hostName": "127.0.0.1",
    "port": 5005,
    "timeout": 5000
  }
}
```

For SDK/MCP/plugin host `debug.attach`, explicitly select `adapterId=builtin.java`
and pass this endpoint, or omit `javaAttach` to use the saved/default endpoint.
An empty `adapterOptions` is sufficient for attach and replaces any unrelated
saved vendor options. Java rejects `pid`; native/.NET and contributed adapters
continue to require a positive PID. Typed endpoint fields are reserved host
configuration and cannot be smuggled through vendor `adapterOptions`. The existing
process.execute permission remains required; saving also requires workspace.write.

The host is bounded to 253 ASCII characters and accepts hostnames, IPv4 and
unbracketed IPv6 addresses with optional scope IDs. Port is 1–65535; timeout is
100–10000 **milliseconds**, within the common DAP response deadline. URLs, combined
hostname:port values, spaces, control characters and unknown fields are rejected.
Host/IP validity, DNS resolution, routing and reachability are determined by the
installed Java adapter/JDK, rather than probed while editing or inspecting a
profile. Unicode hostnames require their ASCII/punycode form. Remote endpoints
are reached by the local JDT LS process; its DAP connection with CyCode stays on
loopback. Target JDWP availability, networking or an existing tunnel are supplied
by the user; CyCode does not provision them or search for targets automatically.

After attach, the shared breakpoint, thread, stack, scope, watch and stepping
views apply. The panel shows the session's actual requested target separately
from the editable profile draft, and identifies Java rather than C/C++. Stop,
shutdown or backend reconfiguration sends disconnect with
`terminateDebuggee=false` / `suspendDebuggee=false`. The adapter detaches from the
existing JVM. A Java attach session refuses reverse runInTerminal requests and
never takes ownership of the existing target process. Runtime acceptance of
detach/resume, network failures and reconfiguration remains pending.
Other owners retain their own lifetimes: workspace-owned Run terminals, for
example, still close their processes when that workspace shuts down.

See the upstream [Java attach handler](https://github.com/microsoft/java-debug/blob/main/com.microsoft.java.debug.core/src/main/java/com/microsoft/java/debug/core/adapter/handler/AttachRequestHandler.java),
[disconnect behavior](https://github.com/microsoft/java-debug/blob/main/com.microsoft.java.debug.core/src/main/java/com/microsoft/java/debug/core/adapter/handler/DisconnectRequestHandler.java)
and [Oracle JPDA invocation guide](https://docs.oracle.com/en/java/javase/25/docs/specs/jpda/conninv.html).

## Ownership and boundaries

The Java language-service owner issues the fixed LSP command with a ten-second
request deadline. The Debugger waits without holding its state mutex, checks the
backend generation and validates the returned integer port (1–65535). Its framed
JSON transport connects only to IPv4 127.0.0.1, without DNS, configurable remote
addresses, a listener, shell, bridge process or separate JVM debug implementation.
Connect is bounded to two seconds; socket writes to five seconds. Existing DAP
message/queue limits, stop generations and inspection capability checks apply.
Changing/disabling the Java backend invalidates the active session. Disconnect
asks the adapter to terminate the launched debuggee and closes owned program
terminals; the shared JDT LS backend remains under LanguageService ownership.

Breakpoints, stepping, threads, stack, scopes, watches, sources, exception filters
and available inspection commands reuse the same Debugger/UI services. Their
actual availability depends on the installed Java adapter and debug symbols;
standalone Debug uses javac `-g`, while Release omits symbols. Maven/Gradle retain
their own project compiler settings. Default JDT imports/autobuild remain disabled;
configure project import/source roots explicitly when source resolution needs it.

Local/remote JVM attach is implemented with pending runtime acceptance.
Manual Hot Code Replace is also implemented as described below. JPMS/Android, child sessions,
automatic project import/build synchronization and Java-specific exception UI
remain separate work, along with virtual JDT/JAR-source navigation beyond the
shared file/source-reference views. There is no claim of tested Java/Rider parity, GUI/theme/DPI
acceptance or Linux/macOS execution from a production Windows build.

## Manual Hot Code Replace

Run > **Apply Java code changes**, the Java debugger toolbar and the command
palette share `debug.hotReload`. This action saves desktop buffers first, then
requests an **incremental JDT workspace build** (`java/buildWorkspace` with the
boolean `false`, encoded as JSON-RPC positional `params: [false]`). It never launches javac/Maven/Gradle as a substitute for that
build. The JDT result must be ordinal 1 (success); failed, cancelled, erroneous or
malformed results prevent sending `redefineClasses`.

The debuggee must use the same project/source and class output mapping as the
imported JDT model. Configure the needed JDT project import and source/output
roots explicitly, including `adapterOptions.projectName` when needed. The normal
task-generated standalone javac directory is **not automatically synchronized**
with a JDT project's output. Merely opening a Java file, compiling externally or
having syntax coloring does not establish this mapping. Defaults still disable
JDT Maven/Gradle import and autobuild. This action does not enable them, install
tools, download dependencies or change the saved project build profile. An
explicit JDT build can execute that imported project's configured builders.

`debug_hot_reload` takes `sessionGeneration` from the current `debug_status`
snapshot and an optional unique `operationId` (ASCII letters/digits/underscore/
hyphen/dot, at most 128 characters). The generation identifies the active DAP
session and rejects queued requests from earlier launch/attach sessions. It is
zero while disconnected/tearing down, and a new nonzero value is issued only
when starting a new session. The
native owner also checks Java backend epochs and the saved/conflict-free open
workspace document identity/revision snapshot before and after compilation.
Editing/replacing a captured native document or leaving it dirty during the build
requires a retry. This is not an atomic external filesystem snapshot: save and resolve
external changes and keep the debuggee/JDT source mapping consistent.

`javaReload` in debugger status reports `building`, `applying`, `applied`,
`unchanged`, `failed` or `cancelled`, with `operationId`, `changedClasses` and
`message`. A successful DAP envelope containing `errorMessage` is still a
failure. An empty class list is **unchanged**, never a successful application of
changes. The reply is bounded to 256 KiB and 2048 class names of at most 4096
bytes. The native view lists the actual returned names. Adapter ERROR/WARNING
diagnostics remain visible separately; STARTING/END/BUILD_COMPLETE notifications
do not themselves establish successful replacement and never trigger automatic
application. No invented standard DAP HCR capability is assumed; an older or
incompatible add-on's request rejection remains an explicit error.

**Cancel Java reload** signals the pending compilation through
`debug.cancelReload`; SDK `debug_cancel_reload` can match the same `operationId`,
including a reload still queued in the native debugger. The SDK's own job
cancellation sends this matched signal and retains its reservation until the
native call returns. Cancellation is checked before application, with a 120 s
build deadline and JDT request cancellation. The callback remains pending until
JDT responds or that deadline expires; `cancelRequested` identifies this wait in
status/the desktop. The compiler can finish before acknowledging cancellation,
but its result will not trigger replacement. If a server ignores cancellation or
the deadline expires, its compiler may still be active; CyCode does not claim to
terminate it or kill the shared language server. A Stop request also interrupts that
pre-application wait and then performs normal launch/attach disconnect semantics.
Cancellation never stops the JDT language server or an attached JVM. Once
`redefineClasses` has been sent, the 30 s adapter wait runs to an outcome or
timeout; cancellation cannot undo a VM change. Failed, interrupted or timed-out
application may have partially applied changes; inspect status and restart the
session explicitly when needed. There is no automatic restart or Undo operation.

Reload attempts and Java stack invalidation events expire cached frame/scope/
variable/source-reference generations. Actual continued/stopped events retain
execution state, and paused inspection refreshes asynchronously. Existing native,
.NET and contributed stdio DAP adapters do not accept this Java-specific action.

On a standard JVM HCR primarily supports method body changes; adding/removing
members or changing signatures may require a JVM restart. The installed VM and
adapter remain authoritative. See the upstream
[HCR handler and reply semantics](https://github.com/microsoft/java-debug/blob/main/com.microsoft.java.debug.core/src/main/java/com/microsoft/java/debug/core/adapter/handler/HotCodeReplaceHandler.java),
[JDT build protocol](https://github.com/redhat-developer/vscode-java/blob/master/src/protocol.ts),
[JSON-RPC positional parameter encoding](https://github.com/microsoft/vscode-languageserver-node/blob/main/jsonrpc/src/common/connection.ts),
[build implementation](https://github.com/eclipse-jdtls/eclipse.jdt.ls/blob/main/org.eclipse.jdt.ls.core/src/org/eclipse/jdt/ls/core/internal/handlers/BuildWorkspaceHandler.java)
and [VM limitations](https://github.com/microsoft/vscode-java-debug/wiki/Hot-Code-Replace).
Windows compilation/schema checks do not establish actual JVM reload acceptance;
real build/redefine/partial failure/cancellation, remote attach mapping, graphical
interaction and Linux/macOS remain unexecuted while native tests are suspended.

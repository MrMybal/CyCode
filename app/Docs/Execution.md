# Run, terminal and debugging

The desktop and MCP use the same native Terminals and Debugger services. Windows
x64 has been exercised with actual PowerShell/cmd, a C++ PDB executable, CodeLLDB,
and a .NET 8 console assembly with portable PDBs through NetCoreDbg,
the official MCP SDK and real GUI key input. Linux/macOS implementations and pinned
adapter archives are present; their execution remains unverified.

## Terminal

Alt+F12 opens/focuses the terminal. Its toolbar creates shells, switches between up
to eight sessions, clears scrollback or closes the selected session/process tree.
Closing the docked panel hides it; the session stays alive. Panels can be detached
into native windows using the existing docking system. The normal Windows profile
uses installed PowerShell (pwsh when found, otherwise Windows PowerShell); cmd is
also available. POSIX profiles use the user's shell, bash or zsh. The startup option
`--terminal-profile cmd` selects the profile for subsequently created terminals.

These are real PTYs: ConPTY on Windows and forkpty on POSIX. libvterm 0.3.3 handles
UTF-8, cell widths, ANSI/VT state, alternate screens, cursor/device queries, paste and
resize/reflow. Read/write threads and a process supervisor keep synchronous PTY I/O
off the UI thread. ConPTY output continues draining during teardown. Only visible
rows are copied for GUI rendering. Standard terminal colours follow the active theme
and foreground contrast is adjusted; explicit 256/RGB colours remain terminal data.

The focused terminal owns shell editing keys, Ctrl+C, Ctrl+F and Tab. Ctrl+Shift+C
copies a mouse selection; Ctrl+Shift+V pastes with bracketed-paste support. Paste is
capped at 64 KiB and rejects escape bytes; raw authorized MCP input can send controls.
Scrollback is bounded to 2,000 lines and 200,000 cells per session; input queues are
bounded to 256 KiB. Terminal snapshots use display-cell coordinates, not UTF-16.
The renderer supports bold, underline, strike and reverse video; italic style is
retained in native cells but does not select a separate italic font. Full mouse
reporting, terminal tabs with separate split panes, shell integration and IME/font
coverage on other platforms need broader work/validation.

## Run and debug

Run > Run without debugging launches the selected executable or .NET assembly with argv/env in a
native terminal. A persistent profile is stored in .cycode/launch.json. The Debugger
panel edits executable, working directory, arguments (one argv element per line),
environment, runtime (automatic/native/.NET), .NET host, adapter and stop-on-entry. Environment entries are KEY=VALUE; a line
`-KEY` removes an inherited variable. Working directories remain within the workspace;
an explicitly selected executable can be external, including UnrealEditor.

Build before debugging saves modified documents and runs the configured CMake,
MSBuild or Unreal build before launching. Build failure prevents launch. CMake target artifacts
can populate the executable selection. For discovered Unreal projects, the installed
UnrealEditor executable/project arguments are suggested. UnrealEditor debugging
itself has not been exercised; the executed debugger evidence is an owned C++ fixture.

The adapter speaks the actual Debug Adapter Protocol over private child-process
stdio. One owner thread handles requests/events and refreshes stopped-thread stack,
scopes, locals and watches. The framed JSON child transport is reused from the
language client without sending LSP initialization. Requests have a 15-second deadline;
commands, incoming messages, output and variable pages are bounded. MCP cancellation
waits for native work to finish; immediate cancellation/streaming is not provided.

Click the editor gutter or use Toggle breakpoint. Breakpoints persist and include
adapter-verified locations. The Breakpoints tab supports conditions, hit conditions,
log messages and enabled flags; an expected-breakpoint snapshot rejects conflicting
edits. Stack/thread selection refreshes variables and jumps to source. Registered SDK
sources open read-only. Adapter-issued disassembly/source references open as separate
read-only debug:// views and are valid only for the current debug session.

The Variables tab expands children, bounded to an initial 200-item page per reference
and eight display levels. Additional collection-page controls are pending; MCP can
request further pages. Watch expressions persist (up to 32), are reevaluated on stops
and report individual failures. Evaluate uses the selected frame. Expression/debugger
commands may have side effects in the target. No synthetic variable values are used.

Stop terminates a launched debuggee and its owned terminal. Attach takes an explicit
process ID; stopping that session detaches and leaves the existing process running.
Native acceptance verifies this distinction. Process ownership is tied to native
shutdown/workspace changes. Always use matching symbols and sufficiently unoptimized
code. MSVC incremental-link thunks can produce disassembly-only stops; the source-level
fixture disables incremental linking. Remote/core-dump workflows, data/function/exception
breakpoint UIs, memory/register/disassembly tooling, reverse debugging, and full Unreal
visualizers/Live Coding integration remain pending or unverified.

## C#/.NET runtime

`runtime=automatic` detects PE/CLI assemblies and native apphosts with a matching
managed DLL/runtimeconfig. The managed backend uses NetCoreDbg. MSBuild evaluated
artifacts offer runnable managed DLLs as well as executables; class libraries without
a runtimeconfig are not offered as runnable targets. Select the actual built output,
with matching portable PDBs. Automatic .NET host discovery probes installed runtimes
and the private analysis host; an explicit host can be selected. Missing target
framework runtimes produce an error rather than silently enabling major roll-forward.
Standalone .NET 8 console debugging is validated; .NET Framework, Mono, Unity,
other target frameworks, ASP.NET/browser and remote workflows remain unverified.

The same source markers, stack/threads, locals, object/array children, watches,
evaluation, step into/over/out, pause and attach/detach are used for C# and C++.
Inspection identifiers are cleared on resume/stop/relaunch; stop generations stay
monotonic across sessions. Deferred managed breakpoint verification updates markers.
NetCoreDbg supports conditional breakpoints; it does not advertise logpoints or hit
counts. Unsupported enabled options are rejected before changing a live saved list.
Unverified, disabled or unbound breakpoints remain visibly distinct.

Managed program output appears in the Debug console. NetCoreDbg does not implement
runInTerminal or debuggee stdin; use Run without debugging for interactive console
programs. UTF-8 program output is tested (the console fixture selects UTF8Encoding);
other Windows output encodings and environment values outside the active Windows
code page need broader adapter coverage. Unicode paths, accented environment values
and arguments containing spaces, embedded quotes and shell punctuation are tested.
The Windows adapter's argv builder drops empty arguments; CyCode rejects those and
unrepresentable quote/tab-plus-trailing-backslash cases with a clear error. Native
terminal Run preserves ordinary argv directly. Advanced `adapterArgs` persist in
launch.json/MCP as separate argv elements, not a shell command; leave logging off
for normal use. Environment overrides/removals are applied to the adapter's native
environment to avoid NetCoreDbg's JSON-to-ANSI override corruption on Windows.

## Keyboard profiles

| Action | Rider | Visual Studio / VS Code |
|---|---|---|
| Debug / Continue | Shift+F9, F5, F9 | F5 |
| Run | Shift+F10 | Ctrl+F5 |
| Toggle breakpoint | Ctrl+F8 | F9 |
| Step over | F8 | F10 |
| Step into | F7 | F11 |
| Step out | Shift+F8 | Shift+F11 |
| Stop | Ctrl+F2 | Shift+F5 |
| Terminal | Alt+F12 | Alt+F12 |

Keyboard settings can override these bindings. A focused PTY owns its function keys;
focus the editor/debugger before invoking debug function-key bindings.

## Dependencies and reproduction

libvterm is pinned in dependencies.lock.json and fetched for GUI/core builds. The
GUI build runs setup_debugger.py, installing official CodeLLDB 1.12.3 and its bundled
LLDB 22.1.8 under runtime/tools. Its platform archives/digests are locked in
debugger-tools.lock.json. This avoids relying on an installed LLVM adapter whose
Python DLL dependencies may be absent. For native debugging, `automatic` prefers bundled CodeLLDB,
then an installed lldb-dap; a custom adapter path can be selected. Only the bundled
CodeLLDB backend was exercised for native debugging. setup_debugger.py also installs
official NetCoreDbg 3.2.0-1092 for managed debugging, with platform SHA-256 hashes in
the same lock file. Upstream provides Windows x64, Linux x64/arm64 and macOS arm64
archives for this release. Other architectures need an installed adapter; only
Windows x64 is validated here. Dependency licenses remain in app/licenses;
CyCode's original source is licensed under GPL-3.0-only (see the root LICENSE).

The native `terminal` and `debugger` CTests use owned fixtures. The debugger test
reports a skip if no adapter is installed in a core-only environment. GUI scripts:

```text
python app/scripts/verify_terminal.py
python app/scripts/verify_debugger.py
python app/scripts/verify_managed_debugger.py
```

Run the private Python integration suite for SDK permission/schema checks and actual
CMake-built C++ debugging over MCP. Every terminal_* and debug_* operation requires
process.execute, including inspection, because process data can extend beyond source
documents. Persisted profile/breakpoint changes also require workspace.write. Native
sessions belong to the IDE and survive job/task completion until explicitly stopped;
they are persistent runtime state rather than task-file artifacts.

Primary implementation references: [Microsoft ConPTY lifecycle](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session),
[redirected-parent handle behavior](https://github.com/microsoft/terminal/discussions/15814),
[libvterm](https://www.leonerd.org.uk/code/libvterm/),
[CodeLLDB configuration](https://github.com/vadimcn/codelldb/blob/v1.12.3/MANUAL.md).
Managed adapter references: [NetCoreDbg release](https://github.com/Samsung/netcoredbg/releases/tag/3.2.0-1092),
[DAP implementation](https://github.com/Samsung/netcoredbg/blob/3.2.0-1092/src/protocols/vscodeprotocol.cpp).

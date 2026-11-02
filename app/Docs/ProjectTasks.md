# Cargo, npm and standalone Java projects

CyCode detects `Cargo.toml`, `package.json` and standalone Java sources when opening
a folder or its project file. **View > Project tasks** selects the project directory,
configuration, target and arguments. Existing Build/Run shortcuts use the detected
adapter: Rider Ctrl+F9 builds and Shift+F10 runs; Visual Studio/VS Code bindings are
also routed through the shared command catalogue. Build errors appear in Problems.
Unsaved IDE documents are saved before GUI tasks; native/MCP execution refuses any
remaining dirty workspace document.

| Project | Implemented tasks | Run/debug |
|---|---|---|
| Rust/Cargo | Real metadata, Debug/Release builds, package/bin selection, Cargo tests, offline option | Actual compiler-reported executable in the integrated terminal; build-before-debug through native DAP |
| npm web/Node | Existing build/test scripts, selected named script, explicit dependency install | Selected script, default start/dev, in a persistent integrated terminal |
| Java | Standalone source-directory compilation with javac; Debug/Release symbols; compiler diagnostics | Main classes discovered from compiled class-file methods and run through java in the terminal |

The Rust adapter uses installed Cargo, or CyCode's private pinned toolchain when
Cargo is absent. `cargo metadata --format-version 1 --no-deps` supplies actual
workspace targets. Builds parse compiler JSON for diagnostics and executable paths;
custom build-script/test executables are excluded from runnable artifacts. Outputs
go under `.cycode/cargo-target`. Dependencies, build scripts and procedural macros
execute when explicitly building/testing trusted projects, according to Cargo rules.
The language-server default still disables automatic execution of those rules.

npm uses installed Node.js and its npm CLI with an argv vector. The selected script
must exist in package.json. Install dependencies explicitly uses `npm ci` with a
package-lock, otherwise `npm install`; lifecycle scripts follow the trusted project's
npm configuration. Inspection and configuration never install dependencies or run
scripts. Build/test commands use their corresponding scripts; the selected script
controls Run/Run npm script. Framework toolchains and their packages belong to the
project. Long-lived dev servers should use Run, then their terminal's stop control.
CyCode does not open a browser or provide JavaScript/browser debugging yet.

The standalone Java adapter uses installed javac/java and UTF-8 source/argument
files. Annotation processing is disabled (`-proc:none`). Compiler output is UTF-8;
interactive output uses the attached terminal's encoding. Each compilation receives
a fresh `.cycode/java-build/<id>/classes` directory. Runnable entry points come from
actual public static main(String[]) methods in compiled classes. If several exist,
select the qualified main class. Maven/Gradle descriptors are detected but their
dependency models, resources, test frameworks and build/debug adapters are not
connected; the panel states this explicitly. This adapter has no Java debugger.

## Native services and MCP

`ProjectTasks` belongs to the SDL-free native Engine and is shared by GUI and MCP.
Profiles live in `.cycode/project-tasks.json`; `.cycode/project-build.json` caches
inspected inputs/targets/artifacts. Paths are workspace guarded, including symlinks.
Failed builds clear runnable artifact metadata. Builds always invoke the real tool;
cached metadata never substitutes for a compilation.

`project_inspect` reads manifests and cached targets without executing project code.
The following operations require the SDK's `process.execute` permission:

- `project_configure`, `project_build`, `project_test`, `project_install`, `project_script`.
- `project_launch` returns a terminal id for the existing terminal read/write/close operations.
- `project_debug_target` builds a Cargo binary and returns its executable for the
  existing debug_configure/debug_launch operations; it does not launch a debugger.
- `project_cancel` stops the current compiler/test/script process tree. Persistent
  run terminals have their own terminal_close lifecycle.

The execution profile accepts `system` (automatic/cargo/npm/java), `directory`,
`configuration`, `package`, `target`, `script`, `mainClass`, `sourceDirectory`,
`offline` and `args`. One native project task runs per instance, with a 180-second
total process deadline and 4 MiB output bound. GUI Cancel and CyToolsCore job
cancellation terminate the actual owned process tree. Internal request correlation
prevents cancelling a different running task. The SDK owns jobs, identities,
permissions and reservations; the native bridge only dispatches authenticated IPC.

Windows x64 is the local validation target. Linux/macOS implementations remain
unverified. This milestone does not establish full Rider project/debugger parity.

References: [Cargo metadata](https://doc.rust-lang.org/cargo/commands/cargo-metadata.html),
[Cargo JSON messages](https://doc.rust-lang.org/cargo/reference/external-tools.html#json-messages),
[npm run-script](https://docs.npmjs.com/cli/v10/commands/npm-run-script/),
[javac](https://docs.oracle.com/en/java/javase/24/docs/specs/man/javac.html).

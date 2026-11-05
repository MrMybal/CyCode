# Named Rustup toolchains

Project tasks > **Rustup toolchain (empty is automatic)** selects an installed
Rustup toolchain for Cargo configuration/build/test and build-before-run/debug.
Leave it empty to retain the previous installed Cargo/private fallback behavior.
The adapter does not change Rustup defaults, directory overrides or project files.

`cargoToolchain` accepts up to 128 ASCII letters/digits, dots, hyphens and
underscores, beginning with a letter or digit. Examples of syntax include
`stable`, `nightly-2026-10-01`, a version/host-qualified name or an existing custom
linked toolchain name. These examples do not assert local installation. Paths,
leading `+`/options, whitespace and shell separators are rejected.

## Cargo execution

Explicit names use an argv vector equivalent to:

```text
rustup run <name> -- cargo metadata/build/test <Cargo arguments>
```

The command separator keeps a literal command/argument from becoming a Rustup
option. CyCode never adds `--install`, queries installed versions, installs Rustup
or edits its configuration. Missing toolchains/components are reported by Rustup
in task output or server logs. Cargo dependencies and trusted build scripts retain
their normal behavior; offline/locked flags still belong to the Cargo invocation.

Rustup is found through the existing managed/system tool resolver, including
`PATH` and `CYCODE_RUSTUP`. Inspection lists its path without invoking it. Explicit
selection requires a standard working Rustup installation/proxy setup; it does
not use CyCode's private standalone Cargo as a substitute. Rustup owns custom
toolchain/component fallback rules.

The selected name sets the child process's `RUSTUP_TOOLCHAIN`; `CARGO`/`RUSTC`
use `cargo`/`rustc` through Rustup's configured proxy PATH. CyCode's derived private
Cargo home/compiler environment is not injected for a named toolchain. Output
directory, deadline, cancellation and process-tree ownership stay in ProjectTasks.
No shell or discovery subprocess is added.

The cache includes the name. Changing it clears cached targets/artifacts; builds
always invoke the real tools. Version/date names can express an intended pin,
but this implementation does not attest installed bits. Updating an installed
toolchain under the same name is not a watched cache input; the next actual build
is still owned/evaluated by Cargo.

## Rust analysis

**Apply Cargo options and restart Rust** also copies the selected name into the
local/session `cargoProject` analysis snapshot as optional `toolchain`.
Empty/omitted names preserve the legacy profile shape. Later task changes still
need explicit application; changing a build name does not restart analysis.

For a named snapshot, installed defaults request:

```text
rustup run <name> -- rust-analyzer
```

This requests the analyzer component in Rustup's selected context. Missing
components require separate installation; custom-toolchain fallback belongs to
Rustup. An explicit user analyzer argv is retained as the command inside that
same wrapper, allowing an already installed compatible server. Availability of
Rustup alone does not prove that the toolchain or analyzer component is installed.

Both initial and subsequent configuration use `cargo.sysroot: "discover"`
instead of retaining CyCode's private fallback sysroot. `cargo.extraEnv` receives
the three selection variables; conflicting copies in `check.extraEnv` are
removed so normal checks inherit them. Other trusted options/extra arguments
remain; arbitrary custom commands can still choose their own compiler behavior.
Existing automatic build-script/proc-macro/check-on-save policy is retained.

The existing LanguageService owns the Rustup/analyzer process tree and restart
epoch. Enabled preference application validates the manifest/settings and Rustup
presence before writing preferences or replacing the current configuration; it
does not probe installation by launching Rustup. Disabled settings can still be
stored/stopped without Rustup. Removing the snapshot or applying an empty name
restores the base launch/environment/sysroot. Saved profiles retain a symbolic
name, not resolved Rustup/compiler paths.

MCP/plugins use the same `cargoToolchain` task field and
`language_configure.cargoProject.toolchain`; operation IDs, permissions and the
native C ABI layout are unchanged. Project/pref inspection does not launch tools.

## Validation and limits

See [Validation](Validation.md) for Windows production compilation, MSVC syntax
checks and the actual SDK manifest/schema examples. Wrapper literals, separator/
no-install policy, names/bounds, environment projection, cache invalidation,
analyzer component/custom argv, sysroot and disabled preference assertions extend
existing opt-in test targets and have not been executed under the current
antivirus-related suspension. No Rustup, Cargo, compiler, analyzer or GUI run
validated this addition. Missing-component behavior, native cancellation, actual
feature/toolchain coherence, Unicode paths and Linux/macOS remain unverified.

Installed-toolchain listing/management, arbitrary toolchain paths, cross-target
runners and broader Rust debugger/framework coverage remain separate work.

References: [Rustup toolchains](https://rust-lang.github.io/rustup/concepts/toolchains.html),
[overrides](https://rust-lang.github.io/rustup/overrides.html),
[installation/proxies](https://rust-lang.github.io/rustup/installation/index.html),
[Rustup run implementation](https://github.com/rust-lang/rustup/blob/master/src/cli/rustup_mode.rs).

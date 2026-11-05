# Cargo workspaces, features and targets

Project tasks exposes the same native adapter to the desktop, MCP and the
permission-checked `project.*` plugin host. Cargo owns TOML parsing, resolution,
profile inheritance and compilation; no second Rust backend or ABI is added.

Open a workspace folder or Cargo.toml, then **View > Project tasks**. Configure
refreshes packages, targets and declared features through
`cargo metadata --format-version 1 --no-deps`. Read-only inspection uses the saved
model without invoking Cargo. Configure can resolve dependencies/update Cargo.lock;
offline and locked also apply to metadata.

## Saved controls

| Field | Behavior |
|---|---|
| `cargoToolchain` | Empty preserves automatic selection; a named installed Rustup toolchain wraps Cargo through `rustup run`, without requesting installation. See [Rust toolchains](RustToolchains.md). |
| `cargoWorkspace` | `--workspace` builds/tests all members. Mutually exclusive with a nonempty `package`. False preserves Cargo's normal selected-manifest/default-member rules. |
| `package` | Exact package name/specification forwarded as one `--package` argument. Wildcards and leading option markers are rejected. The menu selects reported names. |
| `cargoTargetKind` | `bin` (default), `example`, `lib`, `test` or `bench`. Named targets use the corresponding Cargo selector; libraries use `--lib`. |
| `target` | Optional exact target name. Empty bin preserves default Cargo targets; empty example/test/bench selects `--examples/--tests/--benches`. Libraries ignore the name. |
| `cargoProfile` | Optional `--profile`, up to 128 UTF-8 bytes; overrides Debug/Release. Empty preserves Configuration. Cargo validates existence/inheritance. |
| `cargoFeatures` | Up to 128 unique names, 256 UTF-8 bytes each, one per line. `package/feature` is supported; the declared-feature menu adds that form. No whitespace/comma bundles or option markers. |
| `cargoAllFeatures` | `--all-features`. |
| `cargoNoDefaultFeatures` | `--no-default-features`. |
| `cargoLocked` | `--locked`: Cargo rejects missing locks or resolution changes. |
| `offline` | `--offline`; combined with locked gives Cargo's frozen behavior. |

Metadata receives feature/offline/locked options, without build-only package, target
or profile switches. It lists all workspace members. Packages retain opaque IDs,
workspace-relative manifests, declared feature names and Cargo's default-member
status. Targets retain package IDs, kinds and required features. Required features
are displayed, never silently enabled. This model does not claim transitive
enabled-feature state.

Example SDK/native parameters for an executable example:

```json
{
  "system": "cargo",
  "directory": ".",
  "package": "render-tools",
  "cargoWorkspace": false,
  "cargoTargetKind": "example",
  "target": "preview",
  "cargoFeatures": ["render-tools/gpu"],
  "cargoNoDefaultFeatures": true,
  "cargoProfile": "development-fast",
  "cargoLocked": true
}
```

## Executables and cache

Run/debug build first and use Cargo's actual reported executable. It must match a
known workspace package ID, target name and selected kind, and exist inside the
opened workspace. Dependency executables, custom-build scripts, test harnesses
and example libraries are excluded. Equal target names remain distinguished by
package and kind; several eligible executables require narrower selection.
Libraries/tests/benchmarks cannot run as applications. `project_test` still invokes
Cargo test; no benchmark runner or structured framework result tree is added.

The cache includes every new option, workspace/member manifests, the workspace
lock file, and config/toolchain files from the selected directory through the
opened workspace root. Changed options/inputs invalidate cached targets/artifacts.
Failed, cancelled, timed-out or truncated configure/build operations leave an
incomplete cache with no runnable artifacts. Legacy profiles gain defaults; old
Cargo caches require refresh. Every build still runs Cargo. Existing buffer,
deadline, cancellation and terminal lifecycle guards remain shared.

Workspace roots, member manifests and target sources must fit inside the opened
IDE workspace. Open the enclosing workspace instead of an isolated member whose
Cargo root is outside it. Global/user Cargo config, external dependencies/toolchains
and external source edits are not watched; the next real invocation evaluates them.
Named installed Rustup selection is available; toolchain listing/management,
arbitrary toolchain paths and cross-target execution/custom runners remain separate work.
Trusted project build rules/macros execute on explicit tasks; language-server
defaults remain unchanged. The explicit **Apply Cargo options and restart Rust**
action synchronizes manifest/features/default-feature/offline/locked settings;
it can remember a snapshot in local application preferences. Later task changes
require applying again. Package/target/custom profiles remain build controls.
See [Rust analysis](RustAnalysis.md) for the mapping, removal and validation limits.
Builds are not sandboxed project execution.

## Validation

See [Validation](Validation.md) for Windows production compilation, SDK schemas
and syntax-only checks. New regression assertions extend the existing task target;
they have not run while native acceptance is suspended after antivirus removals.
No Cargo invocation, generated Rust executable, debuggee or GUI run validated this
addition. Actual workspace/feature resolution, example run/debug, menus/keyboard,
themes/DPI and Linux/macOS remain unverified. Advanced Rust debugging and framework
test discovery remain broader work.

References: [Cargo build](https://doc.rust-lang.org/cargo/commands/cargo-build.html),
[metadata](https://doc.rust-lang.org/cargo/commands/cargo-metadata.html),
[compiler JSON](https://doc.rust-lang.org/cargo/reference/external-tools.html#json-messages).

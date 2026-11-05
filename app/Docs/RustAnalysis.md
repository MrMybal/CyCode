# Cargo options for Rust analysis

**View > Project tasks > Apply Cargo options and restart Rust** copies the current
Cargo manifest, features, default-feature and offline/locked options to the Rust
language server. It is an explicit restart/enable action. Editing build controls,
inspecting a project or reading saved preferences does not start a server.

Remember for this project stores that snapshot in local application-owned
language preferences. Uncheck it for this session only. Later Cargo task edits do
not silently change the saved snapshot; apply again to adopt them. The existing
revision/file-lock guard rejects competing preference writers before replacing
the server configuration. A host without a preferences directory applies only
session settings. Raw executable arguments and server settings remain separate.

## Mapping

| Cargo task option | rust-analyzer setting |
|---|---|
| `directory` | The guarded Cargo.toml becomes the single `linkedProjects` manifest. |
| `cargoFeatures` | `cargo.features` list, including `package/feature` names. |
| `cargoAllFeatures` | `cargo.features: "all"`. |
| `cargoNoDefaultFeatures` | `cargo.noDefaultFeatures`. |
| `offline` / `cargoLocked` | `--offline` / `--locked` in `cargo.extraArgs`, for metadata and other Cargo invocations. |
| `cargoToolchain` | Optional named Rustup context for the server/Cargo/rustc; empty keeps the base launch. See [Rust toolchains](RustToolchains.md). |

The override replaces those fields in both initialization options and the
`rust-analyzer` configuration namespace sent through the existing LSP client.
Legacy flat Rust settings remain consistent too. Explicit `check.features` and
`check.noDefaultFeatures` are reset to null so normal checks inherit the Cargo
feature selection. Existing `--offline`, `--locked` and `--frozen` extra arguments
are replaced by the typed choices without accumulating duplicates. Other trusted
settings, argv, environment resolution, sysroot and extra arguments are preserved.
Custom override commands or feature flags in other raw arguments can still affect
the server and remain the user's configuration responsibility.

Minimal custom settings gain the same disabled defaults for automatic build
scripts, procedural macros and check-on-save as the installed Rust configuration.
Explicit user settings enabling them are retained. No project-supplied executable,
shell command or initialization JSON is imported by this typed action.
Rust-analyzer can evaluate Cargo metadata/toolchains and resolve dependencies on
explicit startup; offline/locked have their normal Cargo meanings.

Package, target-kind/name, Debug/Release and custom compilation profiles remain
build controls. Rust-analyzer loads the linked Cargo workspace and does not
reproduce those exact build selections or arbitrary profile-specific cfg/env in
this integration. Build-script-generated cfg/types still depend on the user's
separate build-script/proc-macro settings. This addition does not establish full
equivalence between compiler and analysis behavior.

## Native, SDK and plugins

`project.inspect` / MCP `project_inspect` includes an optional `rustAnalysis` object
for Cargo projects. It is derived from the normalized task profile without Cargo
execution. Forward it as `cargoProject` to the existing process-authorized
`language.configure` / MCP `language_configure` with `language: "rust"`:

```json
{
  "language": "rust",
  "cargoProject": {
    "manifest": "member/Cargo.toml",
    "features": ["member/gpu"],
    "allFeatures": false,
    "noDefaultFeatures": true,
    "offline": true,
    "locked": true
  }
}
```

`enabled` defaults to true. `save: true` requires the current
`expectedPreferencesRevision`; the saved launch retains relative manifest and
typed options, not resolved tool paths or derived initialization data. Core/MCP
reading never activates that preference. Desktop first-document activation uses
the existing saved-server policy. The ABI2 JSON host shares these same operations
without a C ABI layout change or another process owner.

Omitted `cargoProject` retains the existing override, including when editing raw
argv/settings. `cargoProject: null` removes it and returns to the base settings.
Problems > Configure language server also shows the manifest and a removal action
that commits only on Apply. Use installed defaults clears the draft override;
Reload restores the saved one; Cancel leaves the running configuration intact.
Forget saved settings keeps the existing current-server behavior.

The profile accepts only manifest/features/allFeatures/noDefaultFeatures/offline/
locked and optional toolchain. Feature names retain Cargo task limits (128 unique items, 256 UTF-8 bytes
each); manifest paths use portable forward slashes, at most 8192 bytes, and must
name Cargo.toml within the opened workspace. Parent traversal, absolute/drive/ADS
paths and escaping symlinks are rejected. Enabled configuration validates the file
and derived 64 KiB settings limit before preference writes or server changes.
Disable remains possible after the manifest disappears; later Enable validates
the path again. No automatic file watcher adopts task-profile changes.

## Validation

Windows production compilation, source-only MSVC checks and actual SDK schema
examples are recorded in [Validation](Validation.md). Native mapping, dual LSP
settings, immutable base/clear, disabled/read-only policy, path/size guards,
preference conflict and reopen assertions extend the existing lifecycle test
target. They have not run under the current antivirus-related suspension.
Actual rust-analyzer import/resolution, restarts/epoch invalidation, multi-instance
behavior, GUI keyboard/themes/DPI and Linux/macOS acceptance remain pending.

References: [rust-analyzer configuration](https://rust-analyzer.github.io/book/configuration.html),
[Cargo build options](https://doc.rust-lang.org/cargo/commands/cargo-build.html).

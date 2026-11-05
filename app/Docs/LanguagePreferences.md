# Language-server preferences

Rust adds an optional typed `cargoProject` selection alongside raw argv/settings,
including a symbolic installed Rustup toolchain name when selected.
Project tasks can apply the current manifest/features/offline/locked snapshot and
remember it through this same revision-guarded store. Omission preserves it;
explicit null removes it. See [Rust analysis](RustAnalysis.md) for supported
mapping, restart behavior, path guards and build-profile limitations.

Problems > Configure language server edits the native server's unresolved argv
and settings. Remember for this project is selected in an ordinary desktop
session. Apply and restart saves that choice and enables the server. Enable,
Disable and Restart also remember the current launch and activation choice.
Unchecking Remember applies a session-only change. No shell command is assembled.

Use installed defaults sets arguments to `[]` and settings to `{}`. The host
resolves the current bundled/system tools and default settings on preparation.
Java's private data/OSGi paths and Rust's derived tool environment are not
serialized as a saved default launch. An explicit argv remains an explicit
user-selected override. The legacy single-executable/C# DLL setting is retained
when its fields are left unchanged.

Reload saved settings replaces the modal's draft and revision with the current
saved choice. A revision conflict leaves the draft intact until that action.
Forget saved settings removes the persisted choice without changing the current
server; the next session returns to installed defaults. Applying again with
Remember checked saves the draft again.

## Storage and activation

The desktop and CLI supply an application-owned directory to the UI-free Engine.
Files live under the installation's ignored
`data/settings/language-servers/<workspace-key>.json`, with an adjacent persistent
lock file. The key hashes the canonical workspace directory (not a solution's
filename); the file verifies the complete workspace identity to reject collisions.
Windows uses invariant Unicode case folding; POSIX retains case. Moving a project
or portable installation does not migrate profiles. Profiles are not included
in the existing UI configuration export or project templates.

The host does not load executable preferences from `.cycode`, a solution or a
cloned project's files. Read/inspection refreshes stored data but never enables
saved servers. An embedding using `Engine(workspace)` without an explicit
preferences directory cannot persist settings. Desktop smoke/test sessions omit
it; source-only verification does not create profiles.

On desktop opening, saved C++/C# choices override the global default executable.
Unreal retains its initial analysis pause; restoring analysis does not undo an
explicit Disable or reset another backend's current configuration. Installed
additional servers still start on the first relevant document opening, using the
saved launch unless that backend was explicitly disabled for the project.
Core/CLI/MCP additional servers require explicit `language.configure` permission,
even if a saved profile says enabled. Reading a profile does not start it.

Plugin profiles use the contributed backend ID. They remain stored while the
plugin is disabled or absent; registration/inspection does not execute them.
Reactivation retains the existing disabled-owner policy. An explicit Enable or
the normal first-document desktop policy restores the saved launch. There is no
implicit plugin installation or native-code sandbox.

## Shared native, MCP and plugin contract

- `language.preferences` / MCP `language_preferences`: refresh local preferences;
  return `available`, `revision`, `error` and `profiles`. Read permission only.
- `language.configure` / MCP `language_configure`: existing process-authorized
  launch control. Omitted launch fields preserve the current explicit choice or
  use the saved profile; explicit empty arguments/settings select installed
  defaults. `enabled` defaults to true. `save` defaults to false; true requires
  `expectedPreferencesRevision` from the preferences read.
- `language.preferencesForget` / MCP `language_preferences_forget`: remove one
  saved backend with the expected revision; requires `process.execute` because
  it changes future executable configuration. It does not stop/reconfigure the
  current server or mutate project files.
- `language.status` includes compact cached preference metadata and each current
  server's unresolved `configuration`, separately from resolved runtime
  `arguments`/`settings`. Frame-by-frame status does not reread the file or copy
  the full saved catalogue. Explicit reads refresh other instances' changes
  without applying them to current servers.

The existing permission-checked native plugin host exports all three operations.
GUI, MCP and plugins share one store and the actual native server owners; Python
does not maintain a second configuration model. Read-only identities cannot
change future executable preferences.

## Guards and limits

Saves hold a nonblocking OS file lock across a fresh read, revision check and
guarded atomic replacement. A competing/stale writer is rejected before changing
the current server. Forget uses the same guard. A crashed owner releases the OS
lock; the filename stays in place to avoid replacing the locked inode.
Invalid/mismatched files report errors and are never silently overwritten.
Write failures leave current server configuration unchanged. If a successful save
is followed by a server-preparation failure, that distinction is reported and
the saved configuration remains available for correction.

Limits: 42 saved backend IDs per workspace (10 built-ins plus 32 plugin slots),
160 ASCII letters/digits/`._-` per ID, 128 argv values of at most 8,192 bytes,
64 KiB settings, 4 MiB per file, integer revisions through 2^53-1. NUL arguments,
an empty argv program, unknown fields, ambiguous executable/argv/settings and
unsupported readiness notifications are rejected. Custom executables are trusted
user preferences, not a sandbox.

Windows/MSVC compilation and source checks cover this change. New native assertions
cover reopening, restoration, default intent, stale revisions, unavailable plugins,
invalid files and a host without storage. The existing MCP web test adds permission
and save/forget checks. Those runtime tests and graphical interaction remain
unexecuted while native validation is suspended following the Kaspersky removals.
Linux/macOS locking, filesystem and desktop behavior remain unverified.
See [Validation](Validation.md).

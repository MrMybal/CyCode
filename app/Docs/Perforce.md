# Perforce provider

`cycode.perforce` is a bundled native ABI2 plugin; its provider is
`cycode.perforce.provider`. Git remains built in and selected by default. Enable
the plugin in Extensions and select Perforce (P4) in Version control with the
project folder in the mapping field. The mapping uses the normal revision-checked
`.cycode/vcs.json` service and deepest-root selection. Opening a project never
creates a client, logs in, changes its view or runs a sync automatically.

Install the P4 CLI separately and configure the workspace using the normal P4
environment/configuration, client, ticket and trust files. Tool discovery checks
absolute `CYCODE_P4`, packaged tools and absolute PATH entries. Neither P4 binaries
nor credentials are packaged. The implementation uses current tagged JSON,
fstat/reconcile and file-only submit options; real client/server version acceptance
is still pending. It does not implement a P4API SDK or a second VCS engine.

## Available actions

| Action | Scope |
| --- | --- |
| status / inspect | Selected client's opened files under the project, using filtered `fstat -Ro`. Shows action, changelist, type and unresolved status. This is not an automatic scan for unopened/offline edits or untracked files. |
| diff | Project or explicit file; unified P4 working-copy differences against the opened depot revision, shared CyUI Diff review, headers reconstructed from tagged file records. Unopened files are not forced into the diff. |
| log / history | Submitted changes affecting the mapped folder/file, default 50 and maximum 200, Unicode descriptions and first-line subjects. Dates are server Unix timestamps. |
| blame | Selected file's `#have` revision; read-only revision/line annotations. |
| checkout | `edit` for one existing regular file, default or explicit positive numbered changelist. |
| add | One existing regular file, explicit literal filename handling, default or numbered changelist; respects P4IGNORE and the server typemap. |
| reopen | Move one opened file into a default/numbered changelist. Does not create or edit changelist specifications. |
| revert | One opened edit/add/delete only, requires `confirmation:"REVERT"`. Discards local changes; P4 owns its file restoration/removal semantics. No editor Undo transaction can restore discarded disk content. Move/integration actions are rejected. |
| commit | Submit one resolved edit/add/delete file in the default changelist with an explicit nonempty description. Excludes the open stream specification (`-Sx`); no whole changelist/default-client submission. Numbered-changelist and move/integration submissions need the external client. |
| sync | Mapped project only; refuses opened project files, then runs read-only `reconcile -n -a -e -d` and refuses reported offline/untracked changes before syncing. No force, cleanup, silent reconcile, have-list-only update or branch switch. |
| lock / unlock | Explicit selected file, current client/user, no force or administrator override. |

The eleven custom actions are rendered by the common provider menus, palette and
forms, using stable contribution/field IDs and the existing EN/FR catalogue. Review
and Diff use the same editor/CyUI views as other providers. Git-specific UI stays
specific to Git. P4 transport errors and partial responses remain visible.

For sync, configure P4IGNORE for normal generated/editor files as appropriate for
your workspace, including unversioned `.cycode` metadata. The provider does not
silently add ignore rules. A conservative preflight can refuse a sync that the
external P4 client would allow; it does not overwrite ignored/untracked files on
the application's own authority.

## Common native/MCP route

`vcs_provider_action` accepts, for example:

```json
{"action":"checkout","contextPath":"Game/Source/main.cpp",
 "input":{"path":"Game/Source/main.cpp","changelist":"123"}}
```

The context determines the mapping; an explicit registered `provider` can override
its ID for that request. Host-owned workspace/root fields cannot be replaced by
plugin action input. Reads and writes through this generic SDK operation retain
its existing conservative `plugins.execute`, `vcs.write`, `workspace.write` and
`process.execute` permissions. Native GUI/CLI routes still select the mapped
provider. Typed SDK Git operations explicitly select Git in the requested workspace repository;
they reject external provider IDs and never execute a plugin under Git read-only
permissions or try to validate provider output with Git-specific schemas.

The native SDK client waits up to 600 seconds for a generic provider action,
covering sequential mapping/status/preflight/network budgets of these bundled
providers. Cancellation retains the existing SDK reservation until the native
operation returns; it does not introduce an early VCS process-cancel API.

## Process and path contracts

The plugin links only the shared native `cycode_process`/file utilities and JSON,
without Engine, CyUI, ImGui, an extra process implementation or a new dependency.
It uses argv, fixed cwd, owned children, bounded output and deadlines. Initial
`info` identifies a configured client/user; subsequent commands explicitly pin
those two identities. The mapped folder can be a subfolder of the P4 client root,
but must stay inside the CyCode workspace. Each root/selected-file `where` query
must return one positive matching mapping. Ambiguous overlay/exclusion query
results, alternate/null client roots, UNC roots and symlink traversal are not
supported. Settings/view/other-process changes during a multi-command operation
are not an atomic transaction.

Paths are explicit local files or an application-generated project `...` selector.
Parent traversal, URL/drive/alternate-stream input and literal `...` components are
rejected. Normal file selectors escape `%`, `@`, `#`, `*` once; adding uses P4's
literal-filename mode. Returned paths must be local and inside the selected root;
matching accommodates Windows drive/path casing and P4's encoded mapping output.
Opened move/integration pairs cannot escape a one-file revert/submit through their
partner because those actions are rejected before mutation.

`-ztag -Mj` output is parsed as bounded JSON records, including code/severity/error
categories. Only severity-2 `EV_EMPTY` (17) becomes a valid empty result for commands
that allow one. Authentication, permission, malformed, unknown and truncated
responses never establish a clean working copy. Output is limited to 512 KiB per
process, 20,000 records and JSON depth 16; callback JSON stays under the common
4 MiB ABI limit. Info/mapping calls use 15 seconds, opened-file reads 30 seconds,
and action/reconciliation calls 180 seconds per process. A timeout or overflow
retains common native process ownership through termination.

The installed P4 client's configuration remains in effect, including charset,
transport/SSO and configured diff helpers. The application never invokes an editor
form, password prompt, login or trust acceptance; stdin is closed and calls remain
bounded. Native extensions and configured client helpers are trusted code. Live
dirty/disk-conflict checks and post-operation document reconciliation are supplied
by the existing common VCS host. External filesystem/client/server races remain
the client's responsibility; concurrently edited buffers are retained by CyCode.

Reference contracts: [P4 JSON flags](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/global.options.html),
[fstat](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/p4_fstat.html),
[mapping queries](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/p4_where.html),
[submission](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/p4_submit.html)
and [reconcile preview](https://help.perforce.com/helix-core/server-apps/cmdref/current/Content/CmdRef/p4_reconcile.html).

## Verification and remaining work

The production Windows plugin and source-only regression checks compile. The
runner-injection regression source covers mapping, metacharacters, Unicode,
changelists, one-file submission/revert, sync preflight, deadlines, empty results,
authentication/permission failures, partial/malformed responses, outside paths,
real production-DLL registration and dirty-buffer/disable routing. The MCP test
source also exercises typed-Git isolation under a Perforce project mapping.
Those native/MCP regressions are **not executed** during the Kaspersky interruption;
no P4 server, P4 subprocess or application window is launched. See
[Validation](Validation.md) for the exact checks actually performed.

Real authentication/SSO, exclusive file types, server triggers, binary/unicode
files, large projects and GUI interactions remain unverified. Windows clean
installation and Linux/macOS remain untested. Further work includes client/view
configuration, numbered changelist creation/full submission, shelves/unshelve,
move/delete/integrate, advanced resolve/stream/overlay workflows, offline scanning
UI and cancellable VCS jobs. [Mercurial](Mercurial.md) has a separate bundled plugin
with its own scope and pending acceptance. This plugin does not
establish complete Rider/P4V parity.

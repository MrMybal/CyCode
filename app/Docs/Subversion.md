# Subversion provider

CyCode builds `cycode.svn` as a bundled native ABI2 plugin. Its provider ID is
`cycode.svn.provider`; Git remains the default. Enable Subversion in Extensions,
open Version control and choose Subversion (SVN) with the working-copy folder in
the mapping field. Mapping is explicit, persisted in `.cycode/vcs.json` and shared
by menus, the panel and native/MCP requests. No checkout or repository creation
runs when selecting a provider or opening a project.

Install the Subversion command-line client separately. Tool discovery checks
`CYCODE_SVN` (an absolute executable), packaged tools and absolute PATH entries.
No client is downloaded or bundled. The implementation targets the modern SVN
1.10+ command-line interface; real client/version/server acceptance is pending.
Project paths cannot select the executable or inject command options.

## Operations

| Action | Scope and behavior |
| --- | --- |
| status / inspect | Local `info --xml` and `status --xml --ignore-externals`; mapped workspace paths, named changelist entries, text/property/tree conflicts, relative repository URL and working revision. No update check or network poll. |
| diff | Working copy or explicit local file; SVN internal unified diff, displayed through the shared CyUI Diff review. No staging area. |
| log / history | XML history, default 50 and maximum 200 revisions; author/date/first-line subject plus full messages in the read-only Review view. A local target defaults to its BASE history. |
| blame | Selected file at BASE, read-only annotations; may contact the server. |
| add | Selected file only, depth empty, no automatic properties or parent addition. |
| revert | Selected file only, depth empty; requires `confirmation:"REVERT"`. Discards text/property changes and unschedules additions/deletions. SVN does not provide Undo for this action. |
| commit | All tracked changes in the mapped copy, explicit nonempty UTF-8 message, no external inclusion. Conflicts must be resolved first. |
| update | Mapped copy, ignores externals, postpones conflict decisions, requires a clean tracked copy. Untracked obstructions remain protected by SVN's own refusal rules. |

The plugin advertises seven custom actions; host menus and command palette use
those descriptors. Git branch/remotes/staging/stash views stay specific to Git.
Alternative-provider diffs, history and annotations now have their own Changes /
Review tabs; this fixes the previous early return that hid their preview.
Action titles/fields use the existing EN/FR catalogue and shared theme/DPI styles.

Native/MCP example after configuring a mapping:

```json
{"action":"log","contextPath":"Source/main.cpp","input":{"limit":25}}
```

Pass this to `vcs_provider_action`. Mutations use the same operation with, for
example, `action:"commit", input:{"message":"Describe the change"}` or
`action:"revert", input:{"path":"Source/main.cpp","confirmation":"REVERT"}`.
The generic SDK operation conservatively retains its existing plugin/process/
workspace/VCS permission requirements, including for provider reads. It does not
weaken the separate typed read-only Git operations.

## Ownership and guards

The mapped folder must be the exact SVN working-copy root inside the opened
workspace. Subdirectories of an outside working copy are rejected. Nested
directory checkouts/externals require their own mapping; targeted file externals
are rejected after a separate local XML status check. Operated paths must be local,
workspace-relative, inside the selected root and outside `.svn`; parent traversal,
URLs and symbolic-link traversal are rejected. The host also checks provider paths.
Status can display symlink entries; this provider does not operate on them.
Paths receive an explicit local prefix and final peg escape, including names with
literal `@`; no shell or project-generated command string is used.

Common process/tool discovery lives in `cycode_process`, shared by the native core
and plugin without copying its source or linking UI/Engine services into the DLL.
Calls are synchronous on the existing background VCS worker, serial with plugin
callbacks. Local commands have 15 seconds; history/blame/commit/update have 180
seconds per process, with 512 KiB output bounds. Native process ownership kills
owned children on timeout/output overflow. Complete XML is required before
interpreting a status or allowing an update/commit. Partial failures return
exitCode/output/timedOut/truncated/cancelled; large callback responses fail above
the existing 4 MiB ABI bound. Invalid bytes in textual client output become Unicode
replacement characters; XML file paths remain UTF-8.

Commands always use `--non-interactive --no-auth-cache`. Existing system SVN
authentication/configuration is used; CyCode does not collect passwords, approve
certificates or save credentials. SVN's configured transports may run installed
helpers. See [SVN authentication flags](https://subversion.apache.org/docs/api/latest/group__auth__fns.html)
and the [official quick start](https://subversion.apache.org/quick-start).

Host mutations refuse dirty buffers and open files whose disk bytes differ from
their baseline. This check also strengthens other external VCS providers. After
an operation, clean buffers are reconciled and concurrently modified/deleted/
unreadable buffers are retained. A provider operation and mapping changes share
the native VCS lock. External filesystem/other-process races are not atomic with
SVN; changes made during execution are preserved by the editor reconciliation.
Disable waits for the serial callback; there is no new early-cancel VCS API or
process isolation boundary for trusted native plugins.

## Validation and remaining scope

The new contract regression source injects a fake runner: no SVN subprocess is
needed for XML/property/tree-conflict/changelist parsing, file-external rejection, root/peg/argv guards, deadlines,
truncation and mutation checks. It also specifies real production-DLL loading,
mapping, disk-conflict rejection and disable-without-fallback assertions.
Existing extension GUI regressions now cover alternative-provider Diff Review in
six themes, EN/FR and three DPI scales. These sources are syntax-checked only;
no test executable is built or run during the current antivirus interruption.
Actual build/check results are recorded in [Validation](Validation.md).

Real SVN local/server operations, authentication, conflicts, GUI interactions,
Windows clean installation and Linux/macOS remain unverified. Checkout/creation,
delete/move/copy, branch/tag/switch/merge, changelists, locks, sparse/external editing,
rich SVN conflict resolution, credentials UI and cancellation require further
implementation. [Perforce](Perforce.md) now has a separate bundled provider;
[Mercurial](Mercurial.md) also has its own bundled plugin. Typed SDK Git operations are explicitly
Git-only; external SVN calls use the generic permission-checked provider operation.
This addition is not complete Rider or full SVN-client parity.

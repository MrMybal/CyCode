# Mercurial provider

The bundled ABI2 extension `cycode.mercurial` contributes
`cycode.mercurial.provider`. Git remains built in and selected by default. Enable
Mercurial in Extensions, then choose Mercurial (Hg) in Version control and map the
exact repository directory inside the workspace. The `.hg` directory must be
local. Mapping a repository does not initialize, clone, pull or modify it.

Install Mercurial separately. Tool discovery uses an absolute `CYCODE_HG` override,
managed tool locations or PATH. No client, credentials or Mercurial code is
redistributed. The implementation targets the documented JSON/template and
`filepath:` CLI interfaces; compatibility with an installed client still requires
runtime acceptance. It has not been tested against any real hg version yet.

## Operations and shared views

Status reports modified, added, removed, missing and unknown files, the named
branch and first parent changeset. Empty repositories have no fabricated parent.
Two parents produce a merge warning. Diff uses the common CyUI Diff view; history
and file annotations use read-only Review tabs. History is limited to 1–200
changesets (50 by default) with author, changeset, timestamp and description.

The twelve declared menu actions are:

- Working-copy diff and repository history.
- Annotations for one selected file.
- Add one existing regular file, or stop tracking one file while preserving it.
- Revert one file after entering `REVERT`; no `.orig` backup is created.
- Commit tracked changes with an explicit nonempty message, without automatic add.
- Update a clean tracked working copy to one verified changeset.
- Create a bookmark or set a named branch for subsequent commits.
- Pull from a configured remote alias without updating working files.
- Push to a configured remote alias without forcing or allowing new branches.

Both remote forms default to the explicit alias `default`. Enter another configured
alias, such as `default-push`, when required. Arbitrary remote URLs and local
directory fallbacks are rejected. Branch/bookmark creation and remote aliases use
bounded simple ASCII names; status can still display existing Unicode branch names.
Update accepts a changeset ID or a simple named reference, including `.`; arbitrary
revset expressions are rejected. Lookup must resolve to exactly one changeset,
whose full node is passed to `update --check`. No clean/merge/force option is used.

Common provider menus/forms, project mappings, document guards and error reporting
are shared with Git/SVN/Perforce. The host refuses write operations while the mapped
scope has unsaved buffers or disk conflicts; clean buffers are reconciled after
disk changes without overwriting edits made during an operation. Provider disable
does not silently route its mapping to Git. Generic SDK/MCP access uses
`vcs_provider_action` with plugin/process/workspace/VCS permissions; typed Git SDK
operations explicitly remain Git-only. See [Plugins](Plugins.md) and
[Version control](VersionControl.md).

## Process, path and repository guards

Every invocation uses an argv vector and the existing bounded native process
owner, never a shell string. `HGPLAIN`, empty `HGPLAINEXCEPT`, UTF-8 strict output,
noninteractive mode, no pager and disabled color stabilize parsing. Repository
identity is checked using `hg root --template "{hgpath|json}"` and filesystem
equivalence against the mapped `.hg`. Status/history/parents require complete
structured responses. Invalid, excessively nested, oversized, timed-out, cancelled
or truncated metadata cannot become a clean status or authorize an update.

File actions use an exact `filepath:<repository-relative-file>` selector after
`--`; repository diff uses literal `path:.`. Parent traversal, administrative
paths, nested repositories and symbolic-link traversal are rejected. Metadata
paths outside the mapping are rejected too. This deliberately excludes operations
on tracked symbolic links until a specific contract is implemented.

Mutations reject shared repositories, subrepositories, two-parent merges and
unfinished rebase/histedit/graft/shelve states. Finish or abort those operations in
the external client. Updates first reject any tracked working change; configured
unknown/ignored-file overwrite policies are forced to abort. Reads remain available
for inspection when possible, but this is not a conflict-resolution UI.

Metadata commands have a 15-second deadline; local final operations have 30 seconds;
pull/push have 180 seconds. Each response is bounded to 512 KiB. Generic SDK requests
allow 600 seconds for sequential preflight and execution. Known no-change exit 1
from commit/push is returned as success with `noChanges:true` and
`nativeExitCode:1`; other failures are preserved. Each operation is serial within
the native provider host. There is no interactive VCS cancellation UI yet.

The installed Mercurial configuration, extensions, hooks, authentication helpers
and transports are trusted user tooling. Plain/noninteractive mode is not process
isolation and does not disable hooks. No automatic credential configuration,
authentication dialog or network operation occurs on repository selection.

The implementation follows the primary Mercurial documentation for
[root templates](https://mercurial-scm.org/help/commands/root),
[exact file patterns](https://www.mercurial-scm.org/help/topics/patterns),
[scripting](https://www.mercurial-scm.org/help/topics/scripting),
[environment](https://www.mercurial-scm.org/help/topics/environment),
[update](https://www.mercurial-scm.org/help/commands/update),
[paths](https://www.mercurial-scm.org/help/commands/paths),
[commit](https://www.mercurial-scm.org/help/commands/commit) and
[push](https://www.mercurial-scm.org/help/commands/push).

## Verification and remaining scope

Windows production compilation and MSVC `/Zs` checks of the client, ABI adapter
and regression source passed. The opt-in runner regression specifies literal
metacharacter/Unicode paths, empty parents, merge/shared/subrepository guards,
exact file operations, pinned updates, remote aliases, no-change results, deadlines,
malformed/partial metadata and missing clients. It also specifies actual production
DLL registration, mapped-provider routing, dirty-buffer refusal and disable behavior.
The existing MCP assertions include provider inventory and typed-Git isolation.

These regression sources have **not been executed** during the Kaspersky
interruption. No hg subprocess, application window or native/MCP/GUI test was
launched. See [Validation](Validation.md) for the checks actually performed. Real
Mercurial local/network operations, authentication, hooks/extensions, large
repositories, graphical interactions and clean installation remain unverified;
Linux/macOS have not been built or run locally.

Remaining work includes clone/init, remove/move/copy, tag/bookmark management,
merge/conflict resolution, rebase/histedit/graft, shelve, amend, shared/subrepository
support, rich branch/remote views, credentials and cancellable VCS jobs. Repeated
CLI startups are bounded but have not been benchmarked; a command-server adapter
may be appropriate for frequent operations. This addition does not establish full
Mercurial-client or Rider parity.

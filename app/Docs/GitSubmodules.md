# Git submodules

The Git **Submodules** tab inspects configured top-level modules in the selected
repository. It joins `.gitmodules` paths with actual index gitlinks, then reads
each available child's HEAD, branch and Git working-tree status. Spaces, UTF-8
and NUL-delimited records are preserved; object IDs support SHA-1 and SHA-256.
It distinguishes absent checkouts, index conflicts, non-gitlink paths, matching
or different commits and unavailable inspection. Unknown dirtiness remains null.

Choose **Version control > Submodules**, refresh, and filter by path/name/state.
Each action opens a review section. Escape or **Cancel submodule action** discards
that review without calling Git. Actions affect one selected module:

- **Register submodule** uses `git submodule init` to register its URL locally.
- **Synchronize submodule URL** uses `git submodule sync` for a registered module.
- **Update to recorded commit** uses `update --init --checkout --jobs=1`. The
  destination is the commit recorded in the parent index, with a detached HEAD.
  This does not stage a new gitlink or commit the parent project.

Clone/fetch requires an explicit checkbox. Without it, update requires an existing
checkout, passes `--no-fetch` and sets a Git protocol allow-list containing no
protocol names (`GIT_ALLOW_PROTOCOL=:`). The nonempty value survives Windows
environment handling. With clone/fetch enabled, normal inherited Git protocol and
credential configuration applies. URL values and raw Git configuration are not
returned by the catalogue. Git hooks/helpers still execute with the user's normal
privileges; this protocol control is not an OS network/process sandbox.

## Add a module

Choose **Add submodule**, enter a repository address and a new repository-relative
folder, then choose **Preview submodule addition**. The destination parent must
already exist. An optional logical name defaults to the path; an optional literal
branch defaults to the remote HEAD. The form uses the current selected Git root.
Changing fields, refreshing/changing repositories or cancelling clears the preview;
cancelled or obsolete asynchronous results cannot reopen it.

The preview reads local metadata without clone/fetch or staging. It shows the
normalized address/name/branch/destination and the effects of the action.
`remoteCommit` stays null: Git resolves the remote commit during the later clone.
**Clone and stage submodule** is a distinct action. It creates a checkout and stages
`.gitmodules` plus the new gitlink, preserves unrelated staged entries and creates
no commit. No source Undo, automatic retry or cleanup is provided for this action.
Partial clones/configuration/index changes remain available after failure.

Both preview and execution require a new ordinary destination in the selected
repository, unoccupied metadata/config names, no overlapping module path/name,
and closed `.gitmodules`/destination documents. Existing `.gitmodules` must be a
normal, unchanged tracked file. Staged/untracked/conflicted changes and hidden
skip-worktree/assume-unchanged flags are refused. Its bytes are compared with the
indexed blob, allowing normal CRLF/LF conversion; custom transformed metadata is
unsupported. New metadata is allowed when the file is absent. Ignored destinations,
existing empty folders/repositories and orphan metadata are not adopted or forced.

The separate preview revision captures normalized inputs, parent directory and
metadata identities, relevant index/blob bytes, catalogue and parent HEAD/reference.
The last guard also detects switching between parent branches with identical
trees, since relative URLs depend on that context. The execution owner recalculates
these guards before one `git submodule add --name ... [--branch ...] -- URL PATH`.
External writers and remote branch changes remain outside an atomic transaction.

HTTPS/HTTP, SSH, Git and file URLs, SCP-style SSH and local/relative addresses use
normal Git protocol/credential policy. Embedded user-info credentials, query tokens,
fragments, custom helper-command addresses and checkout-history branch aliases
are refused; use Git credentials or SSH authentication. No remote commit is
invented or pinned by a local preview. Read queries deny Git transports, including
lazy object fetch; trusted local Git helpers are not OS-sandboxed.

## Edit a module URL or tracking branch

Choose **Edit submodule settings**, select **Repository address** or **Tracking
branch**, enter the new value, then choose **Preview submodule settings**. The
separate **Apply submodule action** button requires that preview. Changing the
value/type, refreshing, cancelling or switching repositories discards it; stale
asynchronous responses do not restore it.

URL changes use fixed `submodule set-url --quiet -- PATH URL` argv. The shared
address validator supports the same forms as addition and refuses embedded
credentials, query tokens/fragments and helper-command URLs. Git may also
synchronize local URL configuration according to its active-submodule rules.
Relative addresses retain Git's parent/default-remote resolution semantics.
These metadata operations deny Git transports; they do not clone or fetch.

Tracking branches use `submodule set-branch --branch VALUE -- PATH`. An empty
value uses `--default`, removing the project setting in favor of remote HEAD;
`.` uses the current parent branch name when a remote-tracking workflow later
uses it. Literal names are checked with Git. Local/effective Git configuration
overrides are reported and preserved. An already-configured project branch is
shown without allowing a redundant action or inventing a process result.
The existing **Update to recorded commit** still checks out the parent gitlink
commit; editing the tracking branch does not introduce remote-tip checkout.

Both operations write project metadata without staging, committing, changing the
checkout or creating an editor Undo transaction. Previously staged and unstaged
`.gitmodules` edits may coexist: the file must have one ordinary stage-zero index
entry without hidden flags/conflicts, but need not match its indexed blob. The
observed working bytes and indexed content are captured separately. Other config
records and their order, duplicates and valueless entries are retained and checked
after applying; the `.gitmodules` index entry remains unchanged. Metadata documents
must be closed. URL changes also require closed child documents and ordinary
parent-owned absorbed metadata for a populated checkout. Independent/embedded or
custom Git-directory layouts are unsupported for URL synchronization.

The preview shows the previous configured value, the new request, exact argv and
effects. Sensitive/unsupported previous values stay null with `previousRedacted`;
raw config is not returned. The revision binds the request, catalogue, metadata
working/indexed bytes, parent HEAD/reference, directories and applicable Git
pointer. It is rebuilt immediately before one action. Bounds are 16 KiB per native
request, URL/value 4096 UTF-8 bytes, branch 256 bytes, 1 MiB `.gitmodules`, 20,000
config records, 2 MiB per query and a 30-second preflight. Postflight inspects the
requested value, unrelated records, unchanged metadata index and checkout/pointer
state. A process exit zero with an unconfirmed outcome still has `catalogError`.
No atomic lock against external processes, automatic rollback or retry is provided.

## Deinitialize a checkout

Choose **Review submodule deinitialization** on a clean, registered checkout at
the recorded gitlink commit. A local preview precedes the separate danger action
**Remove reviewed submodule checkout**. It shows the number of tracked files to
remove and explicitly states the retained data. Escape/cancel, refresh or a
repository change discards the preview; cancelled/stale asynchronous results are
not applied. No default shortcut performs this removal.

Execution uses only `git submodule deinit --quiet -- PATH`, without force, all,
recursion or allowed Git transports. This removes the checkout and local module
registration, while preserving `.gitmodules`, the parent gitlink and the absorbed
repository under the parent's module metadata. It creates no commit/staging or
editor Undo transaction. Reinitialization is a separate operation.

Both preview and action refuse tracked changes, untracked **and ignored** files,
hidden skip-worktree/assume-unchanged flags, conflicted index entries and nested
gitlinks (including unpopulated ones). This intentionally blocks ignored build
outputs until the user preserves/manages them. Native documents under the module
and `.gitmodules` must be closed. `.gitmodules` must be normally tracked and equal
to its index blob, allowing CRLF/LF normalization. Independent repositories,
embedded `.git` directories, custom Git-directory mappings, redirected metadata and non-parent-owned Git pointers
are unsupported; no automatic absorption or migration is performed.

The preview revision additionally binds the complete child index/flags, `.git`
pointer bytes, ordinary metadata/parent directory identities and metadata blob.
At most 20,000 index records, 2 MiB per query, 256 directory levels and 30 seconds
of preflight are accepted. Every check is repeated before the one Git action.
Inspection disables optional Git locks and fsmonitor/untracked cache shortcuts
for the complete cleanliness query. No filesystem-wide lock is claimed against
external writers or later document opens.

The owner retains actual exit/timeout/output information, then inspects checkout
and registration state, unchanged gitlink/metadata and retained repository identity.
Git can report exit zero despite incomplete directory removal; that becomes a
`catalogError` instead of a success notification. Partial outcomes are retained
for inspection, with no automatic cleanup, rollback or retry.

## Native ownership and guards

`GitSubmoduleModel` parses bounded config `key\nvalue\0` and `ls-files --stage -z`
records and constructs fixed argv lists. `Engine::gitSubmodules` owns real Git
queries/actions through the existing native process service and VCS mutex. The GUI,
SDK/MCP and ABI2 plugins use this same owner; there is no Python repository model,
new dependency, executable target or CyUI module. UI colors/styles/filtering use
CyUI Core and English/French controls.

A mutation requires the observed repository identity and catalogue revision. The
revision fingerprints `.gitmodules`, effective parent settings, selected gitlinks,
child HEAD/branch/status, directory identities and private child repository/config
identities. It is recalculated before acting. Linked/nested repository selection
keeps the existing workspace boundaries. Symlinks/reparse points, absolute/traversal/
metadata paths, duplicate or overlapping module paths, invalid records and unresolved
gitlinks are refused. `.gitmodules` must be saved and reconciled with open buffers.

Update refuses reported tracked/untracked child changes and any open child document
at preflight. Original branches/commits remain Git-owned; there is no IDE source
Undo for checkout. Ignored files follow Git's own checkout rules and are not part
of the reported dirty check. No filesystem-wide lock is acquired against external
Git processes, hooks, later file edits or another app instance. These are observed
state/preflight guards rather than an atomic transaction over all external writers.

There is no force, recursive update, remote-tip update, merge/rebase or custom
update command in this service. The explicit `--checkout` overrides configured
custom update modes. Local URL registration/synchronization still follows Git's
semantics. Failures/timeouts may leave changed configuration or a partial checkout;
refresh and review that state. The response retains actual process status and a
new catalogue, or a separate `catalogError`. The owner invalidates the path index;
UI document reconciliation keeps the existing conflict/dirty-buffer protections.

Bounds: 16 KiB native request, 64 configured modules, 1 MiB `.gitmodules`, 2 MiB
per command output, 4096 UTF-8 bytes per relative path, 512 bytes per module name/
branch. Complete catalogue work has a 30-second deadline after repository selection;
addition-preview inspection has its own 30-second deadline and at most 256 parent
directory levels, URL/path 4096 bytes, name 512 bytes and branch 256 bytes;
an action has 180 seconds. SDK waiting allows 600 seconds for the native inspection,
revalidation, action and refresh. Git terminal prompting is disabled. The operation
is noncancellable while native process ownership is retained; timeout is reported
after the existing process owner handles termination, with no automatic retry.

## SDK/MCP and plugins

`vcs_submodules` / `vcs.submodules` returns `repository`, `identity`, `revision`
and typed `entries`. Entries expose name/path/workspace root, expected commit,
actual HEAD/branch, registration/checkout flags, nullable dirty state, status/error
and opaque directory/repository identities. It is a read operation with no mutation
permission or exclusive group. Select a module from its actual returned path.

`vcs_submodule_init`, `vcs_submodule_sync` and `vcs_submodule_update` map to the
corresponding `vcs.submodule.*` methods. For example, after an actual inspection:

```json
{"repository":"project","provider":"git","path":"vendor/math",
 "expectedRepository":"observed-repository-identity",
 "expectedRevision":"observed-catalogue-revision","fetch":false}
```

`fetch` belongs only to update. Each existing-module mutation requires `path`,
`expectedRepository` and `expectedRevision`; `repository` defaults to the workspace
root. Paths refer to persistent project repositories, not SDK task artifacts.
SDK mutations require `vcs.write`, `workspace.write` and `process.execute`, use
the existing workspace mutation group and declare noncancellable behavior. These
typed methods always target built-in Git; other providers retain their own actions.
ABI2 `host->request` exports the same four methods and enforces all three mutation
permissions, including rejection from read-only provider callbacks. C ABI unchanged.

`vcs_submodule_add_plan` / `vcs.submodule.addPlan` additionally takes `url`, `path`,
optional `name`/`branch` and observed `expectedRepository`/`expectedRevision`.
It is noncancellable local inspection with no mutation permissions/group. Its
result contains repository/identity/catalogRevision/revision, normalized request,
workspace root, bounded argv, createsCheckout/stagesGitmodules/stagesGitlink/
mayContactRemote flags and `remoteCommit:null`.

Pass the same inputs and `expectedPlanRevision` from that preview to
`vcs_submodule_add` / `vcs.submodule.add`. It uses the three mutation permissions,
workspace mutation group and noncancellable native ownership, returning the same
actual process/catalogue result as other submodule writes. ABI2 exports both
methods with the same read/mutation distinction. No URL/repository is a temporary
SDK input artifact and no Python Git owner is added.

`vcs_submodule_deinit_plan` / `vcs.submodule.deinitPlan` takes the observed
repository/catalogue guards and path. Its typed result adds the actual entry,
trackedFiles, fixed argv and explicit removesCheckout/unregistersLocally/
preservesGitlink/preservesGitmodules/preservesRepository/mayContactRemote effects.
Pass its revision as `expectedPlanRevision`, with the same guards/path, to
`vcs_submodule_deinit` / `vcs.submodule.deinit`. Preview has no mutation permission
or exclusive group; execution uses all three permissions and the workspace
mutation group. Both are noncancellable while the existing native owner runs.
ABI2 exports the same methods without changing the C ABI or adding a Python owner.

`vcs_submodule_settings_plan` / `vcs.submodule.settingsPlan` takes the observed
repository/catalogue guards, path, setting (`url` or `branch`) and value. The
typed plan returns request/entry, previousValue/previousRedacted/hasConfiguredValue,
hasLocalBranchOverride/alreadyConfigured, argv, writesGitmodules/
maySynchronizeLocalUrl and false staging/checkout/transport effects. Apply the same
inputs plus `expectedPlanRevision` through `vcs_submodule_settings` /
`vcs.submodule.settings`. Preview has no mutation permission/group; application
requires VCS/workspace/process permissions and the workspace mutation group.
Both retain noncancellable native process ownership. ABI2 exports both methods.

## Verification and remaining work

MSVC `/Zs` checks the pure parser/argv planner, Engine, GUI, routing/plugin code and
regression sources without linking/running test objects or executables. Assertions
specify NUL/Unicode/newline paths, conflict stages, SHA formats, duplicate/overlapping
paths, limits, disabled unsafe modes and exact single-module argv. The actual SDK
manifest loader/schema data checks validate 200 operations, typed null/actual state,
required guards, permission/cancellation contracts and the real pure parameter
adapter. Addition assertions extend the same pure/VCS fixture sources with fixed
argv, address/name/branch bounds, read-only previews, context/parent/index/metadata
changes, hidden index flags and open-document rejection. They are syntax-checked
only. These checks do not execute Git, filesystem guards or async GUI behavior.
Deinitialization assertions extend the existing sources with fixed quiet argv,
normal index/flag checks, nested/conflicted rejection, ignored/untracked/hidden
files, open documents, unrecorded commits, stale preview and successful retention
postconditions. These assertions are compiled with `/Zs`, not executed.
Settings assertions cover shared URL validation, exact branch/default/dot argv,
unrelated configuration records/order, staged/unstaged preservation, stale parent
branch/preview, hidden flags, open documents, local overrides, unsupported old URL
redaction and actual URL/branch/default outcomes. They are also source-only.

Real local/remote clone/fetch/checkout/sync, partial failures, transport restrictions,
external races, SDK/plugin Runtime, keyboard/themes/DPI and Linux/macOS acceptance
remain unexecuted during the Kaspersky native-test suspension. Permanent removal,
local branch-override editing, recursive bulk workflows and submodule-specific source Undo
are not implemented. Select a checked-out module as a repository to inspect its
own configured children. See [Validation](Validation.md) and [Testing](Testing.md).

Primary references: [git-submodule](https://git-scm.com/docs/git-submodule),
[Git config output](https://git-scm.com/docs/git-config),
[Git protocol allow-list](https://git-scm.com/docs/git),
[Git deinitialization implementation](https://github.com/git/git/blob/master/builtin/submodule--helper.c),
[Git transport implementation](https://github.com/git/git/blob/master/transport.c).

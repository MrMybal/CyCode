# Multiple Git repositories and linked worktrees

The native Engine now accepts an explicit Git repository inside the workspace.
Version control offers **Automatic (active file)** and a discovered repository
selector. Automatic mode follows the nearest ordinary .git directory or gitdir
pointer within the workspace; a more specific external-provider mapping still
takes precedence. An explicit Git folder is now stored with its observed marker
identity in the existing per-workspace, per-instance desktop session. Restoration
requires the same ordinary repository identity; missing/replaced repositories
fall back to Automatic with a visible warning. Automatic mode and the workspace
root are distinct saved choices. Old sessions remain valid. The Restore last
workspace session option also controls restoring this selection. This does not
change `.cycode/vcs.json` or the SDK's explicit per-request repository selection.

Status, index actions, history, branches, remotes, stashes, text conflicts and
issued hunk previews use the same selection. Paths in documents and returned
status remain workspace-relative; only the Git argv uses repository-relative
paths. Hunk application rejects previews from another repository or identity.
Repository-wide actions require the selected directory to be the actual Git root,
rather than requiring that root to equal the workspace. Unrelated repositories'
dirty buffers do not block the selected Git working-tree action.

Discovery is explicit/background UI work, not a recursive scan on every frame.
It executes no Git process or provider callback. Bounds are 20,000 visited
directories, 200,000 inspected entries, depth 32 and 256 repositories; truncation
and up to 16 errors are reported. Common metadata/generated/cache directories
(.git, .cycode, runtime, data, node_modules, .vs, .idea, .cache, target,
Binaries, Intermediate and Saved) and redirected directories are skipped.
A repository under an excluded folder can still be selected by activating its
file. Discovery recognises both submodule and linked-worktree pointer files;
this does not implement submodule lifecycle management.

## Linked worktree controls

Version control > Worktrees uses Git's complete NUL-delimited porcelain catalogue,
including paths/lock reasons containing newlines. Entries show branch, detached
HEAD, primary/current, lock and pruning metadata. Catalogue bounds are 2 MiB and
256 records; incomplete, duplicate or unsupported fields fail explicitly.

- Create a new local branch, check out an existing local branch, or detach at a
  verified commit. The new directory must not exist and its parent must already
  exist inside the workspace. Git enforces branch availability; there is no force,
  branch reset or remote-tracking guess.
- Lock/unlock another available linked checkout using Git's administrative lock.
  This is not a source-file lock. An optional lock reason is limited to 512 bytes.
- Remove a reviewed clean linked checkout after a desktop confirmation. The
  branch remains. The main checkout, selected checkout, workspace root/ancestors,
  .git/.cycode destinations, locked/unavailable trees and open destination
  documents are refused. Normal Git remove also refuses tracked modifications,
  untracked files and submodules; no force or automatic pruning is offered.
- Open a listed checkout in a new application window using the existing launcher.
  External paths are catalogue-only: their existence is not probed and this
  workspace cannot create/remove/lock/unlock them. Opening is an explicit action.

Source/admin/common-directory identities and a catalogue revision guard every
mutation. The revision is an optimistic concurrency token, not authentication or
a cryptographic integrity claim. Target/parent identities are checked again
before dispatch. An exclusive new-directory reservation is removed after failure
only if it is still the same empty directory. Nonempty/partial checkouts and any
branch Git already created are retained for inspection.

## Reviewed move and link repair

**Move linked worktree** selects another available, unlocked linked checkout.
Enter a new workspace-relative folder whose parent already exists, choose
**Preview worktree maintenance**, then **Apply worktree maintenance**. The main
checkout, selected checkout, metadata paths, overlapping registered locations,
occupied destinations, redirected directories and open native source/destination
documents are refused. Open common worktree-metadata documents also block either
action. Git uses a directory rename; local, staged, untracked and
ignored files accompany the checkout without checkout, staging or commit. Cross-
filesystem copying is not implemented, and no force is used. Submodule index
entries or retained submodule metadata block maintenance.

**Repair moved worktree** is available on a missing registered linked path.
Select the existing folder that was moved manually. Its intact `.git` pointer
must identify this common repository's ordinary `worktrees/<id>` metadata, with
the reciprocal backlink still naming the missing old path. A changed/corrupt or
independent pointer is not adopted or guessed. Missing old parent directories
are supported; the existing ancestor identities are captured. Request repair
from an ordinary main checkout inside the workspace. Moving/copying the main
repository, bare/separate-Git-dir layouts and arbitrary damaged metadata remain
outside this supported repair workflow.

Git `repair` can inspect/update other linked checkouts even with one destination
argument. Accordingly every registered checkout must be inside the workspace;
all other checkouts must be available, unlocked and have correct ordinary
reciprocal links. Their native documents must be closed. The common worktree
directory is audited for unlisted/orphan metadata. The preview lists every
verified linked path; an external, additional missing, locked, redirected or
unreviewed record blocks the whole action. Git uses absolute linking policy;
repair can normalize otherwise-correct relative links in the reviewed set.

Both actions use a separate preview revision binding request, catalogue,
directory identities, affected pointer/HEAD/index bytes and sibling link/index
fingerprints. Editing inputs, refreshing/cancelling or changing repositories
discards the preview; stale asynchronous replies cannot apply it. The native VCS
owner rechecks all guards and closed documents immediately before one fixed Git
argv. Git transport protocols and terminal prompting are disabled. There is no
source Undo, automatic cleanup/retry/rollback or filesystem-wide transaction.

Postflight checks actual source absence, retained directory identity at the
destination, unchanged affected HEAD/index, reciprocal links and fresh catalogue
branch/commit. Repair also checks the reviewed sibling identities, HEAD/index
fingerprints and reciprocal links. Exit zero with an unconfirmed result remains
`catalogError` and produces no success notification. Partial outcomes stay visible
for inspection. These are observed guards, not a lock on external writers or
other app instances.

Bounds: 16 KiB native request, 4096 UTF-8 bytes per portable relative input path,
256 directory levels/worktree records, 200,000 index mode records, 2 MiB per Git
query and 64 MiB total reviewed index bytes. Metadata reads are bounded even if
a file grows while being read. Local maintenance inspection has an observed
45-second budget; each command has at most 15 seconds, and apply has 180 seconds.
Selected-repository/catalogue queries retain their existing separate bounds.

Native commands use argv, literal pathspecs, bounded output and deadlines
(180 seconds for add, 30 for remove/lock/unlock; inspection calls have separate
15-second bounds). Git owns its checkout/index/administrative locks and may run
configured hooks. There is no shell assembled from project data.

Each Engine serializes VCS requests. Git coordinates its own cross-process locks;
the IDE's open-document check covers this Engine, not all independent processes.
A transport error/timeout can follow partial Git work: refresh and inspect before
retrying. A post-operation catalogue error is reported separately from the command
result. OS crashes and arbitrary filesystem races are not a filesystem transaction.

## Shared SDK contract

All typed Git operations accept optional repository (workspace-relative folder,
default ".") and expectedRepository (the observed context identity). They always
select the built-in Git provider. File parameters remain workspace-relative.
Missing explicit folders fail instead of falling back to another root.

| Operation | Purpose | Permissions |
| --- | --- | --- |
| vcs_repositories | Workspace repository discovery without Git/provider execution | None |
| vcs_worktrees | Selected Git catalogue and revision | None |
| vcs_worktree_add / vcs_worktree_remove | Guarded checkout creation/removal | vcs.write, workspace.write, process.execute |
| vcs_worktree_lock / vcs_worktree_unlock | Guarded Git administrative lock | vcs.write |
| vcs_worktree_plan | Local move/repair preview and issued observed revision | None |
| vcs_worktree_move / vcs_worktree_repair | Reviewed directory move / administrative link repair | vcs.write, workspace.write, process.execute |

Mutations require expectedRevision from vcs_worktrees and are noncancellable once
dispatched. The real CyToolsCore scheduler retains the existing workspace-mutations
exclusive group; transport waits up to 600 seconds. There is no second Python Git
implementation. New maintenance operations require observed `expectedRepository`,
`expectedRevision`, path/destination and, for execution, `expectedPlanRevision`.
The preview also takes action (`move` or `repair`); apply derives it from the
method. Its typed result returns request/entry, fixed argv, checkedRoots and
explicit move/repair/local-file/index/transport effects. ABI2 exports catalogue,
preview, add/remove/move/repair and lock/unlock with the same permission distinction;
read-only provider callbacks cannot invoke mutations. C ABI is unchanged. Typed
worktree methods always select built-in Git; external providers keep their own
descriptor/action contract.
Repository-redirect environment variables (GIT_DIR, GIT_WORK_TREE, GIT_COMMON_DIR,
GIT_INDEX_FILE, GIT_OBJECT_DIRECTORY, GIT_ALTERNATE_OBJECT_DIRECTORIES) must be unset
for the built-in Git service; the application does not alter them.

## Verification and remaining work

Windows production compilation, MSVC syntax-only checks, real SDK manifest/schema
checks and source/secret checks are recorded in [Validation](Validation.md).
New assertions extend the existing opt-in cycode_vcs_tests target, not a new
executable: NUL parsing, nested path translation, unrelated dirty buffers,
cross-repository hunk rejection and actual nested index-conflict rejection,
catalogue revisions, lock/removal guards,
untracked-file preservation, retained branches and external-worktree refusal.
Move/repair assertions add portable path/command boundaries, read-only preview,
stale preview/parent identities, occupied destinations, open documents, staged/
untracked/ignored payload preservation, manual relocation, external/orphan/sibling
guards and retained index/link outcomes. Session assertions cover automatic/root/
explicit selection, version/identity/path bounds and legacy compatibility. Their
native execution remains suspended during the Kaspersky interruption.

GUI selection/focus/scrolling, themes/densities/DPI, actual worktree Git execution,
hooks, cross-instance behavior and Linux/macOS remain unexecuted. No new runtime
acceptance is claimed. Still pending: cross-window
open-document coordination, arbitrary external-root writes, worktree removal with
populated submodules,
worktree pruning, arbitrary main/bare/custom-layout repair, credential UI, interactive history editing and complex
binary/delete/rename conflicts.

Configured-module inspection and guarded init/sync/recorded-commit update now
have a separate shared service and view, with pending runtime acceptance; see
[Git submodules](GitSubmodules.md).

Command semantics follow the official [Git worktree documentation](https://git-scm.com/docs/git-worktree)
and [Git rev-parse documentation](https://git-scm.com/docs/git-rev-parse).
The [Git repair implementation](https://github.com/git/git/blob/master/builtin/worktree.c)
explains the complete linked-set audit; [index mode formatting](https://git-scm.com/docs/git-ls-files)
provides bounded submodule detection without printing every path.

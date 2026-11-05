# Git and revision control

History context actions now connect to a Replay view for one cherry-pick/revert,
with source patch review, guarded continuation after conflict resolution and
quit tracking with retained local work/index. Source IDs, complete index/state,
documents and persistent intent receipts share native/UI/SDK/ABI2 ownership.
Actual Git/GUI acceptance remains pending; see [Git replay](GitReplay.md).

The Merge view now connects actual conflict/staged review with previewed unsigned
commit completion and quit tracking while retaining local files/index. It shares
native SDK/ABI2 ownership, explicit hooks policy, persistent intent receipts and
actual ordered-parent/tree/index checks. Quit is distinct from abort/reset;
autostash/other operations remain external. Runtime acceptance is pending; see
[Merge workflow](GitMergeWorkflow.md).

Separate working-file preview/apply/Undo now follows index conflict resolution:
raw staged-byte installation or absent-index removal reuses persistent trash and
common resource guards, with closed affected text/metadata and explicit partial
outcomes. Automatic rename topology and Git filter/EOL checkout semantics remain
outside this action. Actual filesystem/Git/GUI acceptance is pending; see
[Git working files](GitWorkingFiles.md).

Git conflicts now expose actual mode/OID stages, absent sides and binary-safe
inspection without forcing every path into a text document. Preview/apply supports
explicit index choices across a group of rename-related paths while preserving
local files and open buffers. This does not perform physical checkout/rename/
deletion or infer rename topology. Native/UI/SDK/ABI2 share guards and actual
postflight; real Git/GUI acceptance remains pending. See
[Git conflicts](GitConflicts.md).

Repository selection now restores through the existing desktop session with an
identity check. Linked worktree move and manually-moved link repair have separate
preview/apply controls shared with the SDK/ABI2. They retain local files/index and
verify actual metadata/catalogue outcomes; repair audits all linked paths before
Git can update their links. Runtime/GUI acceptance is pending. See
[Git worktrees](GitWorktrees.md).

Configured Git submodules now have a selected-repository catalogue and reviewed
init/sync/recorded-commit update and separate preview/clone-and-stage addition,
shared with the SDK/MCP and ABI2 plugin host.
Separate deinitialization preview/action protects ignored/untracked files and
open native documents while retaining the project gitlink/metadata and Git history.
URL and tracking-branch editing now has a separate guarded preview/apply workflow,
preserving existing metadata/index changes and actual checkout state.
See [Git submodules](GitSubmodules.md) for guards, transport/ignored-file limits,
actual outcomes and pending runtime acceptance. External providers keep their
own actions; these new typed methods explicitly target built-in Git.

The panel now selects multiple/nested Git roots and manages linked worktrees
through the shared native Engine and typed SDK. Automatic selection follows the
active file; an explicit selector overrides it for the current workspace window.
See [Git repositories and worktrees](GitWorktrees.md) for controls, guards,
permissions and compile-only validation of this new milestone.

Text-hunk staging/unstaging is available in Version control > Hunks through
the shared Diff view and `vcs_hunks` / `vcs_apply_hunks` MCP operations. Issued
previews expire and reject changed index/worktree data or dirty buffers. Working
files stay untouched. Added/deleted/binary/mode-changing files retain whole-file
staging. See [Development workflows](DevelopmentWorkflows.md).

The native Version control panel shares its services with MCP. Git is built in;
external providers keep the versioned C ABI and declare supported actions. The
bundled Subversion plugin supplies status/diff/history/blame, file add/revert and
mapped-copy commit/update with a separately installed SVN client. See
[Subversion](Subversion.md) for scope and compile-only validation. A bundled
[Perforce provider](Perforce.md) also supplies checked-client status/review,
single-file checkout/add/reopen/revert/submit/locks and guarded project sync.
A bundled [Mercurial provider](Mercurial.md) supplies status/review, exact-file
add/forget/revert, commit, clean update, bookmark/named-branch creation and
configured-alias pull/push. It requires an installed Hg client; real-client and
GUI validation remain pending.
The bundled [Diversion provider](Diversion.md) adds status/review, commits, file
reset, branch creation, clean checkout/update and locks, preserving automatic
tracking/sync without a staging or push/pull fiction. It requires a configured
Diversion client; real client/agent/GUI acceptance remains pending. The
[provider contract](CustomVersionControl.md) also supports future independent
systems and their own actions/menus.

The panel provides changes, whole-file staging/unstaging, commits, unified diff
and blame review, structured history, local/remote branches, safe branch deletion,
remote URLs, fetch/prune, push/tracking, fast-forward pull, merge and stashes.
Rider Ctrl+K opens Changes; Ctrl+Shift+K opens Remotes and sync. The command palette
also exposes Git branches/history/remotes/stashes/conflicts and file diff/blame.

Changing branches or merging requires saved live buffers and no tracked disk
changes. Stashing requires saved buffers, and includes untracked files only when
explicitly selected. Git itself prevents overwriting conflicting untracked paths.
After a working-tree operation, changed clean buffers reload through the document
service with new revisions and refreshed clangd overlays. Buffers edited during
the operation are retained; deleted/unreadable files are reported as retained.
Repository-wide changes require the selected Git directory to be its actual root.
Document paths remain workspace-relative; unrelated repositories' dirty buffers
are outside the selected Git action's preflight/reload scope.

Pull fetches then merges with --ff-only, rejecting divergence. Push never forces.
Git uses its configured credential helpers, signing and hooks. Authentication
dialogs, interactive rebasing, force operations and conflict-abort controls are not
provided yet. Process calls use argv, bounded output and deadlines; network calls
can take up to 180 seconds. Cancellation currently waits for native completion.

For a normal text merge conflict, Resolve conflict reads the index stages: base,
ours and theirs. The panel shows neutral, read-only source views; edit the working
file in its normal editor tab. Use ours/theirs updates the live buffer only, guarded
by the displayed revision. Refresh conflict obtains current revisions. Save and
mark resolved explicitly saves and stages the working file, rejecting remaining
conflict markers and external disk changes. It does not commit the merge.
Binary, oversized, deleted-working-file and complex rename/delete conflicts still
need external Git tools; the three-way editor does not implement those workflows.

Git metadata/history/diff/blame/conflict reads are exposed through typed vcs_* MCP
operations. These SDK operations explicitly target the selected workspace Git
folder (repository, default "."; optional expectedRepository identity),
accept only `provider:"git"` and keep their exact Git schemas/permissions. External
providers use `vcs_provider_action` with its advertised actions and existing generic
plugin/process/workspace/VCS permissions, including for reads. Generic provider
calls wait up to 600 seconds for bounded sequential native preflight/network work.
Writes require vcs.write; working-file mutations also require workspace.write.
Network, hook-bearing working-tree operations and explicit conflict resolution
require process.execute. Each schema documents its permissions and parameters.
Process results contain exitCode/output/timedOut/truncated; clients must inspect
exitCode, including merge conflicts. Working-tree results additionally report
reloaded/retained paths. Native exceptions become failed Core jobs.

The opt-in native vcs CTest creates owned repositories and a local bare remote. It checks
branches, reload/revision guards, dirty buffer/tree rejection, stash, fetch/pull/push,
tracking divergence, history/blame and actual conflicting commits with revision-safe
resolution. The official MCP integration exercises the same native service and
permission checks. app/scripts/verify_vcs.py is the explicit GUI acceptance suite.
New nested-repository/worktree assertions have been syntax-checked only; native
and graphical runs remain suspended during the antivirus interruption. Tests
never contact or modify a user remote. See [Testing](Testing.md).

Project-specific provider selection is persisted with guarded writes in
.cycode/vcs.json. The deepest root mapping for the active file drives
native routing and the available GUI menus; generic MCP provider calls use the same
selection. Typed SDK Git operations remain explicit as described above.
Git is the built-in default. An extension can
advertise its own checkout/changelist/lock-style actions and text parameters;
Git-specific controls are hidden for other providers. Unavailable providers fail
explicitly without a fallback write to Git. File actions validate workspace/mapping
containment and declared required fields before calling the provider.
See [Plugin SDK](Plugins.md) for descriptors, status contracts and permissions.

Remaining workflows include arbitrary staging-line selection, interactive rebase and multi-commit/mainline replay,
worktree pruning and arbitrary main/bare/custom-layout repair, submodule existing-repository adoption/permanent removal and recursive bulk actions,
credential UI and additional
third-party providers. Linked-worktree writes are limited to this workspace.
The alternative provider used by existing native/UI tests remains a fixture.
The bundled SVN, Perforce, Mercurial and Diversion plugins have separate runner/ABI regression sources;
real clients/servers and graphical execution remain unverified.

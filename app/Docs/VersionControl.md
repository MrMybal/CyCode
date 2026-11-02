# Git and revision control

The native Version control panel shares its services with MCP. Git is built in;
external providers keep the versioned C ABI and declare supported actions. No
Perforce/SVN/Mercurial implementation is bundled.

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
Repository-wide changes require the workspace to be the repository root.

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

Metadata/history/diff/blame/conflict reads are exposed through vcs_* MCP operations.
Writes require vcs.write; working-file mutations also require workspace.write.
Network, hook-bearing working-tree operations and explicit conflict resolution
require process.execute. Each schema documents its permissions and parameters.
Process results contain exitCode/output/timedOut/truncated; clients must inspect
exitCode, including merge conflicts. Working-tree results additionally report
reloaded/retained paths. Native exceptions become failed Core jobs.

The native vcs CTest creates owned repositories and a local bare remote. It checks
branches, reload/revision guards, dirty buffer/tree rejection, stash, fetch/pull/push,
tracking divergence, history/blame and actual conflicting commits with revision-safe
resolution. The official MCP integration exercises the same native service and
permission checks. Run app/scripts/verify_vcs.py for real GUI shortcut/panel checks
and dark/light conflict captures. Tests never contact or modify a user remote.

Remaining workflows include hunk staging, interactive rebase/cherry-pick UI,
worktrees/multiple roots, credential UI and broader VCS provider validation.

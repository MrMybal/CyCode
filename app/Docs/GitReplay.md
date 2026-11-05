# Reviewed cherry-pick and revert

**Version control > Replay** now provides single-commit cherry-pick, revert,
continuation after conflict resolution and quit tracking. These actions share one
native owner across desktop, CyTools/MCP and ABI2 plugins. The existing Merge and
Conflicts views retain their separate responsibilities.

## Choose and review a source

Right-click a built-in Git **History** commit and choose **Cherry-pick commit** or
**Revert commit**. The same view accepts a complete lowercase 40/64-character
object ID. Branch names, shortened IDs, ranges, options and arbitrary argv are
rejected. The object is verified as that exact commit, without replacement refs.
Root/ordinary commits are supported; merge commits requiring mainline selection
remain external. Source messages must be valid UTF-8 without NUL, up to 16 KiB.

**Preview replay action** binds repository/state identity, source object, source
message/parents, paths/stat, previous HEAD, action and fixed arguments. **Review
source commit patch** opens the common Diff review. These source changes describe
the original commit, not a simulation of the resulting worktree. Revert reverses
their effect. Context differences can produce conflicts, empty changes or failure.

Start requires a committed HEAD, no active operation, a clean tracked tree/index,
ordinary index flags and closed repository documents. Untracked files are not
added or included in an exact workspace snapshot; Git protects colliding paths.
Previews reject changes that rewrite the active workspace's `.cycode` or Git
administrative metadata. No force, reset, automatic stash or implicit push is used.

## Conflicts, continue and quit

After a failed cherry-pick/revert, refresh the actual state. A single pending
CHERRY_PICK_HEAD or REVERT_HEAD exposes **Review replay conflicts**, actual
conflict paths and separate blockers. Resolve/stage through the common conflict
services, synchronize working files where needed and close repository documents.

**Continue resolved replay** reviews the whole current staged index, including
unrelated staged changes. **Review staged replay diff** opens the shared Diff
view. Continuation requires resolved conflicts, tracked files matching the index
and ordinary index flags. The eventual committed tree must match this reviewed
index exactly. Empty changes are not automatically skipped or forced into an
empty commit; inspect Git's actual result.

**Quit replay tracking** previews an explicit Git cherry-pick/revert --quit.
It clears operation bookkeeping while retaining working files and index entries;
unsaved source buffers and unresolved index stages may remain. This does not
restore pre-operation files. It differs from --abort/--skip/reset, which are not
provided by this owner. Multiple-commit sequencers, rebase, concurrent operation
markers, autostash and open administrative/managed receipt documents block the
applicable workflow. External mainline/merge-commit replay remains external even
when stopped.

## Execution and outcomes

Apply rechecks repository/state/plan revisions and index/flags before dispatch.
A persistent, exclusive-created JSON intent is written under
`.cycode/merge-receipts/replay-*.json`, sharing the common closed-metadata guard.
It records the request and reviewed context before the command. An intent is not
proof of success, an automatic resume token, a full backup or an editor Undo.
It is not automatically removed or exported as a temporary SDK task artifact.

Commits are unsigned. Configured hooks, editor requests, rerere, automatic Git
maintenance and Git transport are disabled. Trusted configured filters/merge
drivers can still run project tools, including during read-only inspection.
This is not an OS sandbox. Repository document closure protects native live
buffers during writing operations; it does not coordinate other editors/processes.

The mutation has a 120-second native-owned process limit and bounded actual
output. Actual exitCode, output, timedOut and truncated are retained. Conflicts
can leave a valid pending operation with a nonzero process status. After success,
postflight verifies removed operation state, exactly one new commit parent equal
to reviewed previous HEAD, actual raw commit/tree/current index and synchronized
tracked files. Continue additionally verifies the exact reviewed staged tree.
Quit checks retained HEAD/index records/flags. Working-file retention on quit
follows Git's operation semantics, not a bytewise snapshot of every file.

An inspectionError keeps uncertainty explicit even after exit zero. There is no
automatic retry, rollback or success inference from a receipt. External Git/tools
can race preflight; the native VCS mutex serializes local callers only, without
global compare-and-swap. Inspect actual state before another mutation.

## Shared contracts and bounds

| SDK operation | Native / ABI2 method | Effect |
|---|---|---|
| vcs_replay_state | vcs.replay.state | Actual kind/source/conflicts/index and start/continue/quit blockers |
| vcs_replay_plan | vcs.replay.plan | action + observed repository/state; commit ID for start, empty for continue/quit |
| vcs_replay_apply | vcs.replay.apply | Same reviewed request and expectedPlanRevision |

State/plan have no write permissions. Apply requires vcs.write, workspace.write
and process.execute, shares the existing workspace mutation group and retains
noncancellable native ownership. The bridge waits up to 1,200 seconds to cover
the repeated guarded inspections plus mutation/postflight; it does not release
ownership merely on a cancellation request. Typed Git services reject alternative
VCS providers, whose plugins retain their own actions/permissions.

The common inspector is reused rather than duplicating index/document snapshots.
Each replay snapshot checks two common observations around actual marker reads.
Common inspection budgets are 45 seconds, checked before each 15-second command;
a running command can finish later. Other inspection commands have 15-second
limits. Raw objects/index listings are bounded to 8 MiB, index to 100,000 records,
changed paths to 2 MiB/2,048 records and stat to 1 MiB. Common metadata bounds and
ordinary-layout requirements follow [Merge workflow](GitMergeWorkflow.md).
No new dependency, executable target, CyUI module or C ABI is introduced.

## Validation and remaining work

Windows MSVC /Zs and production builds with BUILD_TESTING=OFF, actual SDK manifest
loading and JSON Schema data checks are distinct from acceptance execution.
Opt-in existing VCS fixture sources specify invalid IDs, stale/open-document
guards, default hook suppression, successful cherry-pick/revert, conflict
continuation and dirty-buffer/index/disk preservation on quit. These native
assertions have not run during the existing Kaspersky suspension.

Actual Git/filter/driver/hook/receipt/filesystem/concurrency, SDK Runtime/plugin/
GUI/keyboard/themes/DPI and Linux/macOS remain unverified. Multi-commit/range/
mainline replay, interactive rebase, abort/skip recovery, configurable signing/
hooks and result-tree simulation remain outside this owner.

Semantics follow Git's primary [cherry-pick](https://git-scm.com/docs/git-cherry-pick),
[revert](https://git-scm.com/docs/git-revert) and
[diff-tree](https://git-scm.com/docs/git-diff-tree) documentation.

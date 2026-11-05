# Reviewed merge completion and quit

**Version control > Merge** now joins conflict resolution, working-file review and
final merge actions through one native/UI/SDK/ABI2 owner. Starting a merge retains
the existing branch workflow. This view inspects and completes an already active
merge, or explicitly quits its tracking while retaining local work.

## Inspect and review

**Refresh merge state** reads actual operation markers, HEAD/branch/admin identity,
complete staged mode/OID/stage records, index flags and native document revisions.
It lists unresolved paths, incoming merge parents, staged paths/stat and blockers.
**Review merge conflicts** opens the existing conflict view; **Review staged merge
diff** uses the common Diff review. Working files can then be synchronized through
the separate [working-file service](GitWorkingFiles.md).

Completion requires one incoming parent, all conflicts resolved, tracked working
files matching the index, saved/reconciled repository buffers and closed Git
metadata/managed receipt documents. Skip-worktree, assume-unchanged and
intent-to-add entries block this reviewed completion. Autostash and another
active rebase/apply/cherry-pick/revert/sequencer operation block both actions;
handle those through their actual Git workflow. A changed merge-origin HEAD is
also refused. Inspection is read-only and refresh is explicit.

## Complete the merge commit

Choose **Complete merge commit**, edit the message and **Preview merge action**.
The first MERGE_MSG line seeds an editable subject; user edits survive a refresh
within the same merge. Preview binds the current repository/state, complete staged
index, ordered expected parents, message, hooks policy and effects. The whole
index is included, including unrelated paths staged intentionally or accidentally;
review the full list. Untracked files are not added.

**Apply reviewed merge action** writes an intent receipt, rechecks the snapshot
and executes a fixed Git commit command with a message file, verbatim cleanup and
explicitly disabled signing. The message is valid UTF-8 up to 16 KiB; nonempty
content is required. This workflow currently creates unsigned commits. It never
uses a project-derived shell command, starts an editor or performs a push.

Configured commit hooks default to disabled. **Run configured Git commit hooks**
is an explicit reviewed choice and requires repository documents to be closed.
Those trusted hooks may run tools, change files/index/message, access the network
or fail. They are not an OS sandbox. Their actual process status and any changed
postconditions remain visible. Native Git transport, fsmonitor, automatic
maintenance, rerere and editor requests are disabled for this owner.

Postflight checks that no operation is still active, then reads the raw commit
object to verify actual ordered parents independently of log signature/notes
settings or grafted history. Replacement refs are disabled for this owner. It
compares the committed recursive tree and current index mode/OID/stage/flag
records against the preview. A zero exit code can still carry inspectionError
when the reviewed outcome cannot be confirmed. The result retains the actual
process status and available new commit identity rather than inventing success.

## Quit merge tracking while retaining local work

Choose **Quit merge tracking**, preview its effects and apply explicitly. This
executes Git's merge --quit. It clears merge bookkeeping while retaining working
files and indexed content; unresolved entries and unsaved source buffers may
remain. The next ordinary commit will no longer use those recorded merge parents.
The original merge message/parents are copied into the intent receipt first.

This is distinct from merge --abort, which attempts to restore pre-merge state.
No reset or reconstruction is performed here. Autostash is refused because Git's
quit operation can otherwise alter the stash list. Postflight verifies the removed
active state and retained HEAD/index records and flags. Working-file preservation
follows the requested Git operation; it is not a bytewise snapshot of every local
or untracked file.

## Ownership, receipts and limits

| SDK operation | Native / ABI2 host method | Inputs/effect |
|---|---|---|
| vcs_merge_state | vcs.merge.state | Selected built-in Git repository; actual blockers/revision |
| vcs_merge_plan | vcs.merge.plan | action, expectedRepository, expectedRevision; commit message and optional hooks |
| vcs_merge_commit | vcs.merge.commit | Matching commit request plus expectedPlanRevision |
| vcs_merge_quit | vcs.merge.quit | Matching quit request, empty message/no hooks, same guards |

State/preview are read-only. Apply requires vcs.write, workspace.write and
process.execute, shares the SDK workspace mutation group and retains
noncancellable native ownership. The bridge keeps its existing 600-second wait.
Typed contracts reject external VCS providers; plugin actions retain their
separate permissions. The native VCS mutex serializes local callers. No C ABI,
dependency, executable target or CyUI module changes are needed.

Intent receipts/message files are persistent workspace metadata under
`.cycode/merge-receipts`, separate from temporary SDK task artifacts. Files are
created exclusively before Git dispatch; they are not automatically removed or
exported. A retained intent is evidence of a request, not proof of completion and
not an automatic resume/Undo handle. Commit/quit have no editor Undo, automatic
retry or rollback. External Git/editors/hooks can race preflight; no global lock
or atomic compare-and-swap is claimed. Inspect actual state after uncertainty.

Each inspection uses a checked 45-second budget and 15-second subprocess timeouts;
a running call can finish after the budget. The mutation timeout is 120 seconds,
followed by a fresh postflight inspection budget. Listings are bounded to 8 MiB/
100,000 index records, 2 MiB/2,048 changed paths and a 1 MiB stat. Merge-parent
metadata supports 16 OIDs, with completion limited to one incoming parent.
Metadata files are capped at 16 KiB, except 4 KiB HEAD/MERGE_HEAD, 32 KiB
COMMIT_EDITMSG and 64 KiB MERGE_RR. The edit-message allowance covers Git's appended
newline at the accepted request limit. Ordinary Git filesystem layouts are
required. Exceeding bounds fails explicitly.

## Validation and remaining scope

Windows production compilation, MSVC /Zs and actual CyToolsCore/JSON Schema data
checks are used during the existing Kaspersky native-execution suspension.
Existing opt-in VCS fixture sources now specify parent/path/tree parsing, stale
index/message rejection, intent-to-add blocking, dirty-buffer/index/disk
preservation on quit, open metadata guards, configured hook failure/default
suppression, exact-limit messages and actual raw committed parents/tree outcomes.
These assertions have not run.

Actual Git/receipt/hook/filesystem/concurrency behavior, SDK/plugin/GUI keyboard/
themes/DPI and Linux/macOS remain unverified. Signed completion, autostash
resolution, octopus completion, abort/reset recovery and interactive rebase remain
outside this owner. See [Testing](Testing.md), [Validation](Validation.md) and
[Roadmap](Roadmap.md).

Operation semantics follow Git's primary [merge documentation](https://git-scm.com/docs/git-merge),
[commit documentation](https://git-scm.com/docs/git-commit) and
[diff documentation](https://git-scm.com/docs/git-diff). Object verification uses
the documented [raw cat-file output](https://git-scm.com/docs/git-cat-file) with
[replacement objects disabled](https://git-scm.com/docs/git).

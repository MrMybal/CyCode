---
id: api-vcs
title: Version control
description: Public operations for Version control.
section: API and extensions
parent: api-links
order: 101
revision: 2
---

Public operations for Version control.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`vcs_replay_state`](doc:operation-vcs-replay-state-vcs-replay-state) — Inspect selected Git state for single-commit cherry-pick/revert, reusing bounded complete merge/index/flags/metadata/document inspection.
- [`vcs_replay_plan`](doc:operation-vcs-replay-plan-vcs-replay-plan) — Preview one cherryPick/revert using a full 40/64-character lowercase commit object ID, or continue/quit the active single-commit replay with an empty commit field.
- [`vcs_replay_apply`](doc:operation-vcs-replay-apply-vcs-replay-apply) — Apply the matching reviewed replay request with repository/state/plan guards, persistent intent receipt and pre-dispatch index/flag recheck.
- [`vcs_merge_state`](doc:operation-vcs-merge-state-vcs-merge-state) — Inspect current merge/other-operation markers, full staged index/flags, HEAD/branch/admin identity, dirty buffers and unstaged tracked changes.
- [`vcs_merge_plan`](doc:operation-vcs-merge-plan-vcs-merge-plan) — Review commit or quit against observed repository/state guards.
- [`vcs_merge_commit`](doc:operation-vcs-merge-commit-vcs-merge-commit) — Complete the exact reviewed single-parent-incoming merge through Git commit --file --cleanup=verbatim --no-gpg-sign.
- [`vcs_merge_quit`](doc:operation-vcs-merge-quit-vcs-merge-quit) — Quit the exact reviewed active merge through Git merge --quit, preserving working files and index entries rather than restoring pre-merge content.
- [`vcs_checkout_plan`](doc:operation-vcs-checkout-plan-vcs-checkout-plan) — Preview a single working-file action, without changing index/files.
- [`vcs_checkout_apply`](doc:operation-vcs-checkout-apply-vcs-checkout-apply) — Apply an issued working-file preview after repository/full-index/resource/documents rechecks.
- [`vcs_checkout_undo`](doc:operation-vcs-checkout-undo-vcs-checkout-undo) — Undo one completed native working-file receipt without changing Git index.
- [`vcs_conflict_list`](doc:operation-vcs-conflict-list-vcs-conflict-list) — Catalogue actual unresolved Git index stages, including modify/delete, add/add and multiple rename-related paths, without opening text documents.
- [`vcs_conflict_inspect`](doc:operation-vcs-conflict-inspect-vcs-conflict-inspect) — Read one observed conflict version set safely.
- [`vcs_conflict_plan`](doc:operation-vcs-conflict-plan-vcs-conflict-plan) — Preview one to 32 explicit index resolutions from observed stages.
- [`vcs_conflict_apply`](doc:operation-vcs-conflict-apply-vcs-conflict-apply) — Apply exactly the reviewed index resolution in one native-owned Git update-index invocation.
- [`vcs_worktree_plan`](doc:operation-vcs-worktree-plan-vcs-worktree-plan) — Preview moving or repairing one linked worktree inside the workspace.
- [`vcs_worktree_move`](doc:operation-vcs-worktree-move-vcs-worktree-move) — Move one reviewed linked checkout with fixed Git worktree move -- source destination and no force, transport, staging or commit.
- [`vcs_worktree_repair`](doc:operation-vcs-worktree-repair-vcs-worktree-repair) — Repair one reviewed manually-moved linked checkout with fixed Git worktree repair -- destination.
- [`vcs_submodule_settings_plan`](doc:operation-vcs-submodule-settings-plan-vcs-submodule-settings-plan) — Preview changing one configured Git submodule URL or remote tracking branch.
- [`vcs_submodule_settings`](doc:operation-vcs-submodule-settings-vcs-submodule-settings) — Apply one reviewed URL/tracking-branch metadata change with the same inputs and expectedPlanRevision from vcs_submodule_settings_plan.
- [`vcs_submodule_deinit_plan`](doc:operation-vcs-submodule-deinit-plan-vcs-submodule-deinit-plan) — Preview deinitializing one registered absorbed Git submodule at its recorded commit.
- [`vcs_submodule_deinit`](doc:operation-vcs-submodule-deinit-vcs-submodule-deinit) — Remove one reviewed submodule checkout and its local configuration through Git submodule deinit --quiet -- path, without force/all/recursion or Git transports.
- [`vcs_submodule_add_plan`](doc:operation-vcs-submodule-add-plan-vcs-submodule-add-plan) — Preview adding one new Git submodule without clone/fetch or staging.
- [`vcs_submodule_add`](doc:operation-vcs-submodule-add-vcs-submodule-add) — Clone and stage one reviewed new Git submodule.
- [`vcs_submodules`](doc:operation-vcs-submodules-vcs-submodules) — Inspect configured top-level Git submodules in the selected workspace repository.
- [`vcs_submodule_init`](doc:operation-vcs-submodule-init-vcs-submodule-init) — Register one configured indexed submodule URL locally using Git init, without downloading or checking out files.
- [`vcs_submodule_sync`](doc:operation-vcs-submodule-sync-vcs-submodule-sync) — Synchronize one registered submodule URL from .gitmodules using Git sync.
- [`vcs_submodule_update`](doc:operation-vcs-submodule-update-vcs-submodule-update) — Update one configured indexed submodule to the parent index commit via Git update --init --checkout --jobs=1, without force, recursion, remote-tip selection, merge, rebase or custom update commands.
- [`vcs_repositories`](doc:operation-vcs-repositories-vcs-repositories) — Discover ordinary .git directories and linked/submodule pointer files inside this workspace without executing Git or loading provider callbacks.
- [`vcs_worktrees`](doc:operation-vcs-worktrees-vcs-worktrees) — Read the selected repository worktree list via complete porcelain -z records.
- [`vcs_worktree_add`](doc:operation-vcs-worktree-add-vcs-worktree-add) — Create a linked checkout in a NEW workspace directory with an existing parent.
- [`vcs_worktree_remove`](doc:operation-vcs-worktree-remove-vcs-worktree-remove) — Remove one reviewed, available linked worktree INSIDE the workspace.
- [`vcs_worktree_lock`](doc:operation-vcs-worktree-lock-vcs-worktree-lock) — Lock another available linked worktree inside the workspace against Git pruning/removal.
- [`vcs_worktree_unlock`](doc:operation-vcs-worktree-unlock-vcs-worktree-unlock) — Explicitly remove the Git worktree administrative lock from another available linked worktree inside the workspace.
- [`vcs_inspect`](doc:operation-vcs-inspect-vcs-inspect) — Read branches, remotes, tracking divergence and stashes without changing the repository.
- [`vcs_init`](doc:operation-vcs-init-vcs-init) — Initialize a Git repository in the selected existing workspace directory with an initial main branch; refuses existing Git markers and bare repositories.
- [`vcs_history`](doc:operation-vcs-history-vcs-history) — Read structured commit history affecting this workspace.
- [`vcs_show`](doc:operation-vcs-show-vcs-show) — Read a verified commit and its workspace diff.
- [`vcs_blame`](doc:operation-vcs-blame-vcs-blame) — Read line attribution for a workspace document.
- [`vcs_branch_create`](doc:operation-vcs-branch-create-vcs-branch-create) — Create a branch at a verified commit.
- [`vcs_branch_switch`](doc:operation-vcs-branch-switch-vcs-branch-switch) — Switch branch only with saved buffers and a clean tracked working tree.
- [`vcs_branch_rename`](doc:operation-vcs-branch-rename-vcs-branch-rename) — Rename a local branch.
- [`vcs_branch_delete`](doc:operation-vcs-branch-delete-vcs-branch-delete) — Delete a merged local branch using Git safe deletion; force deletion is unavailable.
- [`vcs_remote_add`](doc:operation-vcs-remote-add-vcs-remote-add) — Add a named Git remote URL without contacting it.
- [`vcs_remote_set`](doc:operation-vcs-remote-set-vcs-remote-set) — Set a remote URL without contacting it.
- [`vcs_remote_remove`](doc:operation-vcs-remote-remove-vcs-remote-remove) — Remove a remote and its tracking references.
- [`vcs_fetch`](doc:operation-vcs-fetch-vcs-fetch) — Fetch and prune the selected remote.
- [`vcs_pull`](doc:operation-vcs-pull-vcs-pull) — Fetch then fast-forward the selected remote branch, refusing dirty buffers, tracked changes or divergence.
- [`vcs_push`](doc:operation-vcs-push-vcs-push) — Push the selected local branch without force, honoring Git hooks.
- [`vcs_merge`](doc:operation-vcs-merge-vcs-merge) — Merge a verified commit into the current branch with saved buffers and a clean tracked tree.
- [`vcs_stash_save`](doc:operation-vcs-stash-save-vcs-stash-save) — Stash tracked working-tree changes, optionally including untracked files.
- [`vcs_stash_apply`](doc:operation-vcs-stash-apply-vcs-stash-apply) — Apply a stash on a clean tracked tree; conflicts remain for explicit resolution.
- [`vcs_stash_pop`](doc:operation-vcs-stash-pop-vcs-stash-pop) — Apply a stash on a clean tracked tree and remove it only after Git succeeds.
- [`vcs_stash_drop`](doc:operation-vcs-stash-drop-vcs-stash-drop) — Permanently drop the explicitly selected stash entry.
- [`vcs_conflicts`](doc:operation-vcs-conflicts-vcs-conflicts) — Read base, ours, theirs and the live working buffer for an unresolved text conflict at repository root.
- [`vcs_resolve`](doc:operation-vcs-resolve-vcs-resolve) — Save and stage a conflict resolution using the live expectedRevision.
- [`vcs_status`](doc:operation-vcs-status-vcs-status) — Inspect Git repository status.
- [`vcs_diff`](doc:operation-vcs-diff-vcs-diff) — Read working tree or staged differences.
- [`vcs_log`](doc:operation-vcs-log-vcs-log) — Read the latest 30 Git commits affecting the workspace.
- [`vcs_stage`](doc:operation-vcs-stage-vcs-stage) — Stage one workspace path.
- [`vcs_hunks`](doc:operation-vcs-hunks-vcs-hunks) — Issue bounded text hunks for a tracked file.
- [`vcs_apply_hunks`](doc:operation-vcs-apply-hunks-vcs-apply-hunks) — Stage selected issued hunks, or reverse staged hunks back out of the index.
- [`vcs_unstage`](doc:operation-vcs-unstage-vcs-unstage) — Unstage one workspace path.
- [`vcs_commit`](doc:operation-vcs-commit-vcs-commit) — Commit the existing Git index, honoring repository hooks and signing settings.
- [`vcs_providers`](doc:operation-vcs-providers-vcs-providers) — List available version control providers and their declared capabilities/actions.
- [`vcs_context`](doc:operation-vcs-context-vcs-context) — Resolve the version control provider for a workspace path using persisted most-specific project mappings.
- [`vcs_configure`](doc:operation-vcs-configure-vcs-configure) — Persist a version control provider mapping for a project folder; detects concurrent configuration changes.
- [`vcs_provider_action`](doc:operation-vcs-provider-action-vcs-provider-action) — Run an advertised action of an installed external version control provider.

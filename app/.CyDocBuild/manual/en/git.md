---
id: git
title: Review changes with Git
description: Select a repository, review diffs, stage and commit.
section: Work in CyCode
order: 230
parent: version-control
revision: 2
---

## Review and commit {#changes}

1. Select the intended repository in Version control, especially in a multi-repository workspace.
2. Inspect working-file differences or the staged version in the shared Review view.
3. Stage a file, or select eligible text hunks for partial staging.
4. Review the index, enter a commit message and commit deliberately.

Commit uses Git's hooks and signing settings. Unsaved editor text and the disk/index are different states. Save before operations that require reconciled files.

## Branches and conflicts {#branches}

Branch, remote, fetch/pull/push, stash, merge and selected cherry-pick/revert actions use reviewed forms. For conflicts, inspect available versions and the proposed index resolution before applying. An index resolution does not necessarily replace working files or remove text conflict markers. The separate working-file application has its own preview and retained backup.

Worktree and submodule actions also require the selected repository and a current preview. Git history operations do not become editor Undo transactions; inspect a failed or uncertain result before retrying.

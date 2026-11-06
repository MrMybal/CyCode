---
id: git-conflicts
title: Resolve conflicts and replay commits
description: Review merge results and rebase/cherry-pick plans.
section: Work in CyCode
parent: git
order: 315
revision: 1
---

## Resolve a conflict {#resolve}

1. Save the relevant editor buffers and refresh the selected Git repository.
2. Select a conflicted file and inspect its base/ours/theirs versions and conflict blocks.
3. Prepare the desired text resolution. Review the resulting content and affected index state.
4. Apply while the preview is current. Refresh status and inspect the remaining conflicts before finishing the merge.

Unsupported binary, encoding or complex conflict shapes remain explicit; use an external Git tool for those cases. A stale source/index observation requires a fresh preview. Do not assume an editor Undo can restore repository operations.

## Merge, rebase and cherry-pick {#replay}

Inspect the current operation state first. Replay actions expose an examined plan before execution; use the offered continue/skip/abort action for the actual operation and state. Branches, index changes and Git hooks follow Git's normal behaviour.

For a merge, review its finalisation plan before creating the merge commit or quitting. These actions can change tracked disk/index data and create commits. If the tool reports an incomplete result, inspect the repository state before retrying. [Operation contracts](doc:api-vcs#operations) expose the separate state/plan/apply calls for automation.

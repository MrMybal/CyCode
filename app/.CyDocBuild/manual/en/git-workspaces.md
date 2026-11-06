---
id: git-workspaces
title: Repositories, worktrees and submodules
description: Select the right Git root before changing linked checkouts.
section: Work in CyCode
parent: git
order: 314
revision: 1
---

## Select a repository {#repositories}

Version control offers Automatic (active file) and discovered repositories. Automatic follows the active file's nearest Git root; a more specific external-provider mapping takes precedence. Verify the displayed root before staging, committing or running repository-wide actions. Paths shown by CyCode stay workspace-relative.

## Linked worktrees {#worktrees}

Open Worktrees and refresh. Create a new branch checkout, an existing local branch checkout or a detached checkout at a verified commit. The new folder must not exist and its parent must exist inside the workspace. Lock/unlock is Git's administrative checkout lock, not a file-content lock.

Remove requires a reviewed clean linked checkout; it preserves the branch. Main/current, locked or unavailable checkouts and locations with open destination documents are refused. Move and link-repair actions use an explicit preview whose catalogue/source identities must remain current. Opening a listed checkout in a new application window is a separate action.

## Submodules {#submodules}

Open Submodules and select one top-level module. Register records its URL locally; Synchronise updates that registration. Update to recorded commit checks out the commit recorded in the parent index, normally with detached HEAD. Clone/fetch requires its explicit checkbox.

Add submodule first previews the address, name, branch and new folder. The separate Clone and stage action stages `.gitmodules` and the gitlink but creates no parent commit. Review partial results after failure. Cancel discards a preview before dispatch; Git side effects are not editor Undo transactions.

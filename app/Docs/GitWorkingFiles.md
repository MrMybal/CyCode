# Working files after Git conflict resolution

The conflict resolver stages selected versions without changing local files.
**Version control > Conflicts > Synchronize working file** adds the explicit next
step: install exact staged bytes or remove a working file already absent from the
index. Binary files never enter editor buffers or JSON text strings.

## Review, apply and Undo

Choose a reviewed conflict path or enter another repository-relative file. **Copy
staged bytes** requires one normal stage-zero blob; unresolved paths, symlinks and
gitlinks are refused. **Remove untracked working file** explicitly requires no
index entry. Close affected text documents and documents inside `.cycode/trash`.
**Preview working-file action** shows original/replacement byte counts and whether
open documents block the action. Closing them requires a fresh preview. Parent
directories must already exist.

**Apply reviewed working-file action** rechecks repository/branch/index and file/
parent/permissions/document guards. Original content moves to persistent project
trash before exclusive installation or removal. No displaced file is permanently
deleted. Each transaction handles one file. Rename conflicts therefore use
explicit index choices followed by separate working-file actions for each path;
there is no automatic rename topology or recursive directory operation.

This copies **raw staged bytes**, without Git attributes, filters, textconv or
line-ending conversion. A CRLF file can become LF when its blob contains LF.
The Git index is retained. POSIX replacements preserve old read/write permission
bits and derive executable bits from the staged mode; privilege bits are cleared.
New POSIX files start private (0600/0700). Windows does not emulate POSIX executable
modes. The original moved payload retains its metadata; newly created files do not
clone ACLs, xattrs or timestamps.

The latest GUI receipt offers **Undo working-file synchronization**. It requires
unchanged installed content/permissions or expected absence, an intact original
backup and closed affected text/trash documents. Current content moves to another
trash entry before restoration through the existing resource service. Undo of a
creation also retains displaced bytes. It never changes the Git index or silently
overwrites newer content. After successful Undo, the original backup reference is
cleared because that payload is back at its path; the displaced backup remains.

Git retains at most 16 in-memory receipts; the resource owner retains at most 32
issued plans. Other file operations can evict old receipts. Previews expire after
ten minutes, with at most 64 MiB of retained binary preview bytes; older payload
previews can be invalidated to stay within that budget. Persistent backups have
no automatic expiry. Direct receipt Undo does not survive restart.

**Open project trash** displays original/displaced payloads after restarting too.
Restore through the existing reviewed resource operation to an empty original or
alternative destination. If the original path is occupied, preserve that newer
file through a separate move/trash operation first; restoration never replaces it.

## Shared owner, guards and partial outcomes

GitCheckout owns selected-repository/full-index/branch/admin snapshots, guarded
receipts and UI/SDK/ABI2 dispatch. It reads local blob sizes/bytes with `cat-file`,
with fsmonitor, prompts, lazy fetch and transport disabled. It performs no Git
mutation, checkout command or configured hook/filter request.

Resources owns ordinary-path/disk/permission guards, private binary payloads and
persistent trash/install journals. It reuses exclusive moves and adds exclusive
binary creation (Windows CREATE_NEW; POSIX O_CREAT/O_EXCL/O_NOFOLLOW). Engine
document locking covers commit and closed-root checks over affected source/trash
documents. Generic resource apply/Undo cannot consume these private payload plans
and bypass their Git owner. Common resource events refresh file and image views.

Entries live under `.cycode/trash/<receipt>/`. The existing five-field record.json
identifies an original payload; install.json records installation intent before
source moves. Interrupted installation can retain replacement alongside payload.
The existing persistent trash restoration format remains compatible.

The backup and installation/restoration are separate filesystem moves. They are
not globally atomic or a compare-and-swap against external writers. `state: partial`
retains available payloads and returns an error/backup references. Postflight Git
uncertainty is a separate inspectionError even after a completed file action.
Inspect actual files/trash; do not automatically retry or roll back. There is no
cross-volume fallback, automatic cleanup or complete power-loss recovery promise.

| SDK operation | Native / ABI2 host method | Inputs |
|---|---|---|
| vcs_checkout_plan | vcs.checkout.plan | Repository-relative path, explicit materialize/remove, expectedRepository |
| vcs_checkout_apply | vcs.checkout.apply | Issued id, expectedRepository, expectedPlanRevision |
| vcs_checkout_undo | vcs.checkout.undo | Completed receipt id and the same identity/revision guards |

These typed operations use built-in Git; external VCS plugins retain their own
actions. Apply/Undo require vcs.write, workspace.write and process.execute, with
the existing workspace mutation group. All three retain noncancellable native
ownership and the existing 600-second bridge wait. No C ABI, dependency, CyUI
module or executable target change is needed.

Bounds: 16 MiB per original/staged file, 8 MiB/100,000 index records, a local
45-second inspection budget and 15-second subprocess timeouts. A running call can
finish after the inspection budget. Redirected/protected paths, nested repositories
and the active project entry are rejected before installation.

## Validation status

Windows production compilation, MSVC /Zs and actual SDK/JSON Schema data checks
are used during the existing Kaspersky native-execution suspension. Existing
VCS/resource fixtures specify exact binary installation, retained original/newer
bytes, physical deletion/creation Undo, closed managed metadata, stale index/file
guards and exclusive-create refusal. These assertions have not run. Actual Git/
filesystem/plugin/SDK/GUI behavior, keyboard/themes/DPI and Linux/macOS remain
unverified. See [Testing](Testing.md), [Validation](Validation.md),
[Resources](Resources.md) and [Git conflicts](GitConflicts.md).

Git semantics follow its primary [cat-file documentation](https://git-scm.com/docs/git-cat-file)
and [ls-files documentation](https://git-scm.com/docs/git-ls-files).

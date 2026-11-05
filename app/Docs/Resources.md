# Workspace file operations

Git working-file synchronization reuses this service for reviewed binary
installation/removal, retaining original/displaced content in the same persistent
trash. Host-owned payload plans have separate apply/Undo entrypoints; generic
JSON resource calls cannot consume them. Closed-root checks protect source/trash
documents under the Engine lock; common events refresh image previews. No public
resource schema change or new target is needed. Actual new filesystem/Undo
acceptance remains pending; see [Git working files](GitWorkingFiles.md).

**View > Files** uses the common `CyUI::Assets` browser. It shows one physical
directory at a time, including empty directories and ignored/hidden project files.
The existing Project tree and fast file search keep their ripgrep ignore rules;
they offer the same file-action dialogs through context menus. Empty directories
are visible in Files even when absent from the source-file index.

The Files toolbar creates files/folders and undoes the last GUI file operation.
Rename/Move and Delete are also available through the Project command catalogue.
F2 and Delete act on the selected physical resource only while Project or Files
has keyboard focus. Editor keymaps and symbol rename remain separate. Ctrl/Cmd+F
focuses the current Files browser or trash filter; Escape cancels a dialog.

The action dialog prepares a preview before enabling Apply. Paths are relative to
the workspace, and destination parent folders must already exist. A move never
overwrites its destination. Dirty, conflicted or changed affected documents block
the operation; save/reconcile them and prepare another preview. The active entry
selected by directly opening a solution/project cannot be moved/deleted.

Moves preserve open document identities, text Undo history, active tab and native
window identity, including detached windows. The GUI consumes the same operation
events when a move originates in MCP or a trusted plugin. Issued language edits are
invalidated by fresh document revisions; language overlays close/reopen at the new
path. Text is copied on revision changes, not unconditionally every frame.

Deleting moves the complete file/directory into `.cycode/trash/item-…/payload`.
Original relative path, timestamp and a content guard are recorded before the move.
The **Project trash** tab lists dated entries and prepares guarded restoration,
including after application restart. There is no permanent-delete or empty-trash
action. No automatic expiry is applied. Restoring refuses occupied destinations
and modified/corrupt payloads. An alternative destination may be supplied.

Open text buffers survive deletion with a missing-file marker. Restoring their
unchanged original data reattaches them; a changed buffer blocks restoration until
saved or closed. Typing remains possible, and an explicit Save can recreate the
deleted path. A clean missing buffer is also recreated by explicit Save. Changes
made after deletion use the ordinary crash-recovery worker. The retained trash
copy is independent, so it cannot overwrite a newly recreated file.

## Transactions and limits

`resources_preview` hashes a bounded snapshot without changing project files. Its
issued plan captures the open-document identities/revisions and disk state.
`resources_apply` checks those guards before committing an exclusive filesystem
move/create. `resources_undo` prepares the inverse action and checks that content
still matches its last applied state. Undoing a creation moves it to project trash;
it does not recursively delete user data. Text Undo and file-operation Undo have
separate command IDs. File-operation plans live only in the current native process,
with the newest 32 retained and previews expiring after ten minutes. Deleted payloads
remain restorable through the persistent trash even after plans are gone.

The snapshot limit is 4,096 entries, 64 directory levels, 256 MiB total content and
128 affected open documents. Large/unsupported actions fail before moving their
source. The physical browser returns up to 2,048 children after examining at most
8,192 directory entries. Trash listing examines at most 2,048 entries and returns
the newest 256 by default. Truncation is explicit. The GUI loads directories in
background requests; it does not recursively scan the workspace every frame.

Operation events are bounded to 128 records / 4 MiB and use a monotonic cursor.
A gap clears stale navigation history and open text tabs reconcile by stable native
document identities. Those identities belong to one Engine lifetime. Watcher/index
counters are invalidated immediately after a committed operation. `.cycode`, `.git`,
installation `runtime`/`data`, redirected paths and symbolic links are protected.
Regular/binary files use the same reversible filesystem operation; text editing's
4 MiB UTF-8 limit is independent of this service.

Windows uses MoveFileEx without replacement. Linux uses renameat2 with
RENAME_NOREPLACE, and macOS uses renamex_np with RENAME_EXCL. No overwrite-capable
rename fallback or cross-volume copy/delete fallback is used. Hash guards detect
ordinary concurrent changes/corruption and are not authentication. They do not
promise an atomic compare-and-rename against arbitrary external writers. Exclusive
destination handling prevents a destination that appeared concurrently from being
replaced. The implementation does not promise recovery from every power-loss state.

## Shared SDK/MCP/plugin surface

- `resources_preview`, `resources_children`, `resources_trash` and
  `resources_events` are read-only operations.
- `resources_apply` and `resources_undo` require `workspace.write`.
- Trusted native plugins may call the same exported host services. Their manifest
  needs `workspace.write` for mutations, and read-only provider callbacks cannot
  mutate the host. This does not isolate native DLLs at the OS level.
- The SDK's jobs/sessions/permissions/MCP remain supplied by the real CyToolsCore
  runtime. Workspace sources and trash are persistent project data, never temporary
  task artifacts.

These are physical path changes. Includes, solution/project manifests, Unreal
reflection names/assets and debugger breakpoints are not rewritten. Semantic file
rename via LSP resource edits, cross-volume moves, case-only renames on a
case-insensitive filesystem, rich multi-selection/copy/paste, persisted file-operation
Undo/Redo and drag/drop folder-target integration remain future work. The common
Assets module supplies drag payloads but no folder-drop event in the pinned API.
Browser favorites are session-local. A physical directory containing protected
metadata cannot be moved as a whole.

See [Validation](Validation.md) for executed native, headless UI and MCP checks.
Windows is the locally tested platform; the POSIX implementations remain unverified.

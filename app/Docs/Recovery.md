# Unsaved-document recovery

Issued untitled drafts, including empty drafts, now use these same checkpoints.
They restore into memory and require Save as to become source files. Session
coordinates/window keys are applied after explicit recovery; see
[Untitled documents](UntitledDocuments.md). Text never silently restores at startup.

The native Engine checkpoints modified text buffers once per second on its own
worker. There is no autosave to project files. The first dirty buffer creates a
session under `.cycode/recovery/`; opening an unchanged workspace creates no
recovery directory. File indexing, search and the watcher exclude this directory.

On startup, inactive sessions are offered in **View > Recovery** and Find Action.
Select a checkpoint for a read-only, syntax-colored CyUI preview. **Restore buffer**
opens the complete text in memory. Restoration is one editor Undo transaction;
Undo returns to the previously loaded text, and Redo restores it again. Save is a
separate explicit action. Previous editor Undo history is not serialized.

A changed file retains a save conflict instead of overwriting external changes.
If the original was deleted, restoration creates only a buffer; explicit Save
recreates its path without overwriting a file that appeared in the meantime.
Preview is bounded to 64 KiB by default (256 KiB maximum through MCP); truncating
the preview does not truncate restoration. Refresh the preview if its target
revision or disk state changed. A dirty target must first be saved or closed.

Right-click an inactive entry to discard its checkpoint, including a corrupt
payload that cannot be previewed. The confirmation removes that backup only.
Successful Save, explicit document discard/reload and Quit > Discard retire the
current instance's matching checkpoints. A shutdown that retains dirty buffers
keeps their last checkpoint and attempts a final flush.

## Storage and guards

Each session holds an operating-system file lock: Windows file sharing or POSIX
flock. A live instance's entries are visible as **In use** and cannot be previewed,
restored or discarded by another instance. No process-ID timeout guesses that an
instance has crashed. The lock is released by process exit, including abrupt exit.

Checkpoints use normalized UTF-8 payloads and separate original disk bytes. BOM,
CRLF and CR-only metadata preserve the source encoding/newline convention on Save.
An atomic index references immutable payloads only after their writes are flushed.
Lengths and FNV-1a fingerprints detect corrupt or mismatched payloads; fingerprints
are corruption checks, not signatures or authentication. Redirected recovery
directories and paths outside the canonical workspace are rejected. The directory
contains plaintext working code and should remain excluded from version control.

Restoration checks the selected checkpoint, the target revision and a fingerprint
of the current disk content. Before removing the abandoned source entry, the
restored buffer must have its own durable checkpoint. If that transfer fails, the
source is retained and the UI reports the failure. Original project files are
never removed by recovery cleanup.

Limits are 128 dirty documents / 128 MiB of combined text and baselines per session,
and the existing 4 MiB UTF-8 limit per document. Exceeding a quota or a write failure
is reported; earlier checkpoints remain. The list examines at most 512 directory
entries and returns the newest 256 records by default (up to 1,024 requested).
Older inactive sessions have no automatic expiration. Interrupted writes may leave
unreferenced payloads; these are never treated as recoverable documents and are
not recursively swept. A corrupt index is reported for manual inspection.

The interval is best effort: an abrupt exit can lose edits made since the last
completed checkpoint. There is no power-failure guarantee, historical Undo journal,
untitled-document model or large-file recovery. Separate bounded tab/cursor/panel/
layout metadata now restores workspace sessions; see [Workspace sessions](WorkspaceSessions.md).

## Shared services and verification

GUI and CyToolsCore/MCP use the same Engine and document revisions:

- `recovery_status`, `recovery_list` and `recovery_preview` are read-only.
- `recovery_flush`, `recovery_restore` and `recovery_discard` require
  `workspace.write`. Restore requires `expectedRevision` and `expectedDiskState`
  from preview, plus the session/id/checkpoint reference.
- `workspace_poll` includes checkpoint health. Write failures surface in the
  desktop without requiring the user to open Recovery first.

CTest `recovery` uses a real child process that exits without destructors after an
automatic checkpoint. It covers Unicode/BOM/CRLF, active-instance isolation,
revision/disk conflicts, missing files, corruption, path rejection, quota retention,
failed transfers and 4 MiB payloads. `recovery_ui` exercises the real editor and
CyUI view without native windows, including Restore/Undo/Redo/edit/Save and
EN/FR, six themes and three DPI values. Python integration uses the real native
bridge, CyToolsCore permissions and a killed/restarted process. See
[Validation](Validation.md) for executed results and platform limits.

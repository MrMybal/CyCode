# Untitled documents and Save as

File > New text document opens a draft without creating a source file. The existing
New file dialog also offers Open unsaved, using the entered basename (for example
scratch.cpp) to select the shared CyUI lexical syntax. Draft names contain no
folders. Plain text is the default for the direct command. CodeEditor retains
normal editing, in-document search, folding, multi-cursor and live Undo/Redo.

The native Engine owns drafts, their text, revisions and IDs. An issued reference
has the form `untitled://<unique-id>/<name>`. It is not a filesystem path. Read,
replace, close, global text search and recovery share that same native model;
there is no separate Python or GUI text store. Up to 64 drafts / 64 MiB of native
UTF-8 text are accepted. Individual editor text limits and recovery's shared
128-document / 128-MiB quota still apply. Empty drafts remain dirty so they are
checkpointed and must be explicitly saved or discarded.

Drafts created by MCP/native plugins appear in real tabs without stealing focus.
Externally closed live documents retire their GUI views. If an editor still has
uncommitted local text, it is rescued into a new native draft with the same editor
Undo rather than dropped; quota failures leave the local view dirty and report an
error. Save dialogs for externally closed documents are dismissed.

## First save and copies

Save on a draft opens Save as. Save as also works for editable file-backed text.
Enter a **new workspace file in an existing folder**. The native service rejects
an occupied disk path or open-buffer path, an invalid source revision and paths
outside the workspace. It uses the existing exclusive/atomic text writer and
retains UTF-8/BOM and line-ending settings. It does not create destination folders
or overwrite existing files. An OS save picker, overwrite-with-review and arbitrary
external save destinations are not implemented by this workflow.

After a successful write, the document switches to the new path with its live
identity retained. The old disk file remains unchanged when saving a file-backed
document under another name. The UI keeps the same CodeEditor and native window
key, preserving the current text Undo/Redo, and applies the saved-file syntax and
existing language activation policy. Host move/save events contain metadata only.
Old recovery data is retired after commit. Language-service warnings are reported
separately from an already successful save.

Save all writes file-backed buffers and queues drafts for individual destinations.
Cancelling Save as stops the remaining draft queue and cancels an associated
close/quit request; it leaves the buffers open. Close/discard checks the observed
revision before retiring a live buffer and its recovery data.

The VS Code Windows profile uses Ctrl+N for a draft, Ctrl+Shift+S for Save as and
Ctrl+K, S for Save all, following the [official keybinding reference](https://code.visualstudio.com/docs/reference/default-keybindings).
Rider retains Ctrl+N for type lookup; the new draft and Save as commands are
available from File/Find Action and editable bindings. The existing New file
command remains available. Full macOS keymap validation remains separate work.

## Recovery and session metadata

The existing recovery worker checkpoints drafts, including empty drafts, once per
second. A process closing or crashing can leave an inactive checkpoint. View >
Recovery verifies and previews it; Restore buffer reopens the same issued draft
reference in memory. This operation never invents a project file. Save as is still
required to persist it as source. Live target/recovery revisions are checked, and
the source checkpoint is consumed only after the restored draft has a new durable
checkpoint. Explicit discard removes the selected recovery data.

Workspace sessions store draft tab/window keys, cursor and approximate scroll
metadata alongside the existing file tabs. Draft text remains owned by Recovery.
When the user explicitly restores a draft from Recovery, its matching session
coordinates and detached-window key are applied. Text is **not automatically
restored at startup**. Complete Undo history is not serialized. Recovery data is
persistent project state under .cycode/recovery, excluded from source indexing and
publication, rather than an SDK temporary artifact.

Untitled documents use shared lexical coloring; file-backed language-server,
semantic refactoring, filesystem/VCS and breakpoint operations require Save as
first. Drafts are excluded from filesystem polling/resource operations and from
recent/closed-file lists; their own recovery route handles later reopening.
Project execution checks continue to concern actual project files.

## Shared SDK/native plugin access and validation

CyToolsCore exposes documents_create_untitled, documents_save_as and
documents_close with workspace.write permission. Existing documents_read/replace
and recovery operations accept issued references. These three short mutations are
noncancellable once dispatched so cancellation is not reported after a commit.
Save as and close require expectedRevision. The native ABI2 host exports the same
methods with the existing permission checks; no C ABI shape or SDK runtime is
duplicated. SDK draft creation/replacement also has its existing input-text bound.

Native regression sources cover no-source-file creation, revisions, refusal of
overwrites, same-identity Save as, retention of the original disk file, empty-draft
recovery, session validation and keymaps. They belong to the existing opt-in core
test target; no new test executable is added. During the Kaspersky interruption
the existing opt-in resource GUI target also gains MCP draft/view/close, local-edit
rescue and Undo/window identity assertions. No new GUI runner is introduced;
they are syntax checked only. Actual native/GUI acceptance, cancellation/focus,
Undo across Save as, multi-window restoration, themes/DPI and Linux/macOS remain
unexecuted. See [Validation](Validation.md).

# Create a personal snippet from selected code

Select code in one writable saved workspace document and choose **Edit > Create
user snippet from selection...**. Find Action exposes the same command; no new
default shortcut is assigned. One selection/caret is required. The desktop orders
backward selections before converting their endpoints to zero-based UTF-16.

The native service reads the current document buffer, including unsaved changes.
It copies the selection literally into a personal catalogue draft and opens the
existing shared JSON editor. The original source and saved catalogue are retained.
Edit the generated name, prefix, scope and body, then explicitly Save/Ctrl+S.
Subsequent JSON edits use ordinary draft Undo/Redo; the generated initial draft is
not an edit of the source document. Discard/Reload/Close use the existing dirty
draft decisions. Failed capture shows an error; Reload can open the saved personal
catalogue for repair before capturing again.

## Draft format and boundaries

The default definition name is `Selection snippet`, with the first available
numeric suffix from 2 when needed. Existing named definitions keep their values.
The complete JSON is pretty-printed with two-space indentation; its old whitespace
formatting is not retained in the draft. The generated prefix is `snippet` and the
scope comes from the source language ID (including `verse`, `cpp`, `hlsl`, `glsl`
or the existing extension fallback). Review these technical values in the editor.

Dollar signs, backslashes and closing braces pass through the same bounded literal
escaping used by snippet context variables. Unicode and line breaks are retained.
The body ends with `$0`, placing the final caret after the copied text; capture
does not create numbered editable fields. For example, source `$1` becomes the
JSON body `"\\$1$0"`, so later insertion writes literal `$1`.

The selection must be nonempty, ordered and no more than 8 KiB of UTF-8 source.
UTF-16 endpoints cannot split a surrogate pair. Escaped body plus `$0` must also
fit 8 KiB: 8190 plain ASCII bytes fit, but escaping may require a smaller selection.
No truncation occurs. The saved personal catalogue must be valid/readable, contain
fewer than 256 definitions and leave enough room for the entire draft within
256 KiB, including its JSON formatting. Malformed/disabled/unreadable storage must
be configured or repaired first. No file, directory or lock is created by capture.

## Native/SDK/MCP/plugin contract

`snippets.userDraft` / `snippets_user_draft` accepts workspace `path`,
`expectedRevision` and required `selection` start/end positions. It requires an
already-open saved workspace document, refuses untitled/SDK/external references,
and checks document revision/generation again after preparing the draft. It does
not request a language server, execute plugin callbacks, create a change plan or
apply an edit. ABI2 plugins can use the same permission-free read-only JSON host
service; no ABI structure changes or UI dependency is introduced into the core.

The result contains `catalogue` (the original personal read snapshot), `draft`,
the generated `name`, normalized workspace `path` and source `revision`.
To persist, review `draft`, then call `snippets_user_save` with `text=draft` and
`expectedRevision=catalogue.revision`; `settings.write` is required for that save.
Another instance's change will reject the save. Capture itself never saves, and
the draft is a copied snapshot rather than a live link to later source changes.
Use [personal snippets](UserSnippets.md) for persistence and conflict behavior.

Compiler/data/documentation checks and unexecuted native acceptance are recorded
in [Validation](Validation.md). Native file/Unicode parser/capture/GUI/Undo and
Linux/macOS acceptance remain pending during the antivirus suspension.

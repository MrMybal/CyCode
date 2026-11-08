---
id: workspace-files
title: Manage project files and trash
description: Create, move and restore physical files without confusing them with text edits.
section: Work in CyCode
parent: editing
order: 41
revision: 1
---

## Browse physical files {#files}

Open **View > Files** to browse one physical directory, including empty folders
and ignored or hidden files. Project and file-name search instead use the
ignore-filtered source index. A directory can therefore exist in Files without
appearing in Project. Refresh the appropriate view when its observation is old.

Choose a file or folder before Rename/Move or Delete. F2 and Delete act on the
physical resource only while Project or Files has focus; they do not rename a
symbol. Ctrl/Cmd+F focuses the Files or trash filter. Escape cancels the dialog.

## Preview a change {#preview}

1. Save or reconcile affected documents.
2. Choose New file/folder, Rename/Move or Delete and review its preview.
3. Use workspace-relative paths. Destination parents must already exist.
4. Apply only the still-current preview. An occupied destination is refused.

Moving preserves open text identities and text Undo. It does not rewrite includes,
solution manifests, Unreal reflection names, assets or debugger breakpoints.
The active solution/project entry cannot be moved/deleted this way. Protected
metadata, redirected paths and symbolic links are excluded. Cross-volume moves
and case-only renames on a case-insensitive filesystem are not provided.

```diagram
{"type":"flow","title":"Review a physical file operation","nodes":[{"id":"choose","label":"Choose a resource","x":0,"y":0},{"id":"preview","label":"Review preview","x":230,"y":0},{"id":"apply","label":"Apply current plan","x":460,"y":0},{"id":"trash","label":"Retain deleted data","x":690,"y":0}],"edges":[{"from":"choose","to":"preview"},{"from":"preview","to":"apply"},{"from":"apply","to":"trash","label":"Delete"}]}
```

## Restore or undo {#trash}

Delete moves data to `.cycode/trash`; **Project trash** can restore a retained
entry after a restart. Review its original location or choose another destination.
Occupied destinations and changed/corrupt payloads are refused. There is no
permanent-delete, empty-trash or automatic-expiry action.

The toolbar's file-operation Undo is separate from text Undo. Its plans are
process-local and expire; retained trash survives them. Undoing a creation moves
its result to trash. A deleted open text buffer remains editable with a missing
marker; explicit Save can recreate its path. The retained trash cannot overwrite
that newly created file. Save or close a changed buffer before attempting restore.

Operations are bounded to 4,096 entries, 64 levels, 256 MiB and 128 affected open
documents. The physical browser may also report truncation. A limited view does
not mean the remaining files disappeared.

## External changes and the index {#monitoring}

Clean open documents reload detected disk changes; dirty or conflicted buffers
retain their text. Saving refuses a conflicting disk baseline. Review the change
and deliberately reload/discard when appropriate. File watching is separate from
the recoverable text checkpoints in [Recovery](doc:recovery#recover).

The index respects ignore rules and holds at most 200,000 paths. Explicit refresh
rebuilds it; changes to global Git exclusions can require that refresh. SDK clients
of [workspace_files](doc:operation-workspace-files-workspace-files) must compare
both indexRevision and generation while assembling pages. Physical operations use
the same preview/apply/undo services in [the resource API](doc:api-resources#operations).

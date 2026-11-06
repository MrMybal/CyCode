---
id: editing
title: Editing and files
description: Editing and files — local application manual
section: Guides
order: 20
revision: 1
---

## Documents and transactions {#documents}

The native engine owns live text. Revisions and disk baselines are checked before saving; an external change may require review or Save as. Undo/Redo belongs to editor changes. Opening help does not create an editing transaction.

## Encoding and readability {#encoding}

Shared editor references describe UTF-8 and newline-preserving I/O. Do not assume Reader-specific extended encodings are all IDE services.

## Windows, text size and themes {#layout}

Move detachable views to another display. Application controls own text size and themes. Layout restoration and complete historical Undo remain subject to the documented limits.

```diagram
{"type": "sequence", "title": "Save / Enregistrer", "actors": [{"id": "ui", "label": "Editor"}, {"id": "core", "label": "Native owner"}, {"id": "disk", "label": "Disk"}], "messages": [{"from": "ui", "to": "core", "label": "Live revision + text"}, {"from": "core", "to": "disk", "label": "Check baseline"}, {"from": "core", "to": "disk", "label": "Guarded atomic save"}, {"from": "core", "to": "ui", "label": "Result / conflict", "reply": true}]}
```

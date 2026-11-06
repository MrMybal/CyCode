---
id: editing
title: Edit and save files
description: Work with tabs, unsaved drafts and external file changes.
section: Work in CyCode
order: 40
revision: 2
---

## Documents and drafts {#documents}

Open a file from Project, file-name search or drag-and-drop. New creates an untitled document. Edit it normally, then use Save As to give it a workspace destination. An untitled reference is not a disk path.

Ctrl+S saves the active text document. Save All handles open edited documents. Renaming a symbol or applying a refactoring changes live buffers first; saving remains a separate action. Undo/Redo belongs to the editor, not to Git commits or debugger execution.

## Encoding and line endings {#encoding}

CyCode’s native document service handles UTF-8, with or without a BOM, and preserves the detected LF, CRLF or CR line endings. Convert other encodings with an appropriate text tool before editing them here. Do not overwrite an externally changed file without reviewing the conflict; reload can discard unsaved edits and prompts accordingly.

## Editor layout {#layout}

Use tabs, folding, word wrap and text-size settings for comfortable reading. Multiple cursors and selections affect the live document. Text files are limited to 4 MiB. [Images](doc:images#preview) open in read-only previews, with different controls.

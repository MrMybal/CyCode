---
id: recovery
title: Recover unsaved work
description: Review checkpoints before restoring changed text.
section: Work in CyCode
order: 70
parent: editing
revision: 2
---

## Recover a document {#recover}

Open Recovery to inspect retained document checkpoints. Preview the selected text before restoring it. Restoring a checkpoint updates a live buffer; it does not silently replace a newer disk file. Save explicitly after reviewing the result.

Untitled drafts can also be recovered. Their text is not automatically reopened just because the window layout is restored. Existing disk conflicts still need a deliberate choice.

Session restoration and document recovery serve different purposes: a session remembers where you were working, while a checkpoint retains recoverable text. Neither reconstructs a running terminal or the complete editor Undo history.

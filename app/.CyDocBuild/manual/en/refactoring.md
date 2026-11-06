---
id: refactoring
title: Completion and refactoring
description: Review completion, rename and code-action changes before saving.
section: Work in CyCode
order: 60
parent: editing
revision: 2
---

## Complete code {#completion}

Use Ctrl+Space to request completion from the active language service. Accepting an item can insert its associated imports as one editor transaction. Snippet fields use Tab / Shift+Tab; Escape finishes the snippet. Ctrl+Space leaves the current snippet to request another completion.

## Rename and code actions {#rename}

1. Place the caret on the symbol and choose Rename, or open context actions for the current range.
2. Choose an actual server proposal. Disabled actions explain missing support or context.
3. Review Before/After changes and any confirmations in the refactoring view.
4. Apply the change to live buffers, then save when ready.

The IDE refuses stale or overlapping edits and disk conflicts. Changing the source after a preview can require a fresh query. The server determines available refactorings; syntax highlighting alone cannot rename symbols safely.

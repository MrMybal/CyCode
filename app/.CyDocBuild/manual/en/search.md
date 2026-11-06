---
id: search
title: Find text and files
description: Use document search, project search and Search Everywhere.
section: Work in CyCode
order: 80
revision: 2
---

## Choose the search scope {#find}

| Shortcut | What it searches |
| --- | --- |
| Ctrl+F | The current text document |
| Ctrl+Shift+F | Workspace text |
| Ctrl+Alt+F | File names |
| Double Shift | Files, actions, recent files and available symbols |

The global Search view provides text options and file filters. Select a result to inspect its location. Document search and project search keep their own scopes; an image tab has no document-text search.

## Results and actions {#results}

Use Up/Down and Enter in Search Everywhere. Prefix a query with `>` for actions, `@` for members or `#` for symbols. Symbol results need an enabled server and its current project index. Use [source navigation](doc:navigation#symbols) for definitions, references and occurrences.

Result lists can retain an older observation after a file changes. Refresh the query when a location is no longer current. A compiler message can be copied even when its reported position cannot be mapped to a workspace file.

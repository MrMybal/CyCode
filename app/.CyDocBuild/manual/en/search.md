---
id: search
title: Find text and files
description: Use document search, project search and Search Everywhere.
section: Work in CyCode
order: 80
revision: 3
---

## Choose the search scope {#find}

| Shortcut | What it searches |
| --- | --- |
| Ctrl+F | The current text document |
| Ctrl+Shift+F | Workspace text |
| Ctrl+Alt+F | File names |
| Double Shift | Files, actions, recent files and available symbols |

The global Search view provides text options and file filters. Select a result to inspect its location. Document search and project search keep their own scopes; an image tab has no document-text search.


Unsaved text replaces the disk results for that document. Case-insensitive workspace search uses the same Unicode simple-case rules for saved and unsaved text: accented letters, Greek sigma variants and the Kelvin sign are handled without changing byte-based result columns. It does not perform Unicode normalization or expand `ß` into `ss`. Case-sensitive literal searches remain byte-exact.

Regex and Unicode-insensitive unsaved searches use temporary numeric snapshots, capped at 128 buffers / 64 MiB, with existing cancellation and output limits. They never save project sources. Result previews stop on a complete UTF-8 character within 512 bytes.

## Results and actions {#results}

Use Up/Down and Enter in Search Everywhere. Prefix a query with `>` for actions, `@` for members or `#` for symbols. Symbol results need an enabled server and its current project index. Use [source navigation](doc:navigation#symbols) for definitions, references and occurrences.

Result lists can retain an older observation after a file changes. Refresh the query when a location is no longer current. A compiler message can be copied even when its reported position cannot be mapped to a workspace file.

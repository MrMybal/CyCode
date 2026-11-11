---
id: symbols
title: Explore structure and hierarchies
description: Use server-reported symbols, references and call sites with their source context.
section: Work in CyCode
parent: navigation
order: 91
revision: 1
---

## Structure and the symbol palette {#structure}

Open **View > Structure** for current-file symbols. Filter, sort by source order
or name, follow the caret or locate the active symbol explicitly. Parent context
is retained when filtering. Enter or a row click navigates; merely moving the
selection does not execute a jump. Symbol names, types and nesting come from the
server. A flat response does not become an invented class tree.

Go to type, Go to symbol and Search Everywhere can use Automatic or a selected
enabled language server. A result labelled Location on selection needs a separate
resolve request. Wait for it; refresh after changing source, project or server.
The list does not persist server node IDs across restarts.

## References and occurrences {#references}

Go to definition, declaration, type definition and implementation require the
matching server capability. References offers cached filtering, keyboard/result
selection, copied locations and explicit refresh at caret. A stale observation
can remain readable while navigation is unavailable.

Highlight occurrences at caret uses reported Text/Read/Write ranges. Its dock
offers next/previous and selection of eligible reported ranges. Text occurrences
can be less precise than semantic references. These outlines do not replace
syntax colours. Selection refuses partial, overlapping or outdated ranges before
changing the caret set. Lexical find remains a separate action.

## Call and type hierarchies {#hierarchy}

Place the caret on a function/type and choose **Navigate > Call hierarchy** or
**Type hierarchy**. View > Hierarchy reopens the panel. Switch callers/callees
or base/derived types. Expanding requests a branch from the original server;
recursive relationships and bounds remain visible.

Enter, double-click or Open symbol visits the declaration. Selecting a call site
visits that call: incoming sites belong to the caller; outgoing sites belong to
the expanded source function. Refresh at caret starts a new observation after
edits or backend changes. No hierarchy is guessed from matching names.
See [shortcut profiles](doc:shortcuts#profiles) and
[language operations](doc:api-language#operations).

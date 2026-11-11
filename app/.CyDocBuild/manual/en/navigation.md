---
id: navigation
title: Navigate symbols and problems
description: Use Structure, References, Problems and occurrence navigation.
section: Work in CyCode
order: 90
parent: search
revision: 7
---

## Symbols and locations {#symbols}

Use Go to definition, declaration, type definition or implementation when the server advertises that capability. References shows returned locations and offers filtering, refresh and copied locations. Structure shows current-file symbols; follow-caret and ordering controls help with large files.

The symbol palette can select an enabled server. A result labelled Location on selection must be resolved before navigation. Refresh the query when the source or server changes.

## Problems and occurrences {#problems}

Problems groups compiler and language-server messages. Filter by severity or text, select a message for its details, then navigate to a current mapped location. Advisory or outdated positions remain readable without moving the caret.

Highlight occurrences at caret requests reported Text/Read/Write ranges. Its inspector supports next/previous navigation and reported-range selection. Text highlights may be less precise than semantic references. Use the [shortcut profile](doc:shortcuts#shortcuts) for the active bindings.

## Switch C/C++ header/source {#header-source}

Choose **Navigate > Switch header/source** with a saved C/C++ file active. Rider
uses Alt+O; Visual Studio uses Ctrl+K, Ctrl+O. Find Action also exposes it; the
VS Code profile has no new default. One complete result opens directly, preserving
unsaved source edits. Multiple or partial results use the shared searchable
palette with arrows/Enter/click and Escape. Cancelling a pending list prevents a
later reply from opening a document.

The local service tries same-stem headers/sources, siblings and the nearest
Public/Private/Classes or include/includes/src/source layout, retaining nested
paths and offering a paired-root fallback. It uses at most 128 metadata probes
and sixteen ancestors, without a language server, ripgrep or recursive scan.
Generated stems, file symlinks and external targets are excluded; unreadable or
limited observations are explicit.

Navigation also uses same-stem paths from the already-current workspace file
index, so `src/Widget.cpp` can find `include/library/Widget.hpp`, other layouts and
indexed mixed-case extensions. The lookup uses cached offsets, without scanning
all files per command, starting a refresh or waiting for one. Local matches keep
their labels; additional matches say **Workspace index**. Names shared across
modules remain separate choices and do not establish semantic pairing.

The shared 128-probe bound still applies. Missing, busy, stale, partial or changed
index coverage is shown and requires explicit choice even for one result. Refresh
project files and retry when appropriate. An unchanged complete cache permits
one-result opening; its scope respects ignore rules and does not cover ignored
files. The service adds no semantic index, different-stem pairing or SDK lookup.

The read-only SDK/ABI2 operation is [workspace_counterparts](doc:operation-workspace-counterparts-workspace-counterparts).



## Open an included C/C++ file {#open-include}

Place the caret in a supported literal `#include` directive and choose **Navigate > Open included
file**, Find Action, or the active editor’s context menu. Assign a shortcut in
keyboard preferences if desired; this action adds no default binding. Unsaved
source edits are used and retained.

Quoted `#include "../Public/Actor.h"` first checks the source-relative path. Both
quotes and angles also use paths ending in the literal components from the
already-current workspace index: `<library/Widget.inc>` can find
`include/library/Widget.inc`. Extensionless files and existing generated headers
are eligible within that scope. Parent segments use only the quoted relative
probe. Multiple matches remain choices, with **Source-relative** or **Workspace
index** labels. No cache refresh or recursive scan starts. Missing/busy/stale/
partial/changed coverage is visible and prevents automatic opening; one complete
unchanged result opens normally. Escape cancels; source or caret changes invalidate
the pending context.

The bounded context reader excludes comment/string/common raw-string bodies and
recognizes escaped newlines and digit separators. Inline block comments may
separate directive tokens; immediate backslash-LF/CRLF continuations may split the
directive or keyword. The header itself stays on one physical line. The caret
may be on any continued component:

```cpp
# /* directive */ in\
clude /* header */ \
  "library/Widget.inc"
```

The reported inner-header UTF-16 range remains on the actual header line even
when the requested caret is on the first line. Limits are a 1 MiB prefix, an 8 KiB
physical span for the logical directive, and a 1024-byte target. Macros,
include_next/import, unescaped multiline comments between tokens, whitespace
between backslash and newline, continued header names, backslash/absolute paths
and unsupported spellings fail explicitly. Conditions are not evaluated. File
symlinks and external targets are excluded. The 128-entry bucket and 128-total-probe
bounds remain explicit. Ignore-filtered index matches do not establish compiler
search order or semantic resolution; external Unreal/compiler SDK roots remain
outside this local action.

The same read-only operation is [workspace_include](doc:operation-workspace-include-workspace-include),
with source revision/identity, requested caret, literal target/kind/range and index
observations.


## Inspect related results and get help {#related-results}

Choose **Navigate > Find related C/C++ files...**, Find Action or the active editor's
context menu to inspect counterpart matches, even when there is only one. This
command has no default binding. The shared list also serves ambiguous or partial
header/source and include queries. Filter cached paths, use arrows/Enter/click,
or press Escape to dismiss.

- **Retry this lookup** repeats the query without a scan.
- **Refresh lookup index and retry** explicitly rebuilds the ordinary bounded
  file index and repeats the query. It requires a valid report and an idle normal
  desktop refresh. The index keeps its 30-second / 32 MiB / 200,000-path limits.
  Ordinary queries never start that refresh. Closing prevents delayed opening;
  an explicitly started index refresh remains owned and is not terminated.
- **Copy all result paths** copies every returned candidate, regardless of filter.
- **Copy lookup report (JSON)** copies metadata, provenance and coverage, including
  a valid empty report. Source text is not copied.
- **Help for this navigation** opens this local section, or the include/header-source
  section for those direct commands. Help is also available while loading.

The configured single-stroke **Contextual help** binding works in this list;
the default is **Shift+F1** in all three profiles. Chord bindings remain blocked
inside this modal; use the visible Help action if your binding is a chord.
The visible copy/help actions close the list; the help shortcut leaves it open.
Source content is preserved. Source identity/revision,
active tab and Engine guards remain; include queries additionally guard the caret.
Retry retains inspection mode.

File-list pagination also compares both `indexRevision` and `generation`;
a manual refresh advances generation even when the watcher revision is unchanged.
API clients must discard mixed pairs and restart. See
[workspace_files](doc:operation-workspace-files-workspace-files).

Continue with [Structure/hierarchies](doc:symbols#structure) and [diagnostic navigation](doc:diagnostics#problems).

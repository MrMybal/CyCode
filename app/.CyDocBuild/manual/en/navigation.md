---
id: navigation
title: Navigate symbols and problems
description: Use Structure, References, Problems and occurrence navigation.
section: Work in CyCode
order: 90
parent: search
revision: 2
---

## Symbols and locations {#symbols}

Use Go to definition, declaration, type definition or implementation when the server advertises that capability. References shows returned locations and offers filtering, refresh and copied locations. Structure shows current-file symbols; follow-caret and ordering controls help with large files.

The symbol palette can select an enabled server. A result labelled Location on selection must be resolved before navigation. Refresh the query when the source or server changes.

## Problems and occurrences {#problems}

Problems groups compiler and language-server messages. Filter by severity or text, select a message for its details, then navigate to a current mapped location. Advisory or outdated positions remain readable without moving the caret.

Highlight occurrences at caret requests reported Text/Read/Write ranges. Its inspector supports next/previous navigation and reported-range selection. Text highlights may be less precise than semantic references. Use the [shortcut profile](doc:shortcuts#shortcuts) for the active bindings.

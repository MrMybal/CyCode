---
id: diagnostics
title: Read diagnostics and compiler locations
description: Filter reported problems and distinguish current positions from old output.
section: Work in CyCode
parent: navigation
order: 92
revision: 1
---

## Inspect Problems {#problems}

Open Problems, filter text and severity, then select a diagnostic to inspect its
complete message, provenance and location. Copy diagnostic preserves messages
whose position cannot be opened. Old/unversioned server positions in open files
can say **Position not verified** and remain readable without navigating.
Closed-file and last-build positions are observations, not proof of freshness.

Next/Previous diagnostic follows the filtered list and wraps. Current-file
variants follow line/column order from the current line. Rider uses F2/Shift+F2
for current-file navigation and Alt+F2/Alt+Shift+F2 across reported locations.
Visual Studio/VS Code use F8/Shift+F8 for workspace results; VS Code F2 stays Rename.
Existing personal bindings take precedence.

## Follow compiler output {#output}

Build's live/final log rows can open recognised source locations without editing
or saving the source. Supported forms include `file(line,column):`, `file:line:`
and Rust `--> file:line:column`. Quoted/UTF-8 paths and spaces are supported.
Numeric GLSL source IDs such as `ERROR: 0:12:` do not identify a file.

```text
src/main.cpp(42,7): error C2065: missing identifier
src/main.cpp:42:7: error: missing identifier
```

Only existing regular workspace files can map. External/missing files and URI
paths stay text; no basename guess is made. Live project-task logs use their
recorded working directory, other supported logs the workspace root. Compiler
column conventions vary; exact Unicode display columns, source maps and arbitrary
terminal escape decoding are not inferred. Rebuild or refresh the server when
source changed since the observation. See [task output](doc:project-tasks#output)
and [build_location](doc:operation-build-location-build-location).

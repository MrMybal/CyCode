# Switch C/C++ header and source files

Choose **Navigate > Switch header/source** with a saved workspace C/C++ document
open. Rider uses **Alt+O**; Visual Studio uses **Ctrl+K, Ctrl+O**. Find Action also
exposes the command; VS Code has no new default binding. These conventions follow
the [Rider navigation guide](https://www.jetbrains.com/help/rider/Navigation_in_CPP.html)
and [Visual Studio C++ navigation](https://learn.microsoft.com/en-us/cpp/ide/navigate-code-cpp?view=msvc-170).
The selected profile's ordinary override/conflict system remains responsible for
custom bindings.

A single result from a complete, unchanged file cache opens through the ordinary
document/navigation owner, preserving the source's unsaved edits. Multiple
candidates open the common CyUI Commands
palette: search paths, use arrows/Enter or click, Escape to dismiss. A bounded or
partly unreadable search always offers a choice instead of automatically opening
its single remaining result. Loading, errors, no matches and partial results are
visible. Candidates/labels are cached; drawing the palette performs no filesystem
queries. Closing during a query prevents a later reply from opening a document.
Workspace changes and application shutdown retain/join owned work.

## Inspect related results

**Navigate > Find related C/C++ files...**, Find Action and the active editor's
context menu query the same counterpart owner but always show the list, even for
one complete match. No default key binding is added. The common palette offers:

- **Retry this lookup**: repeat the same kind of query without scanning.
- **Refresh lookup index and retry**: explicitly call the existing bounded
  `workspace.files` refresh, then repeat the query. Available after a valid report
  when the desktop's normal file refresh is idle. This uses the ordinary ripgrep
  owner and its 30-second / 32 MiB / 200,000-path limits. It is never implicit in
  an ordinary navigation query. The tree keeps its normal refresh lifecycle.
- **Copy all result paths**: copy all returned candidates, not just filtered rows.
- **Copy lookup report (JSON)**: copy the native metadata and coverage report,
  including an empty valid result; source text is not included.
- **Help for this navigation**: open the matching local CyDoc section. The
  configured single-stroke `help.context` binding also works while this list is
  active (default Shift+F1 in all profiles). Chord bindings are still blocked
  inside this modal; its visible Help action remains available.

These actions also appear for ambiguous/partial include and header/source
results. Help is available while loading. Retry preserves inspection mode; the
normal direct-open commands retain their one-complete-result behavior. The visible copy/help actions close the list; the help shortcut leaves it open.
The source is not modified/opened. The existing
tab/document identity/revision/Engine guards apply to all results and actions;
include results additionally guard the original caret. Dismissal suppresses late
opening. An explicitly started index refresh remains owned/joined; dismissal does
not terminate that refresh. No per-frame filesystem work or independent index is
added. Clipboard, browser handoff and real modal keyboard acceptance remain
unexecuted during the native-run suspension.

## Local conventions

The service reads filesystem metadata only, without a language server, ripgrep,
compiler, project rules or recursive directory enumeration. It recognizes headers
`.h`, `.hh`, `.hpp`, `.hxx`, `.inl` and sources `.c`, `.cc`, `.cpp`, `.cxx`, `.m`,
`.mm`, trying lower-case and upper-case extension variants. `.generated` and `.gen`
filename stems are excluded. It tries the same stem in the source's directory,
then finds the nearest conventional directory within at most sixteen ancestors:

| Origin directory | Paired directories |
| --- | --- |
| Public or Classes | Private |
| Private | Public and Classes |
| include or includes | src and source |
| src or source | include and includes |

Common folder capitalization variants are tried. Nested paths are retained, with
a second paired-root fallback, for example:

- Source/Module/Public/Audio/Actor.h → Source/Module/Private/Audio/Actor.cpp
- include/library/Widget.hpp → src/library/Widget.cpp or src/Widget.cpp

Local directory pairing stops at that nearest layout to avoid crossing unrelated
outer modules. The cached index may additionally show same-stem matches elsewhere
in the workspace; their ambiguity is explicit.
At most 128 unique candidate paths are probed; exhausted ancestry/probe bounds and
unreadable paths are reported. Existing regular canonical targets must remain in
the workspace. File symlinks and outside-workspace targets are excluded. Windows
path deduplication ignores case; POSIX path comparison is case-sensitive.

## Cached file index

The ordinary workspace file refresh also builds a header/source lookup keyed by
filename stem, using offsets into that same sorted in-memory path list. Navigation
copies at most 128 matching paths under a nonblocking cache lock, rather than
walking all indexed files for every command. The lookup itself never calls
`workspace.files`, starts ripgrep, waits for an in-progress scan or performs
another directory scan. The result list has a separate explicit refresh action
described below.
The existing index keeps its 200,000-path / 32 MiB discovery bounds.

This finds flat `src/Widget.cpp` against `include/library/Widget.hpp`, other
same-stem layouts and indexed mixed-case extensions such as `.HpP`. Local
relations retain priority and duplicate paths are removed. Same stems across
modules remain separate choices; no name match proves a semantic association.
The total metadata-probe bound remains 128, including local and cached paths.

The `index` observation contains `state`, watcher `revision` and cache
`generation`. A manual rebuild advances the generation even when the watcher
revision is unchanged. Missing or contended caches report `missing` or `busy`;
old watcher revisions report `stale`. These caches contribute no paths. A current
bounded cache or overfull matching bucket reports `partial`; its retained paths
may still be offered. A complete current cache reports `ready`. Rechecking both
revision and generation after metadata probes marks concurrent changes as
`changed`. Unavailable or incomplete coverage sets `truncated`; the GUI shows an
explanation and requires explicit selection even for one candidate. Refresh the
project files and retry when appropriate. Querying does not activate the cache.

`ready` describes the existing indexed scope, including its ignore rules, rather
than every file on disk. Explicit local path probes do not consult ignore files.
No persistent index, file-content parser, compiler model or semantic/clangd
pairing is added. Different stems, generated files, module interfaces and external
SDK headers remain outside this query. A target can disappear/change after
inspection; ordinary opening still validates and reports the error. No project
file is generated, edited or saved.

## Shared service

`workspace.counterparts` / SDK `workspace_counterparts` accepts `path`,
`expectedRevision` and `expectedDocumentId`. The source must already be open and
saved inside the workspace. Untitled/SDK/debug/external references are refused;
there is no implicit document opening. The Engine captures lifetime/revision,
releases its mutex while probing, then rechecks both before returning. The UI also
checks the original tab, active source, Engine and revision before navigation.

The result contains source `path`, `revision`, `documentId`, `candidates`
(`path`/`relation`: sibling, paired-directory, paired-root, workspace-index),
`index`, `checked`, `unreadable` and `truncated`. ABI2 plugins use the same
permission-free read-only JSON host service. SDK/plugin callers receive
observations, not a UI-opening capability.
No ABI field, native executable target, dependency or CyUI module is added.

Compiler/data checks and unexecuted native/GUI/filesystem acceptance are recorded
in [Validation](Validation.md). Linux/macOS and real keyboard/DPI navigation remain
unverified during the antivirus suspension.

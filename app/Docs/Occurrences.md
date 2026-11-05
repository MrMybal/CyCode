# Document occurrences

Three connected changes share the existing native language owner: normalized
document-highlight requests, independent occurrence geometry in CodeEditor, and
a dockable inspector with kind filtering, navigation and explicit multi-selection.
The new service is `language.documentHighlights`, also available as SDK/MCP
`language_document_highlights` and a permission-free ABI2 JSON export. The SDK
surface contains 227 operations. The C ABI and process/edit/Undo owners are unchanged.

## Provider and source contract

The request is LSP `textDocument/documentHighlight`; initialization advertises
`textDocument.documentHighlight.dynamicRegistration=false`. Built-in C++/managed,
other language servers and contributed backends all require an actual advertised
`documentHighlightProvider`. Status exposes `methods.documentHighlights`.
Disabled backends are not enabled by this read. Explicit requests can initialize
an already configured/enabled server through the existing language service.

Input requires `path`, `expectedRevision` and a zero-based UTF-16 `position`.
Optional `expectedDocumentId` and `expectedEpoch` guard the caller's source
snapshot. `includeExternal` permits an already registered SDK source through the
existing access policy; arbitrary external paths are not granted access.
The response contains `path`, `revision`, `documentId`, `backend`, `epoch`,
`position`, `positionEncoding: utf-16`, `occurrences`, `invalid` and `truncated`.
Each occurrence has `range` and integer `kind`:

| Kind | Meaning from provider | UI palette role |
|---|---|---|
| 1 | Text (including omitted kind) | accent |
| 2 | Read | info |
| 3 | Write | error |

Text highlights can be fuzzier than symbol references. The application does not
infer semantic identity or classify reads/writes by matching names. Results may
depend on the backend's current index. No claim of exhaustive semantic references
is made for a text-only provider. Protocol reference:
[LSP document highlights](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/documentHighlight.md).

Native checks retain the same open document identity/revision and backend epoch
before and after the reply; unrelated document edits do not invalidate this read.
Every endpoint must fit the live LF/UTF-8 buffer in UTF-16 coordinates without
splitting a surrogate pair. Malformed rows are counted and omitted. Exact duplicate
ranges collapse, preferring Write over Read over Text when the provider reports
different kinds for the same span. Ranges are sorted by start/end.
Null means no highlights; a non-array, non-null top-level response is rejected.
The default row limit is 1000; callers may request 1–4096. Normalization visits at
most 8192 records and retains at most 4 MiB of normalized rows; truncation is explicit.

## Editor and inspector

Navigate / Find Action exposes Highlight occurrences at caret. The Rider profile
uses Ctrl+Shift+F7, following the documented
[Rider highlight command](https://www.jetbrains.com/help/rider/Navigation_and_Search__Finding_Usages__Highlighting_Usages_in_File.html).
View > Occurrences opens an ordinary dockable panel. Next/previous reported
occurrence, Select reported occurrences and Clear occurrence highlights are also
commands, available for custom bindings. Existing F3 document search and lexical
occurrence selection shortcuts remain unchanged in every profile.

Automatic highlighting defaults on for an already ready, enabled, capable server.
It waits 350 ms after the caret/source context changes. Only one request is in
flight, and an unchanged successful, empty or failed context is not repeatedly
requested. The panel can disable automatic requests; explicit refresh remains.
Untitled/image/debugger-source tabs, smoke fixtures and Unreal's preparing-analysis
phase do not start occurrence queries. Opened registered SDK text remains inspectable.
The automatic/kind settings are session-local.

Replies are tied to path, document identity/revision, backend epoch and main-caret
position. Late replies cannot move cursors. A changed context clears the old
geometry/report, and Clear also suppresses a pending reply for that context.
Moving to a different context clears suppression; an explicit highlight also retries. Internal next/previous/list
navigation retains the report's original query anchor while adopting the new
caret context; outside movement starts a new query. This is a single-anchor view,
not Rider's multiple simultaneously pinned highlight sets or import-use analysis.

The panel shows Text/Read/Write counts, invalid/truncated state and display counts.
Kind filters govern geometry, cyclic navigation and explicit selection. The common
CyUI text filter governs only the virtualized location list; its labels/indices
are cached until the report, filter, kind mask or locale changes. Locations display
one-based UTF-16 line/column. Clicking a row or using next/previous navigates the
current document only after checking its context again.

## Independent display and selection

The configured CodeEditor backend stores only one native glyph annotation. Adding
occurrences as squiggles would overwrite overlapping diagnostics or plugin/search
annotations. The host therefore builds its own bounded geometry model and draws
unfilled palette-coloured outlines after the existing CodeEditor draw. Syntax
foregrounds, native glyph annotations and Undo are not changed. The adapter uses
the backend's public document/visual/screen coordinates and CodePoint width API
for tabs, wide glyphs and wrapped/folded/scrolled rows, clipped to the editor child.
It shares no new backend copy and does not modify CyUI or the downloaded editor.
This requires an ImGui child-window lookup in the app adapter. Native multi-layer
glyph annotations/backgrounds remain a library limitation; runtime visual acceptance
of this independent outline approach is still pending.

Geometry retains at most 512 selected-kind occurrences, 64 lines per occurrence,
2048 line segments, 4096 glyphs per segment and 65536 glyphs total. The panel reports
display truncation independently of native query truncation. Only visible visual
rows are drawn. Theme palette roles and dp dimensions are read at draw time;
changed tab width or kind mask rebuilds geometry. EditorTextMap shares the host's
UTF-16-to-glyph boundary conversion with diagnostics, removing the duplicate decoder.

Select reported occurrences deliberately replaces the current cursor set with
sorted forward ranges from the selected kinds; it does not preserve an old main
cursor or backward selection direction. The entire report must be current and
complete (`invalid=0`, no query truncation). Every selected span must be nonempty
and disjoint, with at most 128 selections. All native ranges and editor glyph
positions are preflighted before a single SetSelections call. There is no queued
selection waiting for an asynchronous report, no partial selection and no source
edit or Undo entry. Display truncation alone does not discard valid query rows.

## Validation scope

Existing semantic, command, module and language-lifecycle regression sources cover
normalization/kinds/deduplication, malformed and Unicode ranges, budgets, navigation
wrap/masks, selection limits/overlap/atomicity, source/Undo/annotation preservation,
tab/wrap geometry, source/backend lifetime, unrelated edits, unsupported providers,
clear-during-request and current-caret checks. They are source-checked with MSVC
/Zs, not linked or executed under the current Kaspersky suspension.

Windows x64 production compilation and actual SDK manifest/schema plus pure
bridge/data checks are recorded in [Validation](Validation.md). No GUI, language
server, SDK Runtime/native client, plugin or test executable is launched for this
milestone. Actual timing, rendering, keyboard/focus, wrapping/folding/scrolling,
Unicode, Undo interactions, multiple themes/densities/DPI/windows and Linux/macOS
acceptance remain unverified. No dependency, CyUI module or executable target was added.

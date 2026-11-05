# Document structure and symbol navigation

Three connected features reuse the existing native language services: a bounded
hierarchical symbol catalogue, a dockable Structure panel, and an explicit language
server choice in Go to type / Go to symbol / Search Everywhere. No independent
parser, index, Python editor model, dependency or CyUI module is introduced.

## Shared native results

`language.documentSymbols` / SDK `language_document_symbols` and
`language.symbols` / `language_symbols` retain the flat `symbols` compatibility
list. Each row now also has:

| Field | Meaning |
| --- | --- |
| `node`, `parent`, `depth` | Nonzero snapshot-local ID, represented parent ID (0 for a root), nesting depth |
| `range` | Existing navigation/selection range, zero-based UTF-16 |
| `fullRange` | Complete symbol extent for caret containment |
| `deprecated` | Server deprecation flag/tag |

Names, details, kinds and containers come from the server. Hierarchical
DocumentSymbol trees preserve their represented parents. Flat SymbolInformation
responses remain roots; the application does not infer a tree from names.
Duplicate rows are removed. A filtered, unavailable or malformed parent may leave
valid descendants as roots, retaining their textual container context. Node IDs
belong to one result snapshot and are not persistent semantic identities.

Results include `backend`, configuration `epoch`, `invalid` malformed-row count,
`outsideWorkspace`, `truncated` and the existing encoding marker. Document results
also include `path`, `revision` and `documentId`. Optional `expectedEpoch` and
document `expectedDocumentId` inputs check the caller's snapshot. Language status
exposes each server's epoch, including contributed server IDs; the existing ABI2
JSON host exports share these results without a C ABI change. Existing permission classifications remain. The added language_symbol_resolve
operation brings the SDK surface to 226; ABI2 adds language.symbolResolve without
a C ABI change.

Document replies require the same open identity/revision and backend epoch after
the server responds. Closing, replacing, editing or reconfiguring invalidates the
reply. Unrelated open-document edits no longer invalidate a document structure
query. The owner verifies that selection and body ranges refer to that same file
and fit its current UTF-16 buffer. A single pass indexes line lengths and surrogate
splits; validating many symbols does not repeatedly scan the entire file.
Registered SDK sources keep their issued read-only references and gain a new
identity on loading a new source snapshot.

Workspace queries retain their workspace-edit/configuration guard; returned
positions remain index observations rather than verified current document ranges.
External sources still require registered SDK/engine roots and explicit
`includeExternal`. No arbitrary external path access is added.

Bounds are 1,000 returned rows (or the smaller requested limit), 8,192 considered
records, nesting depth 64 and a 4 MiB normalized symbol array. Names/details/URIs
and tag arrays are bounded. Malformed ranges, kinds and envelopes are rejected or
counted. Exceeding a collection budget reports truncation. A malformed present
range is not treated as an unresolved symbol. URI-only workspace entries now use
the deferred workflow below; document symbols still require real ranges.

## Deferred workspace-symbol locations

The client advertises `workspace.symbol.resolveSupport` for `location.range`,
plus the existing supported kinds and deprecation tag. The worker checks the
server's `workspaceSymbolProvider.resolveProvider` before sending
`workspaceSymbol/resolve`; `language.status.methods.symbolResolve` exposes that
capability for each configured built-in/contributed backend. This follows the
[official LSP 3.17 workspace-symbol contract](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/workspace/symbol.md).

Complete rows retain their previous shape. A valid URI-only workspace row carries
`unresolved: true`, `resolveIndex`, normal display metadata and a scoped `path`;
`range` and `fullRange` are absent. Its report carries a `session` reference.
There is no fabricated line zero. Names, kinds and tag/text validation still apply;
source-access filtering occurs before issuance. Exact duplicate deferred records
share one row and do not consume a second opaque-data cache entry.

GUI, MCP `language_symbol_resolve` and ABI2 `language.symbolResolve` submit only
`session`, `index` (the row's `resolveIndex`) and optional `expectedEpoch`.
The native owner retains the original server object/data and backend; callers
cannot inject arbitrary resolve payloads. A successful response contains one
complete `symbol`, `backend`, `epoch` and `positionEncoding: utf-16`.
The returned name/kind/URI must match the issued item, and only its location range
is adopted. The same normalized UTF-16 range and workspace/registered SDK source
guards apply again. Read-only SDK references remain read-only; no command, source
edit, arbitrary external read or backend installation/activation occurs.

Sessions are process/workspace local: at most four, each expiring two minutes
after creation. They hold at most 1,000 raw items, 64 KiB per raw object and 1 MiB
of raw/resolved cache payload per session. The independent normalized list stays
bounded to 4 MiB. Oversized raw rows increment `invalid`; unavailable/outside
sources increment `outsideWorkspace`. Unknown, unissued, expired or evicted
session/index pairs require a new search. Successful bounded resolutions are
cached without extending expiry; a full cache may return a valid result without
retaining it. No persistent semantic identity or index is added.

Workspace buffer/lifecycle generation, observed workspace-monitor revision and
backend epoch are captured at search, checked before resolving (including cached
hits) and checked after the server reply. A changed context requires Refresh.
Background filesystem activity can invalidate this conservative session guard;
Refresh retries against a newer observation. The monitor is not an atomic disk
snapshot, and workspace symbol
positions remain server-index observations rather than verified live-buffer
ranges. Document structure retains its more precise document identity/range
checks and permits unrelated edits.

## Structure panel

View > Structure opens an ordinary dockable/multi-window panel, hidden by default.
Existing session visibility and docking restoration include it. It follows the
active supported source and uses the same document-symbol service as the member
palette and SDK. Images, untitled drafts and debugger source references do not
issue structure requests. Opened SDK sources remain read-only.

The panel provides a shared CyUI filter with matching ancestor context, source or
alphabetical order, a caret-following scope and explicit Locate/Refresh actions.
Alphabetical sorting reuses CyUI's ASCII/Latin case/accent folding; this is not
full Unicode collation. Both sort orders and filtered visibility are cached.
Locate clears the current filter and reveals the enclosing scope. Caret-follow
reveals a newly entered scope without discarding the filter. Source extents are
half-open, with exact point ranges also supported.

Click a symbol label or press Enter on a focused tree node to navigate through
the existing document/history service. Arrow/disclosure actions retain tree
navigation. Clicking a destination disables caret-follow until re-enabled.
Caret reveal opens ancestors and scrolls inside the tree's own child view.
Kind, deprecation and server detail are shown without translating source names.
Filters, sorting and follow state are view state for the current workspace;
only the normal panel visibility/docking state is persisted.

Requests are debounced and limited to one in-flight structure request. Request
tickets, document ownership and configuration epochs prevent obsolete replies
from appearing. Hidden/reopened panels can repeat the same request. Empty results
do not trigger a polling loop; Refresh, a document edit or a context change can
request again. C++ structure stays inactive during desktop Unreal preparation.
No server is installed or enabled automatically. An already enabled lazy server
may initialize when the user requests structure, through its existing owner.

## Symbol palette

The server selector offers Automatic (active document, falling back to C++) and
enabled available built-in/contributed servers. Known unsupported workspace-symbol
methods are disabled. One chosen server is searched; this is not a merged search
across all language indexes. The selection lasts for the current workspace.
File structure/member lookup always belongs to the active document's backend.
Kinds and server details help distinguish similarly named results.

Changing the query/server/context, closing the palette or switching modes prevents
late results/errors from populating another query. Backend-picker Enter/arrow
handling is separated from result activation. Existing Rider/Visual Studio/VS Code
bindings and `@` member / `#` symbol prefixes remain. The new persistent view has
no default shortcut; custom bindings can target `view.structure`.

Deferred rows show “Location on selection” instead of a line number. Click/Enter
keeps the palette open, shows resolution progress and opens the source only after
a current successful reply. Errors stay visible with Refresh available. Changing
the query, server, mode or selected action/file, or dismissing the popup prevents
a late resolution from opening a document. An explicit Refresh creates a new UI
request ticket even for the same text, so an older same-query reply cannot win.
Resolution uses a separate single in-flight UI future; dismissing the view discards
its reply rather than terminating the language server. Existing worker deadlines
and queue limits remain. These view/timing interactions still need native/GUI
acceptance under the current test suspension.

## Validation limits

Regression sources extend existing semantic, module and lifecycle fixtures:
hierarchical/flat/duplicate/malformed/deferred responses, bounds, UTF-16/point
scopes, cached order/filter changes, opaque-data preservation/capability negotiation,
issued index/session checks, resolution caching/eviction, unrelated edits,
close/reopen/replacement, server disable, malformed/changed destinations and stale
palette/panel/selection replies. They are checked with MSVC `/Zs` only.
The real installed SDK loader and schemas validate the new contracts and EN/FR
data. Actual server timing/filesystem, keyboard/focus, docking/multi-window,
Unicode destinations, SDK/plugin execution, themes/DPI and Linux/macOS acceptance
remain pending under the Kaspersky native-test suspension. See
[Validation](Validation.md).

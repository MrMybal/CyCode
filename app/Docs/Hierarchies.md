# Call and type hierarchies

Navigate > Call hierarchy / Type hierarchy inspects the symbol under the caret.
View > Hierarchy reopens the dockable panel. Both commands are also available in
the command palette and editable keymaps:

| Profile | Call hierarchy | Type hierarchy |
| --- | --- | --- |
| Rider (IntelliJ-style) | Ctrl+Alt+H | Ctrl+H |
| Visual Studio | Ctrl+Alt+K | Menu/palette; user-assignable |
| VS Code | Shift+Alt+H | Menu/palette; user-assignable |

These defaults follow the official
[Rider keymap](https://resources.jetbrains.com/storage/products/rider/docs/Rider_default_win_shortcuts.pdf),
[Visual Studio shortcuts](https://learn.microsoft.com/en-us/visualstudio/ide/default-keyboard-shortcuts-in-visual-studio)
and [VS Code C++ navigation](https://code.visualstudio.com/docs/cpp/cpp-ide).
They describe these profiles, not every keymap supported by those products.

## Desktop workflow

Place the caret on a function or type in an editable workspace source document,
then inspect it. The panel switches between callers/callees or base/derived types.
Expand a row to request that branch from the original language server. Only the
first root expands initially. Further descendants load when opened; recursive
relationships and depth limits remain visible rather than recursing indefinitely.

Select a row to inspect its call sites. Open symbol, double click or Enter on a
focused tree row navigates to its declaration through the existing UTF-16-aware
document/SDK navigation service. Select a call site to visit that individual call:
incoming sites are in the caller, while outgoing sites are in the expanded source
function, not in the callee's declaration. Opening a clean file does not itself
invalidate the browsing session.

Refresh at caret starts a new hierarchy from the current document/cursor. It is
also the recovery path after edits, filesystem notifications or a backend change.
The panel uses CyUI Core controls/theme roles, stable nested tree IDs, DPI-scaled
dimensions and EN/FR messages. It adds no UI-library copy or dependency.
This is a read-only graph inspection; source text, editor Undo/Redo, saves and
refactoring transactions keep their existing owners.

## Actual language-server contract

The common LanguageService advertises static call/type hierarchy support and
checks callHierarchyProvider/typeHierarchyProvider before sending a request,
including for C++ and C#. Status exposes both methods for command enablement.
No disabled backend is enabled by inspecting a hierarchy. The configured server
may start on the explicit request through its ordinary owner.

Preparation uses textDocument/prepareCallHierarchy or prepareTypeHierarchy.
Expansion uses callHierarchy/incomingCalls or outgoingCalls, and
typeHierarchy/supertypes or subtypes. The native cache preserves the server's
opaque data and exact item on follow-up requests. GUI, SDK and native plugins
receive only issued session/node IDs and validated display locations; callers
cannot inject a URI, opaque data or a server command.

The source must be a workspace document with expectedRevision. Descendants/sites
can use workspace locations or, when includeExternal is true, already registered
read-only SDK roots. Unsupported metadata/non-file URIs and unregistered external
locations are omitted with an explicit count. Starting at an SDK/untitled document,
arbitrary external browsing, Blueprint calls, runtime/dynamic call completeness
and unsupported server methods are not implemented by this view.

This uses the official LSP
[call hierarchy](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/callHierarchy.md)
and [type hierarchy](https://raw.githubusercontent.com/microsoft/language-server-protocol/gh-pages/_specifications/lsp/3.17/language/typeHierarchy.md)
contracts. Results depend on the enabled server, project configuration and index;
there is no text-search or inferred fallback presented as a semantic hierarchy.

## Sessions, bounds and concurrency

- Eight native sessions, five-minute lifetime from preparation, 8 MiB combined
  payload; at most 512 nodes / 2 MiB per session, including raw and public data.
- At most 2 MiB per server response, 256 returned relationships and 256 call sites
  per relationship. A raw item is limited to 64 KiB; names/details to 4/16 KiB.
  Truncation and omitted locations are exposed; malformed data fails explicitly.
- A desktop session caches up to 64 branches / 8 MiB, draws at most 4,096 rows
  per frame and limits a visible chain to 32 levels. Start from another symbol
  to continue after reaching a limit.

Sessions pin document identity/revision, native document-event generation,
workspace notification revision and backend epoch. Edits, saves, closes, reloads,
conflicts and resource/recovery changes invalidate the context. Status checks
perform no LSP request or synchronous filesystem rescan. The visible panel polls
validity at most twice a second and checks again before cached navigation.
Expansion verifies the context before and after the server response.

Cache additions are staged under the Engine lock. Concurrent expansions reject
conflicting issued-node additions so a returned ID cannot silently refer to a
different server item; retry the branch. Queries release the document lock while
waiting on the existing bounded language worker. Deadlines and the 32-request
per-backend queue remain owned by LanguageService. New hierarchy-specific
cancellation is not provided.

Validity reflects events already observed by the application. Notification lag,
external unopened-file changes on polling backends and the language server's
asynchronous index can still affect results; status does not prove a complete or
synchronously refreshed call graph. Explicit refresh remains necessary.

## SDK and plugin surface

language_hierarchy_prepare accepts path, zero-based UTF-16 position,
expectedRevision, kind (call/type) and optional includeExternal.
language_hierarchy_expand accepts sessionId, issued item and direction
(incoming/outgoing or supertypes/subtypes for the corresponding session kind).
language_hierarchy_status accepts sessionId and returns valid/reason, including
valid:false for unknown/expired sessions.

All three are read-only real CyToolsCore operations without mutation permissions
or an exclusive mutation group. The existing native plugin JSON host request
exports the same three language.* methods, without changing the C ABI or exporting
STL/ImGui objects. Native plugins retain their existing trusted-code boundaries.
Paths are persistent workspace/issued SDK references, not imported task artifacts.

## Acceptance

See [Validation](Validation.md) for checks actually completed in this milestone.
Pure range/item/routing and keymap regression assertions extend the existing
opt-in cycode_tests target; no new executable/runner is introduced. Current local
verification uses MSVC /Zs and production builds with tests disabled.
Actual LSP preparation/expansion, plugin/MCP execution, cache concurrency/expiry,
keyboard/scroll/focus, themes/densities/DPI and Linux/macOS remain unexecuted while
the Kaspersky interruption is unresolved. Compilation and schema examples do not
establish runtime acceptance or complete support by every language backend.

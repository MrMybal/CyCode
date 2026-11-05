# C++ and C# semantic editing

Document highlights now use actual server Text/Read/Write results through the
shared native owner, with precise source lifetime/backend checks and bounded
UTF-16 normalization. The GUI adds a debounced single-anchor outline layer,
filtered occurrence inspection, navigation and preflighted multi-selection.
Text-kind reports may be fuzzier than symbol references; no local name classifier
or exhaustive-usage claim is added. See [Document occurrences](Occurrences.md)
for the native contract, one-glyph-annotation limitation and pending acceptance.

Declaration, type definition and implementation now join definition/references
through actual configured language servers, including contributed backends.
Precise source identity/revision and backend epoch guards permit unrelated edits;
shared bounded Location/LocationLink validation protects all five result forms.
The References dock adds filtered keyboard selection, copy and explicit refresh
at the active caret. Other-file positions remain index observations. See
[Source navigation](SourceNavigation.md) for capabilities and pending acceptance.

The shared native symbol services now preserve bounded hierarchy, complete extents,
selection ranges and deprecation, with precise document/backend ownership guards.
Workspace queries can retain URI-only entries and resolve their position on
selection through the original server using issued bounded sessions. SDK/ABI2 and
the palette share expiry/cache and workspace/lifecycle/monitor/backend guards.
They power the Structure view and explicit per-server palette search across
configured languages. This adds presentation/navigation rather than a new parser
or index. See [Symbol navigation](SymbolNavigation.md) for limits and pending
real-server/GUI/SDK/plugin acceptance.

C# uses the same native document/revision/edit transactions through a separate Roslyn
server. Completion, definitions/references, type/member lookup, semantic rename and
Roslyn formatting use actual project references. See [C#](CSharp.md) for prerequisites,
activation, synchronization and limits. The clangd-specific details below apply to C++.

The GUI and CyTools/MCP call the same native Engine and clangd connection. Correct
compilation flags and an available index are necessary. Unreal analysis preparation
provides UHT headers and the managed compilation database automatically.

## Semantic coloration

The ordinary editor augments its immediate lexical colors with semantic tokens
from clangd or Roslyn. This classifies project aliases, methods and function references,
locals/parameters/fields and enum constants using project analysis. Unreal naming
conventions remain a fallback; actual variable classifications can override a guessed
F/U/A-prefixed type. Unreal's generated headers and compilation database remain
necessary for complete engine results. clangd also identifies inactive preprocessor
branches, shown with a muted foreground. Both dark and light themes use the same
roles with contrast adjustment.

The UI waits 300 ms after a change, prioritizes visible documents and allows two
color queries at once. Stale revisions/generations are discarded, and an edit or undo
immediately reverts affected displays to lexical coloring. Source text, undo history,
identifier selection and completion are independent from the display overlay.
Malformed or unsupported responses leave lexical coloring available. Unknown token
kinds retain lexical colors; modifier styling, range updates and full backend/
framework/Unreal-version coverage are not claimed.

Full/delta transport is now negotiated through the same native worker, with
bounded immutable bases and a single full-request fallback. It still reconstructs
the complete document overlay; actual delta/GUI acceptance remains pending.
See [incremental token updates](SemanticTokenUpdates.md) for bounds, refresh/
restart invalidation and remaining verification.

MCP language_highlight returns the same normalized tokens with source revision,
backend generation, supported and truncated flags. Its positions and lengths are
zero-based UTF-16; glyphStart/glyphEnd are distinct Unicode codepoint indexes used
by the native widget. The operation accepts an opened issued SDK reference without
granting filesystem writes. C# must first be enabled through language_configure.

Protocol references: [LSP semantic tokens](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/semanticTokens.md),
[clangd classifications](https://clangd.llvm.org/features#semantic-highlighting).

## Search and navigation

Call/type hierarchy now complements symbol lookup with actual LSP callers/callees
and base/derived types. Navigate commands, lazy tree expansion and individual
call-site navigation share issued revision-guarded sessions with the SDK and
native plugin host. Backend capabilities and registered SDK roots still apply.
New runtime/GUI acceptance is pending; see [Hierarchies](Hierarchies.md).

The search palette has Types, Symbols and Members tabs. Workspace symbols use
workspace/symbol; file structure uses hierarchical textDocument/documentSymbol and
retains container names. Queries are debounced, stale results discarded and locations
opened with UTF-16 coordinate conversion. Results are bounded to 1,000 symbols and
may be incomplete while clangd indexes. Rider defaults are Ctrl+N, Ctrl+Alt+Shift+N
and Ctrl+F12. Prefixes `#` and `@` select symbols and members.

Definitions/references can opt into external SDK locations. Only discovered Unreal
Engine/Source and Engine/Plugins roots, compiler headers and registered OS SDK roots
are eligible. The Engine issues sdk:// references after validating canonical paths;
sources.read accepts only issued references, rechecks their root and opens a read-only
document. The UI shows its absolute path and read-only badge. Workspace-relative
document operations still reject arbitrary external paths and SDK writes. SDK source
files are UTF-8, at most 4 MiB, and do not expand editable workspace permissions.
The installed UE 5.3 Actor.h path was exercised on Windows; other platforms remain
unverified. Nonstandard SDK installation roots may need additional discovery.

## Rename, preview and undo

Shift+F6 prepares a C++ rename using prepareRename and rename. It is a semantic
operation, not a workspace string replacement. An explicit before/after preview
lists affected files and edit counts. Code wraps within each column; whitespace
decoration is hidden. Changed lines use subtle semantic red/green backgrounds and
margin markers; syntax contrast is adjusted against plain and changed backgrounds.
Theme/language changes refresh the preview styling and marker labels.

Apply changes validates every affected live revision/text and disk baseline before
changing any buffer. It opens affected files in the background and preserves the
originating editor. No source file is saved implicitly. Save all persists changes
through the ordinary conflict-checked document service. Undo refactoring reverses
the whole transaction only when every affected document still matches the applied
revision/text and its current disk baseline. It also works after an explicit save,
creating unsaved reverse edits. Later editing, closing a document or an external disk
change rejects the whole transaction rather than partially reverting it.

There are at most eight retained plans, each with up to 100 files and 16 MiB of
before/after text. Old plans are pruned after 20 minutes when new plans are created.
Edits are validated for UTF-16 bounds, surrogate boundaries, overlap and expected
old symbol text. File create/delete/rename operations and SDK modifications are
refused. Versioned documentChanges require the shared LSP owner's current overlay
mapping. Index completeness limits cross-file rename coverage.
Blueprint assets, serialized Unreal names, arbitrary macro transformations and
other languages are not handled by this C++ rename implementation.

Annotated text edits now retain labels/descriptions, effective file groups and
per-plan confirmation IDs through the shared preview/apply/undo path. The desktop
shows annotated corrections before applying; required labels need individual
confirmation. SDK/plugins use the same native guard. Actual annotated-backend/
GUI acceptance remains pending; see [annotated changes](ChangeAnnotations.md).

## Formatting

Format document (Rider Ctrl+Alt+L) and Format selection (Ctrl+Alt+Shift+L) request
clangd formatting, including the project's .clang-format rules, tab size and spaces.
The normal UI command applies the validated result to live buffers and supports
editor undo. MCP prepares a plan; changes_apply explicitly performs the edit.

The public MCP operations are described in [MCP integration](MCP.md). Native tests
exercise real clangd, cross-file rename, shadowed identifiers, Unicode, formatting,
revision/disk conflicts and SDK access checks. Desktop acceptance uses real keyboard
input and captures the preview in all six themes plus French.

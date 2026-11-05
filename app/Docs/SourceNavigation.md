# Source navigation and result locations

Definition, declaration, type definition, implementation and references share one
native read-only pipeline. They use the configured language server for the source,
including contributed backends; there is no independent parser or guessed fallback.
The existing References dock hosts results for all five operations and preserves
its layout/session identity.

## Commands and language support

| Command | Native / SDK | Default bindings |
| --- | --- | --- |
| Go to definition | language.definition / language_definition | Existing F12 and profile bindings |
| Go to declaration | language.declaration / language_declaration | Assignable; no default |
| Go to type definition | language.typeDefinition / language_type_definition | Rider Ctrl+Shift+B; assignable elsewhere |
| Go to implementation | language.implementation / language_implementation | Rider Ctrl+Alt+B; Visual Studio / VS Code Ctrl+F12 |
| Find references | language.references / language_references | Existing profile bindings, including Shift+F12 |

All commands use the existing menu/action catalogue and keyboard preferences.
Rider's build remains Ctrl+F9; the former extra Ctrl+Shift+B build alias is removed
from that default profile so it can select the type. Other profiles retain their
build bindings. Explicit user overrides remain editable through keyboard settings.
Rider's C++ shortcut reference and type navigation reference are
[documented by JetBrains](https://www.jetbrains.com/help/rider/Navigation_in_CPP.html)
and [Go to Type Declaration](https://www.jetbrains.com/help/rider/Navigation_and_Search__Go_to_Type_Declaration.html).
The other implementation defaults follow
[Visual Studio](https://learn.microsoft.com/en-us/visualstudio/ide/navigating-code?view=vs-2022)
and [VS Code](https://code.visualstudio.com/docs/editing/editingevolved).

The three added operations require the corresponding declarationProvider,
typeDefinitionProvider or implementationProvider on every backend. A ready server
without the capability disables the UI command, and the native worker rejects
unsupported calls explicitly. It never substitutes a definition result for an
unsupported operation. Existing definition/reference routing remains compatible.
The status methods expose declaration, typeDefinition and implementation.
No server is installed or enabled by inspection; an already enabled lazy server
can initialize on a request through the existing service owner.

The initialize capabilities advertise static linkSupport for definition and the
three new requests. These follow the official LSP contracts for
[declaration](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/declaration.md),
[type definition](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/typeDefinition.md)
and [implementation](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/implementation.md).
Language/project/index completeness and the meaning of returned implementations
remain properties of the actual backend. This does not implement Rider-specific
inheritance analysis, decompilation or a merged cross-language index.

## Shared native location catalogue

The input keeps path, expectedRevision and a zero-based UTF-16 position. Optional
expectedDocumentId and expectedEpoch check the caller snapshot. includeExternal
retains the existing registered SDK/engine root policy, and references retains
includeDeclaration. The native owner checks the same source identity/revision and
backend epoch after the server reply. Editing another document no longer rejects
a source navigation read. Editing, closing/replacing the source or changing its
backend rejects the reply. Other semantic mutations/completion retain their
existing workspace-generation rules; document identity is also checked there.

NavigationLocations accepts null, one Location, an array of Locations or
LocationLinks. The existing path/range navigation fields remain. Every normalized
row includes fullRange; a link may also include originRange. Target selection
must lie within its extent. All ranges have ordered nonnegative integer LSP
coordinates. Origins must fit the exact live UTF-16 source without splitting a
surrogate pair. A target referring to that same source is checked against its
live buffer as well. URI strings are nonempty, NUL-free and at most 8 KiB.

The owner resolves only workspace or explicitly permitted registered SDK sources.
External references remain issued read-only handles. Source scope is checked;
target existence/content is read through the existing guarded opener when chosen.
Other-file positions remain server-index observations, even when another buffer
is open. They are not an atomic disk snapshot or verified current target revision.

Duplicate path/selection destinations collapse to their first valid row, retaining
that row's extent/origin. Results sort by path, then selection start/end. Limits
are 5,000 returned rows (or the smaller requested limit), 16,384 considered records
and 4 MiB for the normalized location array. Stopping at a budget reports
truncated. Malformed ranges/rows, invalid URIs and resolution exceptions count as
invalid; the existing outsideWorkspace counter counts scope-filtered/unavailable
source references. The core is UI-free and reuses Hierarchy range validation and
SymbolTextBounds, without a new parsing/indexing dependency.

Reports also include source path/revision/documentId, backend/epoch, method,
the requested UTF-16 position and positionEncoding. SDK output schemas bind method
to the called operation. GUI/MCP and ABI2 plugins call the same native owner.
The three added SDK operations bring the manifest to 226 operations; the ABI2 JSON
exports add those three native methods without a C ABI or permission change.
They are read operations without permissions or mutation/execution groups.

## Result view and activation

The header identifies the operation and source position. A shared CyUI filter
narrows cached path/line/column labels. Previous/Next and Up/Down select matching
rows cyclically; Enter or a row click opens the selected destination through the
existing document/history service. Keyboard selection can reveal a clipped row
inside the result child view. Copy locations copies the filtered labels without
reading target source contents. Invalid/unavailable/truncated counts remain visible.

Single-result non-reference queries jump automatically only while the source tab,
identity/revision/backend and original active caret still match. Multiple results
and references remain in the dock. UI query tickets discard obsolete replies.
At reply consumption the owner refreshes language status before checking readiness
and epoch, so lazy-server initialization is not judged against an old status copy.

Results retain their observations when the source closes/changes or its backend
changes; a stale warning blocks opening those rows. Inspection/copy remains
available. Refresh at caret explicitly reruns the selected operation in the
currently active supported source and clears the result/filter selection. It does
not pretend to refresh an old source position. Filter/selection are transient view
state; the existing panel visibility/docking persistence remains.

Capability checks borrow the current server/method objects instead of copying
all server diagnostics every frame; the command capability map is immutable and
created once. This removes repeated snapshot copies without claiming a measured
benchmark. Ordinary editor edits, close, reload and accepted completions retain
the observations for the stale-context guard. Workspace/resource changes can still
clear the result set. Activation does not access a borrowed result after source-opening side effects.

Existing CyUI Core filtering, palette roles and ImGui controls supply the view;
no module, adapter, dependency, process or Undo owner is added. Native data and
editing/history ownership remain in CyCode.

## Verification limits

Regression sources extend the existing semantic, command, module and language
lifecycle targets. They specify Location/Link normalization, duplicates/order,
malformed/Unicode origin ranges, budgets, filter/cache/keyboard wrap, bindings,
capability negotiation, caller identity/epoch, unrelated edits, edit/close/
replacement/disable, unsupported providers and late UI tickets. They are compiled
with MSVC /Zs only; no fixture executable is linked or run.

The actual SDK loader and schemas validate all 226 operation contracts, exact
native facade forwarding, invalid forms/bounds, permissions and EN/FR labels.
Windows production builds keep BUILD_TESTING=OFF. Actual language-server/index/
SDK/plugin behavior, keyboard/focus/scroll, Unicode destinations, themes/DPI/
multi-window and Linux/macOS acceptance remain pending under the Kaspersky native
test suspension. See [Validation](Validation.md).

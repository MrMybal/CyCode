# Commands and keyboard profiles

Open included file follows supported literal C/C++ directives, including inline
comments and immediate backslash-newline continuations. Find related C/C++ files...
always opens the counterpart inspector; its result actions provide Retry,
explicit index refresh/retry, path/report copying and contextual local help.
The configured single-stroke help.context binding works in that modal; default
Shift+F1 opens the relevant section. These new actions have no default binding.
SDK/ABI2 share workspace.include; see [Includes](CppIncludes.md) and
[Related results](CppCounterparts.md#inspect-related-results).

Navigate > Switch header/source uses Rider Alt+O or Visual Studio Ctrl+K, Ctrl+O,
and Find Action without requiring a language server. One complete match opens
through the ordinary owner; ambiguous/partial results use the shared palette.
See [C++ companions](CppCounterparts.md).

Edit > Insert snippet... and Find Action open the shared CyUI palette for saved
project insertion snippets. Rider uses Ctrl+J; Visual Studio uses Ctrl+K, Ctrl+X.
The VS Code profile has no new default binding. Availability requires a writable
saved workspace document and one caret. With a selection, Insert snippet uses
wrapping; Edit > Surround with snippet... requires a nonempty selection and uses
Ctrl+Alt+J in the Rider profile. VS/VS Code expose Surround without a default
binding. Both use TM_SELECTED_TEXT and disable bodies that cannot consume the
selection. Escape closes the list;
Enter prepares the selected issued proposal through the ordinary atomic editor
completion path. Edit project snippets... opens the fixed project catalogue,
creating it only on that explicit action and leaving its initial JSON buffer
unsaved. Existing files and dirty buffers are retained. See
[Project snippets](SnippetCatalogue.md) for scope and format.

Edit > Create user snippet from selection... requires one selected saved workspace
source and opens a literal personal JSON draft. Review its name, prefix and scope
before explicit Save; the source is not edited. Find Action exposes it without a
new default key binding. See [Selection capture](SnippetCapture.md).

Edit > Edit user snippets... opens the shared JSON editor for the fixed personal
catalogue, also through Find Action with no new default binding. Ctrl+S explicitly
saves/closes this modal; draft Undo/Redo, Reload and confirmed dirty close/quit
reuse common controls. This catalogue is shared by projects in the portable
installation. See [Personal snippets](UserSnippets.md).

View > Application updates opens the installation update view; Check for updates
is also available there and through Find Action, with no new default shortcut.
Startup checking is configurable and enabled by default. A newer stable release
gets an EN/FR proposal with Later and Skip this version. Download and Install
update and restart are separate actions; the latter uses the existing document
save/discard/cancel close flow. Native test/smoke sessions disable I/O and handoff.
See [Application updates](Updates.md).

Navigate / Find Action includes Highlight occurrences at caret (Rider
Ctrl+Shift+F7), Next/Previous reported occurrence and Clear occurrence highlights.
Edit includes Select reported occurrences; View > Occurrences opens its dockable
inspector. Other profiles/actions have no new default binding. Existing F3 find
and lexical occurrence selection are unchanged. All action availability is tied
to the current native report/provider. Selection rejects incomplete, overlapping,
empty or oversized reports before replacing cursors. See
[Document occurrences](Occurrences.md) for source references and scope differences.

Navigate adds Go to declaration, Go to type definition and Go to implementation,
using the real backend capability. The Rider defaults add Ctrl+Shift+B for type
and Ctrl+Alt+B for implementation; Ctrl+F9 remains build and the former extra
Ctrl+Shift+B build alias is removed to avoid a conflict. VS/VS Code add Ctrl+F12
for implementation. Declaration and other missing defaults remain assignable.
The References dock adds shared filtering, cyclic keyboard/result selection,
Enter/click activation, Copy locations and Refresh at caret; stale source/backend
snapshots remain inspectable but do not navigate. See
[Source navigation](SourceNavigation.md) for source references and limits.

View > Structure opens a dockable persistent panel without a new default shortcut.
Rider/Visual Studio/VS Code member/symbol bindings and @/# palette prefixes remain.
Structure row clicks or focused-row Enter use existing source/history navigation;
disclosure arrows keep tree behavior. The symbol/type/everywhere palette can
select one available enabled server, including contributed IDs; document members
always follow their own backend. Picker navigation does not activate a result. URI-only symbol rows display
“Location on selection”; click/Enter waits in the palette for a current resolved
position. Progress/errors remain visible. Refresh symbols creates a new request
ticket even with unchanged text. Query/server/mode/selection changes or popup
dismissal discard obsolete resolution replies; existing bindings remain.
See [Symbol navigation](SymbolNavigation.md).

Previous/Next diagnostic commands now navigate reported workspace or current-file
locations with the Problems text/severity filters and wrap behavior.
Unverified positions in open documents are retained for inspection/copy and
excluded from navigation. The selected message has its own detail card. Rider uses F2/Shift+F2 in
file and Alt+F2/Alt+Shift+F2 across reported locations; Visual Studio/VS Code use
F8/Shift+F8 for the latter, keeping VS Code F2 for Rename. CyUI icon controls and
guarded document opening share the same model. See
[Diagnostic navigation](DiagnosticNavigation.md) for scope and pending acceptance.

The command catalogue is shared by menus, keyboard dispatch and the action palette.
Run > Run to cursor resumes once toward a verified destination in the active saved
workspace document. It keeps existing breakpoints/exceptions and restores the
original source list on the next actual stop. Rider uses Alt+F9 and Visual Studio
Ctrl+F10; VS Code has no new default binding and custom bindings retain priority.
Run > Restore source breakpoints is an explicit recovery action, available only
with a confirmed pause and settled replies. See [Run to cursor](RunToCursor.md)
for guards and pending real-adapter/GUI acceptance.

Run > Step back / Continue backward / Restart selected frame are capability-gated
commands with user-assignable shortcuts and no new default binding. Debugger >
Execution controls sets the common stepping granularity and selected-thread mode;
existing step shortcuts and Continue use those same native options/context guards.
Frame restart is an explicit program action, separate from document Undo. See
[Execution controls](DebugExecution.md) for capabilities and pending acceptance.

Run > Choose step-in target opens the Debugger target list and reads actual
adapter choices for the selected paused frame. Select a function explicitly,
then use Step into selected target; ordinary Step into keeps its existing behavior.
The new query command has no default binding and can receive a custom shortcut.
See [Targeted step-in](StepInTargets.md) for captured context and pending acceptance.

Run > Set next statement queries destinations in the selected frame source and
opens the Next statement tab. Reading/selecting a destination does not move the
instruction pointer; Set next statement here is a separate explicit action.
It skips intervening code and does not run to the cursor. No default shortcut is
added; see [Set next statement](GotoTargets.md) for pending adapter/GUI acceptance.

Run > Apply Java code changes / Cancel Java reload use the same catalogue and
native JDT/DAP service. They have no assigned shortcut by default; keyboard
preferences can bind them. Application is manual and requires an active Java
session with correctly imported JDT sources/output. See [Java debugging](JavaDebugging.md).
Navigate > Call hierarchy / Type hierarchy and View > Hierarchy now use the same
catalogue. Rider uses Ctrl+Alt+H / Ctrl+H, Visual Studio Ctrl+Alt+K for calls and
VS Code Shift+Alt+H for calls; other bindings remain user-assignable. These inspect
actual capability-gated LSP relationships with lazy navigation, not string
references. See [Hierarchies](Hierarchies.md) for scope and pending runtime acceptance.
Options > Keyboard shortcuts selects Rider / ReSharper (default), Visual Studio or
Visual Studio Code. `--keymap rider|visualstudio|vscode` overrides the startup profile.
These are Windows/Linux keyboard schemes. Dedicated macOS schemes remain pending.

The user-requested bindings are present in every preset:

| Shortcut | Action |
|---|---|
| Ctrl+Shift+F | Workspace text search |
| Ctrl+F | Current document find/replace |
| Ctrl+Alt+F | File name lookup |
| Double Shift | Search Everywhere: files, actions, C++ types/symbols and members |

Search Everywhere provides files, actions, recent files, C++ workspace symbols/types
and current-file structure. Results are selectable with Up/Down and opened with Enter.
The Actions tab or a `>` query searches commands; `@` selects members and `#` symbols.
Symbol results come from the live clangd index and can be incomplete while indexing.
Only commands implemented by the application are registered; unavailable actions
are disabled. A key binding is not an implementation of a debugger or refactoring.

The Rider profile uses Ctrl+B/F12 for definition, Alt+F7/Shift+F12 for references,
Ctrl+F9 for build, Ctrl+D for duplication, Ctrl+Y for line deletion and Ctrl+Shift+Z
for redo. Ctrl+Shift+N searches file names; New window is available from File or
Find Action. Visual Studio/VS Code use their separate editing/navigation mappings,
including Ctrl+K, Ctrl+C to comment and Ctrl+K, Ctrl+U to uncomment. Those actions
are idempotent. Chords time out after 1.5 seconds and cancel on Escape or focus change.
Caret movement, ordinary typing and completion are still supplied by the editor.
Editor editing shortcuts do not modify a document while typing in another field.

Interactive completion snippets have contextual Tab/Shift+Tab parameter navigation
and Escape to finish, in all three profiles. Ctrl+Space leaves the snippet and
requests a fresh completion. These contextual widget keys are currently fixed;
the parameter badge shows them and a choice field offers its alternatives in a
dropdown. See [Languages](Languages.md) for mirror behavior and current limits.

The catalogue also provides document close/reopen, save, undo/redo, clipboard,
duplication/deletion/movement, indentation, comments, selection growth, occurrences,
case conversion, whitespace trimming, go to line, recent files, navigation history,
tab switching, folding, view toggles, panels, builds and refresh commands. Saves and
closures retain revision/conflict/unsaved-document checks.

Semantic commands use actual language services:

| Action | Rider | Visual Studio | VS Code |
|---|---|---|---|
| Types | Ctrl+N | Ctrl+1, Ctrl+T | Types tab |
| Symbols | Ctrl+Alt+Shift+N | Ctrl+1, Ctrl+S | Ctrl+T |
| File structure | Ctrl+F12 | Ctrl+1, Ctrl+M | Ctrl+Shift+O |
| Rename symbol | Shift+F6 | Ctrl+R, Ctrl+R | F2 |
| Format document | Ctrl+Alt+L | Ctrl+K, Ctrl+D | Shift+Alt+F |
| Format selection | Ctrl+Alt+Shift+L | Ctrl+K, Ctrl+F | Ctrl+K, Ctrl+F |
| Quick fixes / context actions | Alt+Enter | Ctrl+. or Alt+Enter | Ctrl+. |

Context actions use the live cursor/selection and actual server proposals. The
Code actions panel offers filtering, arrow selection, Enter/Apply correction and
Preview changes. Unsupported actions show a reason. Apply edits unsaved buffers;
refactoring undo and explicit saving retain the native revision/disk guards.

Rename opens a multi-file preview before applying; Apply refactoring and Undo
refactoring are also executable through Find Action. Applying changes preserves the
originating editor and loads affected documents as unsaved buffers. Formatting uses
the project's .clang-format and normal editor undo. SDK tabs are read-only and
disable editing/refactoring commands. See [Semantic editing](Semantic.md) for limits.

Select an action in the keyboard settings to edit its bindings. Separate alternatives
with `;`, and chord strokes with `,` (for example `Ctrl+K, Ctrl+C; Ctrl+Slash`). An empty
list disables the binding. Duplicate or ambiguous prefix bindings are rejected while
retaining the previous valid map. Settings keep overrides separately for each profile.
Reset command/profile restores defaults. Menus/tooltips use the selected bindings.

The presets cover implemented commands. Remaining Rider functionality is tracked in
[Roadmap](Roadmap.md); full Rider, Visual Studio and VS Code feature parity is not claimed.

Primary references used to establish the schemes:
[Rider predefined keymaps](https://www.jetbrains.com/help/rider/Reference_Keyboard_Shortcuts_Index.html),
[Visual Studio shortcuts](https://learn.microsoft.com/en-us/visualstudio/ide/default-keyboard-shortcuts-in-visual-studio),
[VS Code keybindings](https://code.visualstudio.com/docs/configure/keybindings).

# Commands and keyboard profiles

The command catalogue is shared by menus, keyboard dispatch and the action palette.
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

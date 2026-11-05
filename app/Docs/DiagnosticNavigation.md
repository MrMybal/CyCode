# Diagnostic keyboard navigation

Navigate and Find Action expose Next/Previous diagnostic and their current-file
variants. Problems provides previous/next icon buttons with translated action and
current shortcut tooltips, plus the selected source location. Navigation opens
through the existing guarded document service, preserving edit/Undo ownership.

Problems also has Errors, Warnings and Information toggles. Both the list and
keyboard navigation use the same severity mask and text filter. The selected
diagnostic has a scrollable, wrapped detail card and Copy diagnostic action,
including its complete message and location. Interface colors and dimensions
come from CyUI Core; the list remains CyUI Diagnostics.

The source is the current reported build/project diagnostics and LSP publications
already used by Problems. No extra analysis engine, compiler run or solution-wide
index is created. Unmapped messages remain visible but cannot be navigation
targets. Existing compiler coordinates and LSP UTF-16 conversion are retained;
build diagnostics remain last-run observations and may refer to edited sources.

Language publications carry the native document identity and revision in
`language.status.diagnosticRevisions` (and each server's equivalent). Every Engine
sync, including reload/recovery/rename, supplies the identity. LSP wire versions
increase across documents and close/reopen within one connection; replacement
identities trigger didClose/didOpen even if a queued close was coalesced. Versions
are reused only after the old server connection is stopped. Exhaustion stops
synchronization and reports an error rather than wrapping.

Squiggles and navigation in open documents require enabled/ready servers and an
explicitly versioned publication matching both live identity and revision. The
Problems cache includes those document changes, not just diagnostic payloads.
Unchanged frames compare borrowed diagnostic payloads instead of copying the whole
catalogue; only real diagnostic/ownership/availability changes rebuild the model.
During desktop Unreal analysis preparation, C++ positions are temporarily inactive.
Old/unversioned positions in open documents remain visible and selectable for
inspection/copy, labeled Position not verified, but do not navigate. Inactive
server entries are similarly retained without navigation. Closed-file reports
are advisory positions and can still open the file, subject to workspace guards;
their position is labeled unverified. Build reports are labeled Last build result.
This does not transform ranges or infer freshness for unversioned servers.

Both directions honor the Problems filter and wrap. Workspace navigation follows
the filtered Problems order. Current-file navigation sorts mapped entries by
line/column; without a selected entry it starts at the current editor line in the
requested direction, then wraps. Subsequent steps cycle stable selected IDs. A
removed or filtered-out selection is resolved against the new candidate list.
Closing/switching workspaces clears selection and cached Problems state.

| CyCode profile | Current file | Reported workspace locations |
| --- | --- | --- |
| Rider | F2 / Shift+F2 | Alt+F2 / Alt+Shift+F2 |
| Visual Studio | Available through commands/custom bindings | F8 / Shift+F8 |
| VS Code | Available through commands/custom bindings | F8 / Shift+F8 |

VS Code F2 retains Rename. Existing keyboard overrides remain in the same keymap
store. Bindings follow the relevant defaults documented by
[JetBrains](https://www.jetbrains.com/help/rider/Reference_Keyboard_Shortcuts_Index.html),
its [current-file navigation guide](https://www.jetbrains.com/help/rider/Navigation_and_Search__Context_Dependent_Navigation.html),
[Visual Studio](https://learn.microsoft.com/en-us/visualstudio/ide/default-keyboard-shortcuts-in-visual-studio)
and [VS Code](https://code.visualstudio.com/docs/reference/default-keybindings).
The candidate/severity scopes above describe CyCode's current implementation,
not full inspection parity with those IDEs.

The selected editor destination and Problems header are updated. CyUI's current
diagnosticList API returns clicked IDs but exposes no programmatic selected-row
or scroll-to-row input. That list limitation is retained explicitly instead of
copying a divergent diagnostic widget into the app.

Source assertions specify severity/text filtering and cache changes, non-current
positions, unmapped entries, wrapping, file ordering, line anchors, keymap/rename
preservation and LSP close/reopen identity/version lifetimes. They are syntax-checked only under
the Kaspersky suspension. Actual keyboard/mouse, Unicode destinations, focus,
themes/DPI and Linux/macOS behavior remain unverified; see [Validation](Validation.md).

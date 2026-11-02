# CyCode modular CyUI integration

CyCode now consumes the shared editor API at the exact commit in
`app/editor-source.lock.json` and CyUI at the exact commit in
`app/cyui-source.lock.json`. The Reader application itself is not compiled.
Neither developer checkout is modified by configuration or compilation.

## Modules and migrated views

| Module | CyCode use |
| --- | --- |
| Core | Existing themes, fonts, icons, docking, notifications; shared locale catalogues and filters for menus, project files, diagnostics and settings |
| Code / CodeEditor | Editable documents, read-only SDK/debugger sources, Git source versions and wrapped refactoring previews; common 101-mode syntax catalogue including HLSL, GLSL and logs |
| Diff | Unified/split Git review and refactoring comparisons, synchronized scrolling, inline differences and hunk navigation |
| Commands | Action palette with stable command IDs and keyboard navigation; common ranking plus the IDE's abbreviation matching inside Search Everywhere |
| Diagnostics | Bounded build output with shared log severity classification, filtering and follow; build/LSP problem list and source navigation |
| DocumentViews | Native Markdown documentation from language servers; literal code sections use Code and plain text remains literal |
| Inspector | General settings with stable property IDs, validation, drafts and commit/cancel phases |
| Images / Canvas | Read-only image tabs with transparency, fit/zoom/pan and reload; the application owns decoding and GPU resources |

The IDE retains its project/symbol search tabs, asynchronous symbol requests,
keymaps, document permissions/revisions, multi-file refactoring transactions,
LSP/DAP/Unreal services, SDK/MCP bridge, Git actions, project configuration and
persistence. Its portable file icons remain shared with Reader. Native plugin
contracts continue to use their existing C ABI; no ImGui/STL objects cross it.
The app's Graphite theme is a shared palette adaptation, not a second widget set.

The old raw TextEditor-owned document/preview views, local refactoring marker
renderer, unstructured build-output textarea, diagnostic rows and settings form
have been replaced. Every native editor now belongs to CyUI::CodeEditor. Native
hooks remain necessary for Unreal completion, Tree-sitter/LSP overlays, snippets,
breakpoint gutters and guarded external buffer replacements. Those services and
their Undo history have not moved into CyUI.

## Dependency/build contract

```sh
python app/scripts/fetch_dependencies.py
python app/scripts/build.py --no-install
```

The default CyUI source is a separate commit-specific cache under `runtime/deps`.
Alternatively pass `--cyui PATH` for a clean Git checkout matching the lock.
`--editor-root PATH` explicitly selects a development Reader checkout with shared
source API 1. Production uses the pinned Reader source.
During local validation that Reader commit was obtained from the owner's local
Git repository. Its remote availability was not verified or changed. A fresh
machine needs read access to the exact dependency commits; the explicit local
checkout option remains available while those commits are unpublished.

CyCode validates its own newer CyUI lock before selecting the shared editor's
explicit host-source mode (`CYCODE_CYUI_DEVELOPMENT`). This does not relax the
host's revision or cleanliness checks. Reader's standalone lock remains intact.
Direct CMake configuration performs the same validation.

`CYCODE_EDITOR_UI_MODULES` is set before the shared editor's `add_subdirectory`;
that provider sets `CYUI_MODULES` before adding CyUI. The app links explicit
`CyUI::<Module>` targets. Audio, Gallery, Demo, CEF and library tests are disabled;
Canvas is selected automatically by Images. Plots/Nodes/Timeline/Assets/Viewport/Web
are not required.
Dependencies between the selected modules are resolved by CyUI.

One host `cycode_imgui` target (Dear ImGui >= 1.92) is supplied through
`CYUI_IMGUI_TARGET`. One `cyui_texteditor` backend serves all app and library views.
The shared adapter prepares its advanced hooks in the build directory; it never
changes upstream sources. The obsolete standalone TextEditor archive is no longer
a dependency. Tree-sitter and its grammars remain required for richer language
overlays. Lexilla/Scintilla are selected by Code for the built-in syntax catalogue.
The native core/CLI/file services still compile without any UI dependency.
C++20 is required by CodeEditor; platform backends remain SDL3/OpenGL.

## Preserved interactions and limits

Settings change only on commit. Filtering, closing or collapsing the settings
window cancels active drafts. Loading a configuration advances the model revision.
Menu IDs and problem destinations do not depend on translated labels or filtered
row indices. Ctrl/Cmd+F belongs to the focused filter panel; the document editor
keeps its own find/replace. Ctrl+Shift+F keeps the IDE workspace search binding.
Search Everywhere retains files, actions, types, symbols, members and recent files.

Diff does not implement editable three-way merges or file writes. CyCode retains
its three read-only conflict versions and guarded working-document resolution.
Diff also has no wrapped split renderer; the refactoring view retains a selectable
wrapped CodeEditor presentation. Its bounded comparison can be approximate on
large inputs; full before/after buffers remain available.

DocumentViews implements a Markdown subset, not HTML/CommonMark or a browser.
Links emitted by language-server documentation remain inert. Literal code is sent
directly to Code so fence syntax and all catalogue languages survive this subset's
limits. Large sections have their own scrollable view. No CEF engine is added.

Diagnostics is not an interactive terminal emulator. CyCode's bounded libvterm
grid, PTY processes, ANSI input, selection, clipboard and shell key ownership stay
in its terminal adapter. General project/debug/plugin panels with specialized
service interactions retain Core widgets rather than being forced into generic
properties. The migration does not add new LSP or debugger backends.

## Validation

`ui_modules` exercises the actual app adapters and views with headless ImGui
frames, without SDL/native windows: editing/Undo/Redo and explicit save through
Engine, catalogue selection, filtering/draft cancellation, diagnostics identities,
log severities and action dispatch. View checks cover six themes, both densities,
100/150/200% DPI and English/French. This verifies generated drawing data and
interactions; it is not a native desktop screenshot/renderer acceptance test.

See `Validation.md` for the executed build/test results. Windows/MSVC is the local
validation platform. Linux/macOS builds and native multi-monitor acceptance have
not been executed for this migration. No application window is opened or old
quarantined executable restored by the headless checks. Release validation and
publication are tracked separately in the project changelog and Validation.md.

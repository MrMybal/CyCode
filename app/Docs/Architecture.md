# Architecture

The desktop selects Core, Code/CodeEditor, Diff, Commands, Diagnostics,
DocumentViews, Inspector and Images/Canvas from modular CyUI. The Engine/core/CLI have no UI
dependency. See [CyUI migration](CyUI-Migration.md) for exact pins, view ownership,
source adapters and limits.

Image tabs use the app-owned `ImageLoader` worker, bounded native decoders and
deferred OpenGL textures, rendered by CyUI::Images. They are separate from Engine
text documents and retain the shared tab/docking lifecycle. See [Images](Images.md).

CyCodeReader lives in its [own repository](https://github.com/MrMybal/CyCodeReader).
Its editor/ source module provides the same cycode_editor and cycode_fileicons
targets to the IDE, at the revision in app/editor-source.lock.json or an explicit
CYCODE_EDITOR_ROOT development checkout. CyCode does not build the Reader app.
The Reader hosts dockable file editors without an
Engine, language server, build adapter or Core companion. cycode_files owns UTF-8
validation, newline encoding, bounded reads, path conversion and guarded atomic
saves used by both applications. The IDE retains workspace/revision permissions;
Reader uses files selected directly by its user and keeps per-document disk
baselines across directories. Its application-specific extensions are documented
in the Reader repository.

Interactive execution uses the shared native Terminals and Debugger services.
Terminals owns bounded libvterm screens/history and real ConPTY/POSIX PTY process
trees, with separate read/write/supervisor threads. Only visible grid rows are copied
for rendering. Debugger has one owner thread for DAP and a bounded command queue;
the framed JSON transport is shared with LanguageClient without LSP initialization.
Adapter reverse terminal requests create native terminals with explicit argv/env.
DebugRuntime detects native/managed outputs and selects a compatible .NET host.
CodeLLDB and NetCoreDbg share this DAP owner, inspection state and persistent profile;
NetCoreDbg output uses the debug console, while interactive managed Run uses a PTY.
Existing attached processes detach on stop; launched processes terminate. Persistent
profiles/breakpoints use atomically replaced workspace metadata. See [Execution](Execution.md).

CyCode starts with C/C++ and keeps language support, project/build adapters, editor
views and revision-control providers as separate extension points. A workspace can
eventually contain several project types. CMake, MSBuild and UnrealBuildTool are
executable adapters; solution inspection does not evaluate MSBuild. Explicit MSBuild
evaluation reads real target properties/items, produces managed clangd metadata and
tracks local solution/project/import inputs. Builds share the same native adapter
with MCP; source diagnostics feed the Problems panel.

ProjectTasks supplies Cargo metadata/compiler JSON, npm scripts and standalone
javac compilation to both GUI and MCP. It owns one cancellable native task with
bounded output/deadline and uses Terminals for persistent Run. The private IPC
bridge accepts a bounded set of concurrent connections so cancellation remains
receivable while a compiler is running. CyToolsCore continues to own scheduling,
permissions and identities; matching native operation ids only correlate process
cancellation. See [Project tasks](ProjectTasks.md).

```text
CyUI / SDL3 desktop                 External MCP clients
        |                                  |
        |                           official MCP SDK (stdio)
        |                                  |
        |                        CyToolsCore authenticated HTTP
        |                        runtime, jobs, sessions, permissions
        |                                  |
        +-------- native Engine <--- private loopback command bridge
                     |
             documents / search / Git / CMake / Unreal / trusted plugins
                     |
             LanguageService router
                +-- C++ owner thread <---- LSP transport ----> clangd
                +-- C# owner thread  <---- LSP transport ----> csharp-ls/Roslyn
```

The native Engine has no UI dependency. Documents carry monotonically increasing
revisions, UTF-8 content, original BOM/newline convention and disk baseline. Saves
reject stale revisions and external modifications before atomic replacement.
Workspace paths are canonicalized and checked by path components, including symlink
resolution. New files use exclusive creation. A task cleanup never removes source files.

Documentation and signature requests use those same live document/revision guards.
LanguageInfo normalizes display-only sections and UTF-16 signature parameter offsets
into bounded UTF-8 labels. The GUI keeps one information request in flight and
discards responses for obsolete cursors; it follows the visible panel with a small
debounce. It never executes backend Markdown, HTML or documentation links.

The desktop polls document metadata, copies text on changes, indexes project paths
off the UI thread and delegates content search to ripgrep. Replacing a search cancels
its native process, and generation IDs suppress obsolete results. Dirty buffers
supersede disk text in literal searches. The current path index refresh is explicit;
filesystem watching and persistent symbol indexing are future work.

Every native text editor is owned by CyUI::CodeEditor behind native document services. The GUI
build prepares a patched copy in its build directory using prepare_texteditor.py;
the checksum-pinned downloaded source is unchanged. The small adapter adds function,
macro and reflection-attribute roles, a context tokenizer and a read-only token query
for tests. EditorLanguage supplies bounded lexical C/C++/Unreal and C# rules; the
widget retains incremental multi-line comment/string state. Preprocessor directives
and their strings/comments are tokenized separately. Built-in keywords use one blue
role; Unreal naming conventions supply types and reflection macros/metadata have
dedicated roles. Function calls/declarations are recognized lexically. These fast
rules remain available while project analysis loads or a backend is unavailable.
EditorTheme owns the shared palette for editors,
change previews and VCS views, with contrast checked against both editor grounds.

Highlighting.cpp requests full LSP semantic tokens through the existing C++/C# owner
threads. It decodes each server's negotiated legend and relative UTF-16 positions,
validates single-line/non-overlapping ranges and surrogate boundaries, and returns
UTF-16 positions plus separate Unicode glyph indexes. The same revision-checked
language.highlight operation serves GUI and MCP. Results are capped at 50,000 spans
or a 4 MiB token payload, with explicit truncation. Edits to any live document,
server restart and semantic refresh requests invalidate the backend generation.
The GUI debounces requests by 300 ms and runs at most two color queries concurrently,
prioritizing the active and visible detached editors. Closed-tab queries retire
asynchronously rather than blocking tab closure.

The widget stores semantic display colors independently from lexical glyph roles.
Its transaction version disables obsolete spans immediately, including after undo;
normal keyword/string/comment state, identifiers and completion stay intact. Colors
map actual aliases/classes, functions/methods, variables/parameters/fields, macros
and enum members; clangd's inactive-code comment spans use a readable muted role.
Unknown/dependent classifications retain lexical colors. C# uses the same adapter
with Roslyn's legend. Delta/range streaming, advanced modifier styling and broader
raw-string/preprocessor fallback coverage remain pending.

External buffer replacements are applied as editor transactions to retain undo. The LSP
transport queues writes and delivers replies on a dedicated LanguageService owner
thread. One clangd process serves both UI completion/navigation and MCP queries;
Engine document opens, changes and closes synchronize its unsaved overlays. Requests
never wait on the UI thread. A maximum of 32 outstanding requests and a 15-second
deadline bound the queue, including initialization; expired requests send LSP
$/cancelRequest. Restart/shutdown fails in-flight requests and joins the child.
The transport bounds queued writes by count and bytes; document synchronization
retries under backpressure and completes before semantic queries are sent.
Engine checks the source revision and live-document generation before accepting
semantic replies; UI completion also checks its generation and cursor. The editor
uses 32-bit Unicode codepoints; LSP uses zero-based UTF-16 positions, and ripgrep
columns are byte offsets. Navigation converts those coordinates explicitly and
decodes percent-escaped file URIs. Results outside the canonical workspace are
counted but omitted by default. With includeExternal, registered engine/compiler SDK
roots issue opaque read-only source references; they do not extend workspace write
permissions. This initial adapter launches clangd; other servers need their
own launch settings, project configuration and integration validation.

Semantic.cpp normalizes workspace/document symbols and maintains bounded rename
and formatting plans. TextEdits validates UTF-16 ranges, surrogate boundaries and
non-overlapping replacements. Plans snapshot live revisions and text; apply/undo
validate every affected buffer and its current disk baseline before changing any
buffer. Saves remain explicit. The desktop and MCP use these same operations.
SDK documents live in a separate registry, can be read only through issued handles,
and cannot be passed to document write/refactor operations. See [Semantic editing](Semantic.md).

Commands are registered once for menus, the keyboard settings and action palette.
The toolkit-independent Keymap resolves profile bindings, contexts, custom overrides
and multi-stroke chords. The UI consumes matched keys before the editor widget's
fixed shortcuts, preserving ordinary typing/caret handling. This prevents Rider's
Ctrl+Y/Ctrl+B from invoking the widget's redo or the former build shortcut.
Initial completion waits for the document's first diagnostics publication: clangd's
initialized status alone can still produce unrelated index-only completions while
its first preamble is being built.

BuildSystem owns tool discovery, CMake execution, cache compatibility checks,
profile persistence and File API inspection. UI and MCP use the same native
operations. Configure requests codemodel/toolchain replies and selects the native
compilation database or derives one from File API metadata for Visual Studio.
The active database directory initializes the shared clangd; successful reconfigure
reloads it while retaining live overlays. Local profile state is .cycode/build.json;
generated analysis databases stay in the build tree. See [Build services](Build.md).

Solution inspection accepts directories, .sln and .uproject entries and keeps the
canonical document root at the project directory. Missing/external references remain
metadata rather than new document permissions. UnrealProject discovers the associated
engine and invokes UBT using argv. Database generation reloads the same LanguageService
used by the GUI/MCP. See [Unreal integration](Unreal.md) for tested scope.

SDL3 and the ImGui OpenGL3 renderer provide native platform viewports. Each document
is a dockable window; detached editors and panels own distinct SDL/OS windows and GL
contexts. Rendering restores the main context afterwards. Closing a panel hides it;
View/sidebar actions restore it. Editor closes retain unsaved-document checks.
Independent processes own their engines, buffers, descriptors and jobs. Layout
filenames are keyed by canonical project entry; global preferences use unique temporary
files and atomic replacement. File icons use original vector drawings, semantic
palette colours and DPI-aware geometry without OS symbol-font assets.

CyToolsCore is installed as a real wheel into runtime/python. One companion per GUI
instance shares the Core scheduler across MCP connections. Each instance has unique
private native/Core descriptors. The native token protects local IPC; Core owns
assistant authentication and operation permissions. The bridge is application IPC,
not another MCP parser, scheduler or job registry. The service exits when its native
descriptor is removed. Jobs and artifacts use the SDK lifecycle.

Source document paths are workspace references, not file-import fields: importing a
copy would disconnect edits from the open editor. Temporary generated reports belong
in JobContext.path; explicit source saves remain persistent operations.

Git mutations are serialized by the native Engine. Vcs.cpp provides structured
repository metadata and guarded working-tree changes. Changed clean documents
reload through the same revision/LSP service; dirty buffers survive concurrent
operations. VcsUI consumes structured responses asynchronously and keeps conflict
editing in normal document tabs. Providers continue to use the existing C ABI.

CyUI is an external configurable checkout, not a vendored fork. Platform-specific
code includes process/IPC, Windows engine registration and the Unicode entry point;
SDL3 supplies window/input/render integration. Portability of source is not a claim
that Linux/macOS builds have run.

The shared editor library owns per-document incremental Tree-sitter parsers for
Rust/Java/JS/TS/TSX/HTML/CSS, with display spans separate from lexical and semantic
overlays. Both applications and refactor/conflict previews use this same layer.
The SDL-free core routes language requests to independent server owners for C++,
C#, Rust, Java, TypeScript/JavaScript, HTML and CSS. Initialization options and
workspace configuration are separate; the service respects advertised methods and
maps LSP workspace-edit versions to native document revisions. See
[language support](Languages.md) for runtime prerequisites and deliberate limits.

CompletionEngine.cpp holds bounded issued-item sessions tied to document revision,
workspace generation and backend epoch. Resolve receives only cached server items;
prepare turns the primary edit and additional imports into the existing ChangePlan
transaction after checking UTF-16 ranges, overlaps and disk baselines. The MCP
surface is read-only until changes_apply. The build-local editor patch intercepts
acceptance without inserting a partial symbol. The GUI prepares asynchronously,
checks cursor/revision/generation again, applies the native plan and mirrors it as
one tab-preserving undo transaction. A headless actual widget test verifies the
acceptance hook; complete desktop acceptance remains separate.

Snippet.cpp is an SDL-free bounded parser/expander linked by the core and editor.
Completion preserves field byte offsets through indentation and additional imports.
EditorSnippet converts them once to glyph coordinates and uses the widget's detailed
transaction callback to track linked fields without rescanning document text for
every keystroke. The build-local adapter adds validated multi-selections and a
pre-key handler; ordinary multicursor edits supply atomic mirror typing. Finishing
a session collapses only its recorded cursor history so undo/redo cannot revive
temporary linked cursors. The widget's text extraction also preserves exact final
newlines rather than implicitly appending one to nonempty files.

CodeActions.cpp owns separate bounded issued-action sessions and lazy resolution.
LanguageService adds overlapping published diagnostics to code-action context,
matches decoded file URI identities and maps versioned edits to native revisions.
LanguageClient invalidates diagnostic eligibility for actions on edits while
retaining the last displayed diagnostics and fast completion; versionless
publications remain advisory. New target overlays must be synchronized before a
fresh query, preventing an edit from adopting an unrelated disk baseline. Shared
workspace-edit parsing and ChangePlan validation serve rename and context actions.
GUI and MCP use those same plans; no code-action command is executed. Only the
pinned TypeScript server's verified empty acknowledgement is ignored. Resource
operations and annotated/snippet action edits remain explicitly unavailable.

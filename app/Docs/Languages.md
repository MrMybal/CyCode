# Language support

Verse files (`.verse`, including `.digest.verse`) now use CyUI's shared native
syntax mode. Keywords, primitive types, attributes, effect/access specifiers,
numbers, call names, strings/interpolations and nested/indented comments are
colored without a new dependency. Verse is not routed to the C++/clangd backend,
including inside Unreal projects. A Verse compiler, UEFN connection and a bundled
Verse language server are not supplied. The EN/FR user manual describes this scope.

Java launch/attach debugging now uses JDT LS with the separately installed Microsoft
java-debug bundle and shared DAP services. Project tasks provides explicit actual
main/runtime-classpath launch profiles, while JDWP attach uses independent saved
host/port/timeout fields and detaches existing JVMs. Manual Hot Code Replace builds
the imported JDT model then requests redefineClasses with guarded session/documents
and matched cancellation. JVM support and matching source/output mappings are
required; runtime/network/GUI acceptance remains pending. See [Java debugging](JavaDebugging.md).

CyCode and CyCodeReader share a native, incremental Tree-sitter syntax layer for
Rust, Java, JavaScript/JSX, TypeScript/TSX, HTML and CSS. HTML script/style blocks
use the JavaScript/CSS parsers. Strings, nested Rust comments, raw strings and
JavaScript template substitutions have dedicated syntax handling. Existing
C/C++/Unreal, C#, shader, JSON, Python, Lua and Markdown lexers remain available.
Syntax runs on document changes, with byte/glyph conversion for UTF-8, a 4 MiB
document limit, parser/walk deadlines and bounded display spans. Semantic overlays
take precedence over syntax while protecting strings/comments.

Language services belong to CyCode's native core; Reader hosts the shared syntax
layer, not project analysis or language servers.

Semantic coloring now negotiates full/delta transport for any connected server
advertising it, with bounded per-worker bases and a full-request fallback.
Existing Unicode/revision and display guards remain. Backend/GUI acceptance is
pending; see [semantic token updates](SemanticTokenUpdates.md).

Versioned diagnostics now feed CodeEditor squiggles and message tooltips with
native revision/UTF-16 guards and theme roles. Unversioned advisory reports remain
in Problems. See [Editor diagnostics](EditorDiagnostics.md) for provider limits,
display budgets and unexecuted runtime acceptance.

Python/Pyright and GLSL Analyzer now use the same native service router, and ABI2
plugins can add file selectors/LSP backends without a process-global registry.
Problems includes editable per-project argv/settings, session-only opt-out and
separate server logs. See [language preferences](LanguagePreferences.md).
See [Development workflows](DevelopmentWorkflows.md) for the installer, exact pins,
supported methods and the configurable but unverified HLSL backend.

| Files | Native language server | Connected operations |
|---|---|---|
| C/C++/Unreal | clangd | Completion, diagnostics, semantic colors, definitions/references, symbols, rename, formatting |
| C# | csharp-ls/Roslyn | Completion, diagnostics, semantic colors, definitions/references, symbols, rename, formatting |
| Rust | rust-analyzer | Completion, diagnostics, semantic colors, definitions/references, symbols, rename, formatting with rustfmt |
| Java | Eclipse JDT LS | Completion, diagnostics, semantic colors where advertised, definitions/references, symbols, rename, formatting |
| JavaScript/JSX, TypeScript/TSX | typescript-language-server + TypeScript | Completion, diagnostics, semantic colors where advertised, definitions/references, symbols, rename, formatting |
| HTML | VS Code HTML server | Completion, formatting, document structure and definitions where provided; no workspace symbol/reference/rename method advertised |
| CSS | VS Code CSS server | Completion, diagnostics, formatting, document structure, selector definitions/references and rename; no workspace symbol method advertised |

The Problems panel shows the availability, state and enable/restart/disable
controls for each server. The IDE starts installed additional servers when their
files are first opened, preserving saved disables. The core/CLI/MCP starts those backends only after an
explicit `language_configure` operation authorized with `process.execute`.
Inspection and read-only queries never enable them. Servers are independent child
processes with bounded queues/output and owned process-tree lifetimes. Rust and
Java wait for their workspace readiness notifications before initial requests.
Java receives separate data and OSGi configuration directories per instance.

Navigation and refactoring share the existing live-document/revision/disk guards.
Versioned LSP edits are validated against the versions actually sent and mapped to
native revisions. Java's coalesced edits retain a complete before/after preview.
Initially unopened rename targets are opened and synchronized before recomputing
the preview against their live versions. Applying a rename edits buffers; saving
remains a separate action.

Completion materializes LSP list defaults and supports text edits, insert/replace
ranges, multiline insertion/indentation and interactive snippet parameters.
Selecting an issued item resolves lazy documentation/import edits when the server
supports it. The symbol and additional imports form one revision/disk-checked
native change plan. The editor checks its cursor/generation again before applying
the plan, then records the complete result as one undo/redo transaction, preserving
tabs and converting the final UTF-16 caret to glyph coordinates. Saving is separate.
This acceptance path requires one cursor. Reader shares the widget and syntax;
it does not start project language servers.

`language_complete` returns a sessionId plus items in original index order.
`language_completion_resolve` and `language_completion_prepare` accept that issued
session/index and expectedRevision; clients cannot submit arbitrary edits through
these operations. Prepare is read-only and returns a normal change plan and caret.
Existing changes_preview/apply/undo operations inspect or apply it; apply/undo
require workspace.write. Sessions are limited to eight, five minutes and 8 MiB
combined; each list contains at most 2000 items / 4 MiB, each item at most 256 KiB.
Additional edits are limited to 256 / 1 MiB and must not overlap each other or the
primary edit. Stale document/workspace revisions, server reconfiguration, disk
conflicts and SDK writes are refused. Protected insertion/ranking fields cannot
change during resolve; indentation mode is explicitly advertised as resolvable.

Snippets support numbered tab stops, repeated linked fields, forward references,
nested defaults, explicit $0 final carets and choice lists. Tab/Shift+Tab moves
through numeric field order; Escape finishes. Typing edits all mirrors in one
widget transaction. A parameter badge shows the navigation keys; choice fields
offer a dropdown. Replacing an outer field removes its nested fields. Undo,
external document synchronization, read-only changes, reload and cursor moves
outside the linked selections terminate the session. Redo restores text without
resurrecting the temporary linked cursors. Automatic indentation is suspended
inside fields so it cannot delete snippet delimiters; the prior setting returns
on exit. Ordinary editor indentation and raw-tab undo/redo policies are preserved.
Automatic completion is suppressed while editing linked fields. Explicit Ctrl+Space
finishes that session and requests a fresh completion; nested interactive sessions
are not stacked.

The SDL-free snippet library is shared by core completion and the editor.
Prepare returns optional snippet metadata in explicit UTF-8 byte offsets within
the resulting document: fields/ranges/parents/choices and a final caret. Offsets
include import and multiline indentation changes; the ordinary cursor remains
UTF-16. The UI resolves every byte boundary in one document pass, then tracks
only affected glyph coordinates from actual widget transactions while typing.
Inputs are limited to 8 KiB, expanded text to 64 KiB, nesting to 16, fields to
128, occurrences to 256 (128 per field), indexes to 65535 and choices to 64.
CyCode substitutes fourteen document/workspace/position variables and explicit
defaults before shared numbered-field parsing. Substituted values remain literal;
GUI previews and native preparation use the same helper. Variable regex transforms
add bounded PCRE2 matching, capture/conditional formats and seven Unicode case
methods during native preparation. Their popup entries show the server label
without evaluating regex on the GUI thread. The adapter has its own
8 KiB escaped-output bound. Unknown bare variables, interactive field transforms, recursive fields
and coincident empty mirrors are refused. See [Completion variables](CompletionVariables.md).
Reader includes the shared editor correction preserving the absence/presence of
the final newline; it does not have a snippet insertion catalogue or LSP host.

The saved [project insertion catalogue](SnippetCatalogue.md) uses the same guarded
completion plans and fields without a language-server request. It is opened
explicitly from Edit / Find Action; it is not merged into automatic suggestions.
An explicit single selection can be wrapped through TM_SELECTED_TEXT, including
multiple lines, with a guarded exact replacement and literal layout. Rider
Ctrl+Alt+J opens Surround with snippet; other profiles expose the command without
a default binding. Server completions do not supply selection context.
Further variable families, interactive field transforms, commit-character acceptance, global user catalogues and
full code actions remain pending. Passive editor hints and Java selection/ranking
notifications are ignored; no completion commands are executed. Other commands
are refused. clangd retains --header-insertion=never: C++/Unreal automatic includes
are not enabled by this change. Backend-specific unsupported methods fail explicitly.

## Quick fixes and context actions

The Code actions panel requests actual fixes/refactorings for the live cursor or
selection: Rider Alt+Enter, Visual Studio Ctrl+. or Alt+Enter, VS Code Ctrl+.
It filters quick fixes, refactorings and organize imports. Available actions can
be applied to unsaved buffers or opened in the shared before/after preview, with
the existing refactoring undo. Saving remains separate. The UI source compiles;
complete desktop acceptance remains pending the protection-related GUI block.

The read-only MCP operations `language_code_actions` and
`language_code_action_prepare` return issued summaries and a native change plan.
Clients pass a live UTF-16 range/expectedRevision, then the sessionId/item index;
they cannot submit arbitrary action edits. Lazy resolution uses only advertised
`codeAction/resolve`. Sessions expire after five minutes and are bounded to eight /
8 MiB total, 256 actions / 2 MiB per list and 512 KiB per item. Revision, workspace
generation, backend epoch, target paths, overlaps and disk baselines are checked.
Literal-edit targets are opened/synchronized before recomputing the list; newly
discovered lazy targets require a fresh query. Multi-file apply validates every
affected buffer before changing any. `changes_apply/undo` require workspace.write.

Versioned context diagnostics must match the current LSP overlay. Unversioned
publications are advisory; the cache is invalidated on document edits, but the
protocol cannot establish their revision. Equivalent percent-encoded Windows
file URIs are matched by their decoded path. Servers compute edits from the live
range and all resulting plans still carry native revision/disk guards.

Actions requiring server commands, file create/delete/rename, change annotations
or snippet text edits are unavailable with a reason. No server command executes.
The pinned TypeScript server attaches a passive acknowledgement to ordinary
import fixes; only its empty-command variant is omitted after the edit. Complete
server-specific action coverage, interactive command arguments and Unreal asset /
Blueprint refactorings remain pending. Actual native checks cover clangd typo
fixes, TypeScript imports, Java import organization and Rust visibility rewrites;
other actions depend on the connected backend's advertised methods.

## Symbol documentation and parameter information

Navigate > Quick documentation and Parameter information query the real server at
the current cursor. The Code information panel displays documentation, overloads,
parameter names and the active parameter when the provider reports it. Rider uses
Ctrl+Q and Ctrl+P (Ctrl+Shift+Space is also available); Visual Studio and VS Code
use Ctrl+K, Ctrl+I and Ctrl+Shift+Space. These actions share the command catalogue,
user overrides and capability checks. Reader remains an editor without LSP services.

An open information panel follows the active document/cursor with a short debounce
and at most one request in flight. Native revision/generation checks reject stale
responses; the UI also rejects results for a previous cursor or document. Requests
do not edit buffers, save files, enable a disabled server or execute documentation.
Markdown/HTML returned despite the preferred plaintext format is displayed
literally; rich documentation rendering and mouse-hover popups remain pending.

The read-only MCP operations `language_hover` and `language_signature` use path,
expectedRevision and a zero-based UTF-16 position. Hover returns typed display
sections and an optional validated document range. Signature help returns up to 32
overloads, 128 parameters each and 64 KiB of text. Parameter startByte/endByte are
UTF-8 offsets in the returned label; -1 means an unmapped label. A missing/null
provider activeParameter is preserved as -1 rather than inventing a selection.
In particular, csharp-ls 0.28.0 omits the active index after a call's final comma;
the signature still appears but that parameter is not highlighted. HTML may
advertise signature help for embedded languages and return no signature on a tag.
Unsupported methods fail explicitly; empty results remain normal at positions
without information.

## Installation and configuration

The ordinary build installs locked language tools in `runtime/tools`. To install
them separately, run `python app/scripts/setup_languages.py`, or select `--rust`,
`--java`, `--web`. Archive SHA-256 values are pinned in
`app/language-tools.lock.json`; web package versions/integrities are pinned in
`app/web-languages/package-lock.json`. npm lifecycle scripts are disabled.
Nothing is installed globally.

- Web services require installed Node.js 20+ and npm. Syntax does not require Node.
- The pinned JDT LS 1.50.0 requires installed Java 21+; JDK 24 was tested locally.
- Rust uses an installed toolchain when Cargo is available, otherwise the private
  pinned rustc/Cargo/rust-src/rustfmt toolchain. Build scripts, procedural macros
  and check-on-save are disabled in the default analysis profile. Dependencies
  and project-specific Rust toolchains remain the project's responsibility.
- Automatic TypeScript typing acquisition is disabled. JS/TS use project
  `tsconfig.json`/`jsconfig.json` and installed project dependencies; React types
  and framework plugins are not fabricated or bundled.
- Automatic Maven/Gradle import and Java autobuild are disabled by default.
  Standalone Java files were tested; broader Java project import/build workflows
  remain future work.

`language_configure` accepts `language` (`cpp`, `csharp`, `rust`, `java`,
`typescript`, `html`, `css`), `enabled`, optional `arguments` (complete executable
argv) and `settings`. `$initializationOptions` separates initialization data from
workspace configuration. Custom argv/settings replace the default launch profile.
The legacy `executable` option remains supported; omitting `language` retains its
previous C# behavior. Configuration applies to the current IDE session.

Installed programs can also be selected with `CYCODE_NODE`, `CYCODE_JAVA`,
`CYCODE_RUST_ANALYZER`, `CYCODE_CLANGD` and the existing C# overrides.
Cargo, npm and standalone Java build/run tasks now share the native project service
and the existing shortcuts; Cargo targets also feed the native debugger. See
[Project tasks](ProjectTasks.md) and [Cargo workspaces](CargoBuilds.md) for the
workspace/features/profile/target additions and pending expanded acceptance.
The explicit Cargo-to-Rust analysis action copies supported manifest/feature/
resolution options to initialization and server configuration, with optional
local preference persistence. It does not adopt later task edits automatically
or reproduce custom compilation profiles; see [Rust analysis](RustAnalysis.md).
An installed named Rustup toolchain can now be selected for Cargo and explicitly
copied into the analysis snapshot. It uses the existing native process owners
without requesting installation; see [Rust toolchains](RustToolchains.md) for
analyzer component/custom argv behavior and pending runtime acceptance.
Java/web debug adapters, broader Maven/Gradle integration,
framework previews, remaining server-specific code actions, advanced refactoring and external
standard-library source navigation remain separate work. Linux/macOS builds and
runtime behavior remain unverified.

## Verification

CTest exercises the six additional language entry points using actual servers,
live completion, formatting, symbol structure and shutdown. TypeScript has
cross-file definition/references, rename/apply/undo and type diagnostic checks.
Rust/Java have actual definition/references, guarded rename/apply/undo and semantic
token checks. CSS checks selector rename. A delayed-server lifecycle test rejects
obsolete LSP edit versions. Missing optional runtimes are explicit CTest skips.
Native tests also cover actual documentation/signatures, live C++ changes,
C# overloads and provider selection limits, UTF-16 parameter ranges, bounded text,
unsupported methods and delayed stale documentation/signature replies. The MCP
suite verifies these operations with a read-only identity, including an emoji
before the call, surrogate-boundary rejection and stale revisions.
Native completion tests check actual TypeScript/Java/Rust imports, insertion across
the installed language servers, atomic preview/apply/undo, session eviction and
server restart, forged tickets, delayed obsolete resolves and disk conflicts.
A headless real ImGui editor test exercises Ctrl+Space/Enter, interception before
insertion, one-step undo/redo, raw tabs and a UTF-16 emoji caret without creating an
OS window. These are distinct from full desktop acceptance, which remains pending
the protection-related GUI block. The official MCP test checks read-only import
preview and denied application, followed by writer apply/undo without saving.
Snippet tests additionally verify real widget typing, mirrored Unicode fields,
Tab/Shift+Tab, choices, nested removal, multiline editing, Escape, undo/redo and
reload/read-only/external-edit termination. Exact final-newline tests cover load,
replace, undo and redo. Native fixtures combine imports with linked field offsets;
actual clangd and the official read-only MCP return two editable function arguments.
`verify_information.py` prepares hidden GUI shortcut/capture acceptance for all
three keymaps; its current execution remains pending the protection-related GUI
block recorded in [Validation](Validation.md).
Code-action tests additionally exercise actual preview/apply/undo across four
servers, lazy resolution, multi-file atomicity, forged/evicted tickets, stale
diagnostics/replies, newly discovered targets, URI aliases and disk conflicts.
The official MCP import-fix test verifies read-only preview, denied application,
writer apply/undo and unchanged disk sources.

`python app/scripts/verify_languages.py` tests actual Ctrl+Space/Enter insertion
and syntax in hidden IDE windows, then the shared Reader parsers in two themes.
`app/tests/test_mcp.py` exercises actual TypeScript completion, an unsaved type
change and permission rejection through the official MCP SDK and CyTools runtime.

Upstream references: [Tree-sitter parsing](https://tree-sitter.github.io/tree-sitter/using-parsers/2-basic-parsing.html),
[rust-analyzer](https://rust-analyzer.github.io/book/installation.html),
[JDT LS 1.50](https://github.com/eclipse-jdtls/eclipse.jdt.ls/tree/v1.50.0),
[TypeScript language server](https://github.com/typescript-language-server/typescript-language-server),
[VS Code extracted servers](https://github.com/hrsh7th/vscode-langservers-extracted).
Information protocols: [LSP hover](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/hover.md),
[LSP signature help](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/signatureHelp.md),
[csharp-ls 0.28 signature implementation](https://github.com/razzmatazz/csharp-language-server/blob/0.28.0/src/CSharpLanguageServer/Handlers/SignatureHelp.fs).
Completion protocol: [LSP 3.17 completion and resolve](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/completion.md).
Context actions: [LSP 3.17 code actions and resolve](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/language/codeAction.md).

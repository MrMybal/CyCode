# Capability coverage

| Capability | Native UI | CyTools/MCP | State |
|---|---|---|---|
| CyCodeReader companion | Independent repository/build; shared editor source module consumed by CyCode | Reader owns its separate SDK/MCP and extensions | See https://github.com/MrMybal/CyCodeReader for Reader capabilities and validation; IDE services remain separate |
| Workspace description/files | Explorer | workspace_describe, workspace_files | Implemented |
| Image previews | Shared Images/Canvas view, dockable tabs, async decode, fit/zoom/pan, transparency, reload | No binary-image MCP API; text document guards remain unchanged | See [Images](Images.md) for formats, first-frame/8-bit scope and validation |
| .sln hierarchy/configurations | Solution tree + workspace sources | workspace_describe.solution, msbuild_inspect | Nested projects, ActiveCfg/Build/Deploy mappings and missing/external references |
| MSBuild solution/project builds | MSBuild panel and existing build shortcuts | msbuild_build/rebuild/clean/restore | Real Windows C++ and SDK C# builds, selected solution targets and configuration mappings |
| MSBuild C++ analysis/artifacts | Explicit evaluation/index preparation; Problems diagnostics | msbuild_evaluate/prepare | Evaluated includes/defines/flags, clangd reload, artifact paths and source diagnostics; advanced compiler cases pending |
| Unreal projects/engine/rules | Direct .uproject open, Unreal panel | unreal_inspect | Read-only discovery; UE 5.3 exercised on Windows |
| Unreal generation/build/index | Automatic analysis preparation; Unreal/Build panels, Rider Ctrl+F9 | unreal_prepare/generate/database/build | UBT/UHT and shared clangd; advanced services pending |
| Multiple windows/instances | Native detached documents/panels; File > New window | One private Core per instance | Windows OS lifecycle and two-process isolation exercised |
| Live documents | Tabs/editor | documents_list/read/create/replace/save | Implemented |
| Revision/disk conflict checks | Save | replace/save | Implemented |
| Literal/regex project search | Search panel | search_text | Implemented; dirty regex buffers excluded |
| File lookup / actions | Ctrl+Alt+F, double Shift; ranked results and recent files | workspace_files + client filtering | Implemented |
| C++ symbol/type/member lookup | Search palette, Rider Ctrl+N / Ctrl+Alt+Shift+N / Ctrl+F12 | language_symbols, language_document_symbols | Real clangd symbols and file structure; index completeness depends on analysis state |
| Command/keymap profiles | Options > Keyboard shortcuts; Rider / Visual Studio / VS Code | No UI input operation | Implemented commands, scoped bindings/chords, overrides and conflict detection; complete IDE parity pending |
| Syntax, multicursor, undo, folding | Editor widget | Text replacements only | UI implemented; no synthetic input API |
| Multilingual completion | Shared service, issued proposals, atomic editor transaction; source compiled, headless widget acceptance passed | language_complete, language_completion_resolve/prepare, changes_* | Lazy resolution, additional imports, insert/replace, multiline snippets and linked Tab/Shift+Tab parameters/choices; variables/transforms/commit characters pending; C++ automatic includes stay disabled |
| Symbol documentation / parameter information | Code information panel; Rider Ctrl+Q / Ctrl+P and VS/VS Code equivalents; source compiled, GUI acceptance pending | language_hover, language_signature | Native live-buffer queries, overloads and validated UTF-16/UTF-8 positions; provider limits and unreported active parameters remain explicit |
| Quick fixes / context refactorings | Code actions panel; Rider Alt+Enter, VS/VS Code Ctrl+.; source compiled, GUI acceptance pending | language_code_actions, language_code_action_prepare, changes_* | Actual server edits/lazy resolution, filtered issued proposals, preview/apply/undo with atomic multi-file guards; executable commands/resource operations and complete action coverage pending |
| C++/C# semantic coloration | Immediate lexical colors plus clangd/Roslyn display overlay | language_highlight | Actual types/aliases, functions, variables, fields/parameters, macros/enums and clangd inactive branches; revision/generation guards, bounded full tokens |
| C++ diagnostics/status | Problems panel | language_status | Shared connection and published diagnostics implemented |
| C++ definitions/references | Rider Ctrl+B / Alt+F7, F12 / Shift+F12; Navigate menu | language_definition, language_references | Live buffers, history and revision checks; optional registered SDK locations |
| SDK/Unreal engine headers | Read-only editor tabs | sources_read on issued references | Registered SDK roots; arbitrary external sources and all SDK writes denied |
| C++ rename | Shift+F6; before/after preview, Apply and Undo refactoring | language_rename, changes_preview/apply/undo | Semantic multi-file live-buffer transactions; Blueprint/assets and complete refactoring suite pending |
| Project-aware formatting | Document/selection commands | language_format + changes_* | clangd uses .clang-format; normal editor formatting applies to live buffers |
| Git changes/diff/history/blame/commit | VCS panel, read-only review | vcs_* | Implemented; whole-file staging |
| Git branches/remotes/sync/stashes | VCS tabs, Rider Ctrl+K / Ctrl+Shift+K | vcs_branch_*, vcs_remote_*, vcs_fetch/pull/push, vcs_stash_* | Real Git with dirty-buffer guards, fast-forward pull and safe deletion |
| Git text conflicts | Base/ours/theirs and working editor, explicit save/stage | vcs_conflicts/resolve | Actual index stages and revision/disk/marker guards; complex/binary/delete conflicts pending |
| Toolchain detection | CMake panel | toolchains_detect | MSVC/Clang/GCC discovery; MSVC and Clang exercised locally |
| CMake profile/targets/configure/build | CMake and Build panels | build_inspect/configure/run | Implemented; saved local profile, actual File API targets and compiler guards |
| CMake flags for clangd | CMake status + automatic language service reload | configure/run prepare shared analysis settings | Native compile_commands.json or derived File API database for Visual Studio |
| Native plugin discovery/commands | Extensions panel | plugins_list/invoke | Implemented; bundled sample is executable |
| External VCS providers | Provider selector and capability controls | Optional provider routing | ABI implemented; third-party providers pending |
| Themes/language/text size/icons | Options, explorer and tabs | No UI preference operation | Graphite and five CyUI themes, vector file icons, EN/FR and persistence |
| Interactive terminal | Alt+F12, shell/session selector, copy/paste and scrollback | terminal_* | Native ConPTY/PTY + libvterm; Windows PowerShell/cmd executed; other OSes unverified |
| Run/debug/attach | Run menu, gutter breakpoints, Debugger panel, source jumps, stack/locals/watches | debug_* | Real DAP with CodeLLDB and NetCoreDbg; Windows C++/PDB and .NET 8 portable PDB executed; broader debugging coverage pending |
| C#/.NET builds | Direct project open and MSBuild | msbuild_* | SDK C# build/evaluation exercised |
| C# language services | Completion, Problems, definition/references, types/members, rename/format | language_configure/status/complete, semantic operations | Separate Roslyn server, actual project references and live overlays; advanced language workflows pending |
| Rust/Java/JS/TS/HTML/CSS syntax | Incremental shared editor and Reader parsers; JSX/TSX and HTML script/style | Text only in MCP | Implemented; Unicode-safe display spans and bounded parsing |
| Rust/Java/JS/TS/HTML/CSS language services | Completion, structure, diagnostics/navigation/refactoring according to advertised methods; per-server controls | language_configure/status/complete and shared semantic operations | Independent actual servers; see [Languages](Languages.md) for project/runtime limits and validation |
| Shader compilation/live preview | Syntax only | Not provided | Planned |
| Cargo/npm/standalone Java tasks | Project tasks panel, existing Build/Run shortcuts, process cancellation | project_inspect/configure/build/test/install/script/launch/debug_target/cancel | Real Cargo/npm/javac processes and terminal execution; Java/web debugging, Maven/Gradle and framework integration pending; see [Project tasks](ProjectTasks.md) |
| Installer/updater/marketplace | Not provided | Not provided | Planned |

Validation evidence is recorded separately; an implemented path is not evidence that
every platform or all repository configurations have been tested.

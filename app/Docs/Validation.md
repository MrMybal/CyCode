# Validation — 2026-11-03

This is the first CyCode development milestone. Windows x64 was executed locally.
Linux/macOS builds and clean-machine installation have not been executed. A core CI
matrix is configured but has not run on GitHub. The project license is GPL-3.0-only;
the full text and project copyright notice are in the root LICENSE and COPYRIGHT.

## Executed checks

- GPL licensing update: the identical complete GNU GPL v3 text is installed as
  LICENSE in both projects, with separate GPL-3.0-only project copyright notices.
  Windows builds passed; CyCode's 2 affected headless UI/image tests and all 10
  Reader working-tree tests passed without opening native windows. Reader build
  output and CMake install both contain exact LICENSE/COPYRIGHT copies. The
  CyCode ZIP contains both files and declares GPL-3.0-only in its manifest.
  The shared editor pin advances to Reader's GPL licensing commit; its editor/
  sources are unchanged by that commit.

- Release 0.2.0: Windows x64/MSVC Release compilation, all 42 CTests and all
  20 real CyToolsCore/official MCP integration tests passed with the rebuilt CLI.
  Windows executable metadata, the UI version badge, native workspace metadata,
  CLI `--version` and MCP descriptor now report 0.2.0. The allow-listed Windows
  ZIP was extracted in an isolated directory: every manifest SHA-256 was checked,
  then CLI version/workspace inspection, file listing, document reads and actual
  packaged-ripgrep search passed with a minimal Windows PATH and no private runtime.
  Gitleaks 8.30.1 source/history/dependency and ZIP scans found no secrets, without
  suppressing rules. A checksum-manifest false positive on the PluginAPI.h hash
  was removed by using separate `path` and `sha256` fields. The CyUI MIT link was
  verified against its pinned private GitHub revision; the license is also local.
  No IDE window was launched. OpenGL image display and clean-machine installation
  remain unverified. The GitHub Actions matrix requires the currently absent
  `CYCODE_READER_READ_TOKEN` secret; local tests are not a claim of passing CI.

- Image previews: Windows x64/MSVC build passed all 42 CTests, including the
  new `images` test and existing code/Git/project regressions. Real fixtures cover
  PNG alpha, progressive JPEG, lossless WebP, GIF first frame, BMP, TGA,
  composited PSD, Radiance HDR and binary PGM/PPM. Tests exercise asynchronous
  cancellation, Unicode paths, corrupt/oversized inputs, actual app image tabs,
  wheel/pan/100% controls, failed/successful reload, deferred texture retirement,
  text-command routing and explicit external-file read grants without workspace
  write access. View checks run headlessly in six themes, EN/FR and three DPI
  levels. The injected renderer verifies texture handoff/lifetimes; actual OpenGL
  display and multi-monitor image windows were not executed. Decoder-only WebP,
  Images/Canvas and the single ImGui/editor link graph were verified. Core-only
  fetch still downloads only JSON/libvterm. Gitleaks scanned 215 publishable
  source files with zero findings. See [Images](Images.md) for format scope.
- Modular CyUI migration: clean Windows x64/MSVC GUI build at the Reader and
  CyUI commits recorded in the source locks. All 41 CTests passed, including the
  actual app's headless module integration test; the independent UI-free core
  build passed all 36 CTests. The actual CyToolsCore runtime/official MCP suite
  passed all 20 tests with the newly built CLI. No native IDE window was launched.
  The UI integration exercises typing through CodeEditor into Engine, Undo/Redo,
  explicit Save all, shader/log syntax selection, diagnostics/filter identities,
  shared log severity mapping, action dispatch, Ctrl/Cmd+F/Escape and actual
  Inspector draft/cancel/commit gestures. Drawing checks cover Git review/conflict
  versions, refactoring with/without wrapping, build logs, settings, diagnostics
  and documentation in six themes, two densities, 100/150/200% DPI and EN/FR.
  Link-graph inspection confirms one ImGui library, one TextEditor backend and
  only the selected CyUI modules; core-only has no SDL/ImGui/CyUI targets.
  The initial shell lacked Windows SDK resource compiler paths; final CTests
  use the full MSVC environment. Gitleaks found zero leaks in the publishable
  source snapshot. Native screenshots/multi-monitor acceptance and Linux/macOS
  execution remain unverified for this migration. See [migration details](CyUI-Migration.md).
- Reader separation: the IDE compiles against the independent CyCodeReader editor/
  module, preserving one source for file services, syntax, themes, icons and widget
  adapters. Its core-only suite now has 36 tests (Reader documents moved out); all
  36 native and 20 official MCP regressions passed. Reader has its own build/tests,
  native/data extensions, strict encodings and CyToolsCore/MCP document service.
  See https://github.com/MrMybal/CyCodeReader for that repository's evidence and
  remaining scope. No GUI was launched and no old root executable restored.
  Gitleaks scanned 192 publishable CyCode files after extraction: zero leaks.
  Private Reader access in CI needs the CYCODE_READER_READ_TOKEN read-only secret;
  cross-platform CI/desktop acceptance is not inferred from workflow configuration.
- Code-action milestone: all 37 core-only CTests and four headless editor CTests
  passed. Actual clangd typo fixes, TypeScript imports, Java import organization
  and Rust visibility rewrites prepare/apply/undo without saving disk sources.
  Native fixtures verify lazy resolution, newly discovered targets, atomic
  multi-file changes, stale/forged/evicted tickets, backend restart, protected
  fields, disk conflicts and rejected command/resource/overlapping edits.
  Percent-encoded Windows URI aliases match diagnostics. Edits invalidate action
  diagnostic context while retaining displayed diagnostics and fast completion.
  An asynchronous fake-server check was adjusted to wait for the actual new
  diagnostic publication; older context must remain unavailable during that gap.
- All 20 official MCP/CyTools integration tests passed on the final core. The new
  TypeScript import-fix test checks actual proposals, strict output schemas,
  read-only preparation/preview, denied application, forged/stale ticket rejection
  and writer apply/undo with unchanged disk bytes.
- Code-action milestone Gitleaks scan: 224 publishable source files, about 2.21 MB,
  zero leaks. The private source snapshot includes untracked additions and excludes
  runtime/data/downloaded dependencies. Windows initially held the CLI executable
  during the MCP run; its final link succeeded after that run released the file.
- Both desktop targets compile with the Code actions panel and Rider Alt+Enter /
  Visual Studio and VS Code Ctrl+. bindings. No desktop application was launched,
  no root executable restored and no protection setting changed. Full graphical
  acceptance remains pending the existing protection issue below. The four ImGui
  tests run without an OS window; these results do not establish desktop or
  Linux/macOS acceptance. Executable commands, resource operations and full
  server-specific action coverage remain pending.
- Interactive-snippet milestone: all 32 core-only CTests and four headless editor
  CTests passed. The shared parser checks numbered/forward/nested defaults,
  linked mirrors, choices/escapes, Unicode byte spans, final carets, indentation
  remapping and bounds. Actual clangd returns two function-argument placeholders;
  the native issued-item fixture combines imports with mirrored field offsets.
  Actual ImGui typing covers Tab/Shift+Tab, choices, nested replacement, empty
  fields, Unicode, multiline edits, Escape, undo/redo, read-only changes and reload.
  Exact-text checks verify that files without a final newline keep that property
  through load, replace, undo and redo.
- All 19 official MCP/CyTools integration tests passed. The new read-only actual
  clangd test validates the snippet output schema, UTF-8 field spans after an emoji,
  unmodified live/disk source and denied application without workspace.write.
- Snippet-milestone Gitleaks scan: 221 publishable source files, about 2.14 MB,
  no leaks; private runtime/data/dependencies excluded.
- Both desktop targets compile. No GUI application was launched or root executable
  restored. Complete desktop visual acceptance remains pending the protection
  issue described below; Linux/macOS remain unverified. Snippet variables,
  transforms, a user catalogue and stacked snippet sessions remain pending.
- Completion/import milestone: 31 core-only CTests and three headless editor
  CTests passed. Actual TypeScript, Java and Rust servers produce imports;
  C++/C#/JS/TS/HTML/CSS/Rust/Java acceptance prepares/applies/undoes live edits.
  Issued-session tests cover forged/evicted/stale tickets, restart epochs,
  delayed obsolete resolve replies, protected insertion changes, disk conflicts,
  UTF-16/insert-replace ranges, indentation, overlap rejection and list bounds.
  The real ImGui widget test creates no OS window and verifies Ctrl+Space/Enter,
  native acceptance interception, one undo/redo for symbol/import, tabs and an
  emoji caret. The CMake regression initially lacked rc.exe in the invoking
  environment and passed with the official Visual Studio vcvars environment.
- All 18 official MCP/CyTools integration tests passed. New coverage reads an
  actual TypeScript auto-import preview as a read-only identity, denies apply,
  then applies/undoes as a writer and verifies unchanged source bytes on disk.
- Completion-milestone Gitleaks scan: 215 publishable source files, about
  2.09 MB, no leaks. Private runtime/data/dependencies are excluded.
- CyCode and Reader source compiled in runtime/build-vs after the shared widget
  change. No application window was opened, no root GUI executable was restored
  and no security protection setting was changed. The complete desktop accept
  flow remains unexecuted under the existing protection-related limitation;
  headless widget/native/MCP results do not establish desktop or cross-platform
  acceptance. At that milestone interactive snippets remained pending; the newer
  milestone above adds them. Commit characters and C++ automatic includes remain
  pending. See [Languages](Languages.md).
- Code information milestone: all 27 core-only CTests passed. Two new suites
  cover display-content normalization and actual clangd documentation/signatures.
  Existing Rust/Java/JS/TS/HTML/CSS tests now check documentation and supported
  signature contexts. C# checks real overloads and preserves the provider's missing
  active index. Lifecycle tests reject stale hover/signature replies; command tests
  verify Rider and Visual Studio/VS Code shortcuts. This count excludes the two
  GUI editor/syntax tests that were unchanged from the earlier milestone.
- All 17 official MCP/CyTools tests passed. The new read-only information test
  checks actual documentation, second-argument selection after a UTF-16 emoji,
  malformed surrogate positions, stale revisions and write-permission rejection.
- Final information-milestone Gitleaks scan: 210 publishable source files,
  about 2.01 MB, no leaks. The scan excludes private data/runtime/dependencies.
- The new Code information UI compiled successfully in runtime/build-vs. It has
  not been launched or visually validated because the existing executable-removal
  investigation remains unresolved. No root executable was restored and no
  protection settings were changed. verify_information.py prepares hidden tests
  for all three keymaps and preserves that distinction.

- Project tasks milestone: all 27 native CTests passed after the Windows javac
  UTF-8 diagnostic fix. Actual Cargo/npm/standalone-Java configure/build/run,
  Cargo/npm tests, Unicode paths/arguments, compiler diagnostics, unsaved-buffer
  refusal and stale-artifact clearing were exercised. A subsequent Cargo native
  acceptance also passed real DAP source breakpoints, locals, step and stop.
- All 16 official MCP/CyTools integration tests passed. The new project test
  checks read-only inspection, process-permission rejection, actual npm build/
  test/terminal run, SDK job cancellation and explicit project cancellation.
  A spawned descendant must not survive; the next exclusive task must complete.
- Gitleaks 8.30.1 scanned a private snapshot of 205 publishable source files
  (about 1.95 MB), including untracked source additions: no leaks found.
- Current project-panel GUI acceptance is **incomplete**. A minimal hidden IDE
  session produced a report, but the Cargo shortcut scenario exited with code 0
  before its report and the root CyCode.exe disappeared. Kaspersky is installed;
  a new 19,826,534-byte quarantine entry at 12:06 corresponds in time/size to the
  19,825,152-byte executable plus metadata, but the entry's protected contents
  could not be read. The detection name has been requested from the user. Further
  GUI launches/restoration stopped; neither protection settings nor exclusions
  were changed. The six-theme project-panel and GUI Cargo-debug scenarios in
  verify_project_tasks.py are implemented but are not reported as passed.

- Multilingual milestone: all 24 native CTests passed, including actual Rust,
  Java, JavaScript, TypeScript, HTML and CSS servers. Acceptance checks live-buffer
  completion, formatting, document symbols and shutdown; TypeScript adds type
  diagnostics, cross-file definition/references and rename/apply/undo across three
  files, including an initially unopened target. Rust/Java add definition/reference,
  rename/apply/undo and semantic-token checks; CSS adds selector rename. Lifecycle
  fault tests reject stale LSP edit versions and invalid readiness configuration.
  Java also passed from a fresh per-instance configuration without OSGi caches.
- All 15 official MCP/CyTools integration tests passed. The web test rejects
  unauthorized server activation, then validates TypeScript completion and an
  unsaved string-to-number type change through the official MCP SDK.
- Hidden GUI acceptance passed Ctrl+Space/Enter insertion for all six additional
  language entry points, with source files unchanged on disk. Reader parsed eight
  documents (including JSX/TSX and HTML script/style), in graphite and light-grey.
  Seven resulting language/Reader captures were visually inspected. Shared C++/C#
  semantic captures and the Reader's six-theme/edit/save/detach/two-instance suite
  also passed. No visible application windows were opened for these checks.
  See [Languages](Languages.md) for runtime prerequisites and remaining workflows.

- CyCodeReader uses the actual shared editor library. Native document acceptance
  exercises Unicode paths, tabs, BOM, mixed/no-op/CRLF/CR line endings, Save As,
  exclusive creation, overwrite-baseline rejection, external edits/deletions,
  binary/invalid-UTF-8/oversize refusal and temporary-file cleanup. Hidden desktop
  acceptance exercises real editing undo/redo, Ctrl+S/F/N, Save As, closing and
  overwrite confirmations, native detachment and two independent processes. The
  OS hidden flags are asserted, including the detached native window. All six theme
  captures were visually inspected. Scripted file selections use the production
  result handler; interactive native file dialogs are not claimed as automated
  acceptance. The current combined build passes all 24 native CTest suites.
  See [Reader](https://github.com/MrMybal/CyCodeReader).

- Semantic coloration uses actual clangd and Roslyn results through shared Engine
  documents and negotiated LSP legends. Native acceptance checks aliases, methods,
  macros, fields/parameters/enums, inactive branches, Unicode UTF-16/glyph offsets,
  unsaved overlays, revision rejection, header-change invalidation and server restart.
  Fault tests reject malformed tokens/overlaps/surrogate splits and late responses
  after an edit; missing capabilities retain lexical fallback. Widget tests verify
  display-only spans, preserved lexical identifiers/comments, immediate invalidation
  on edits and undo, and atomic rejection of invalid ranges. Native buffer updates
  preserve literal tabs even when keyboard input uses spaces; undo/redo preserve
  those tabs as well. All fifteen current native CTest suites pass.
  The official read-only MCP test passes typed outputs, stale revisions, live overlay
  changes, input validation and refusal to activate disabled C# analysis. All fourteen
  MCP integration suites pass. Hidden desktop captures exercise the complete GUI/
  Engine/clangd pipeline in Graphite EN/light-grey FR and actual cross-project Roslyn
  colors. The OS hidden flag is asserted; no visible IDE windows are launched.
  The existing owned Unreal 5.3 fixture also returns real class/macro/property/type
  spans from its cached generated-header/compilation database, without rebuilding or
  changing source. Broader Unreal versions/frameworks and Linux/macOS remain unverified.

- Editor coloration uses the actual patched TextEditor colorizer, with no IDE
  window required. Tests check split #include paths, directive/macro/attribute/type/
  function roles on Unreal declarations and calls, neutral parameters, numbers,
  comments/strings, Unicode names, incremental replacement, C/C# routing and syntax
  contrast of at least 4.5:1 against both editor grounds in all six themes.
  A 10,000-line Unreal header colors in 173 ms in the Release CTest run on this
  machine (one fixture measurement, not a whole-project performance claim).
  Six real widget captures were generated in one hidden SDL/OpenGL window without
  launching the IDE, and all six themes were visually inspected. All fourteen
  native CTest suites pass after the editor adapter change.

- Managed DAP acceptance uses pinned NetCoreDbg 3.2.0-1092 and a real offline-built
  .NET 8 console assembly with portable PDBs. Automatic managed detection, native
  Windows PDB paths, conditional breakpoints and deferred verification, stack,
  locals, object/array children and paging, evaluation/watches, step into/out,
  continue/output/exit, Unicode quoted arguments and accented environment values,
  pause across consecutive sessions, interactive terminal run/input and attach/detach
  preserving the target are exercised. Unsupported hit-count options preserve the
  saved breakpoint list. Inspection identifiers clear on resume/stop/relaunch.
  All thirteen native CTest suites pass. The official MCP managed test also passes
  process permission checks, typed profile/status, actual stopped-frame evaluation,
  collection paging and Unicode output through the shared service.
  All thirteen official MCP integration suites pass, including the existing native
  debugger, C#/C++ language, MSBuild, Git, terminal and Core lifecycle checks.
  Six desktop scenarios pass: actual source stops, locals/watches, verified markers,
  Rider F8/F9/Ctrl+F2 and terminal Run. Graphite EN and light-grey FR captures were
  inspected. See [Execution](Execution.md) for the .NET 8 scope and adapter limits;
  managed debug stdin, other Windows output code pages, other frameworks and other
  operating systems are not validated.
  Regression checks also pass all six existing C++ debugger desktop scenarios and
  seven C# language/rename scenarios. Scripted-key tests ignore physical keyboard
  text and can wait for real command availability, preventing input contamination
  and stepping before the adapter is ready. Repeated GUI launches were stopped at
  the user's request; keep one interactive window open for subsequent work and
  prefer CLI checks instead of rerunning the full graphical suite during use.

- Native C# acceptance uses the pinned Roslyn/csharp-ls 0.28.0 server and private
  .NET SDK 10.0.403 against an offline-restored .NET 8 solution with two project
  references. Actual definitions/references, types/members, cross-project semantic
  rename/apply/undo, completion from an unsaved referenced document, CS0029 live
  diagnostics, Roslyn formatting, disable/restart with unsaved buffer replay and
  C++ navigation in the same Engine are exercised. Original source files remain
  unchanged on disk. Initial C# loading requires explicit activation in native
  CLI/MCP mode. See [C#](CSharp.md) for prerequisites and remaining scope.
  The official MCP test checks process permissions, typed per-server status,
  cross-project definitions/references and completion from unsaved referenced
  documents. Seven desktop C# scenarios exercise
  definition/reference shortcuts, filtering and insertion of actual completion
  items, dirty-buffer propagation, Roslyn formatting and readable rename previews
  in Graphite EN and light-grey FR. The captures were visually inspected.
  Native CTest coverage includes C# and the existing MSBuild,
  C++/Unreal, terminal, debugger, VCS and document/language lifecycle checks.

- Native MSBuild acceptance: actual two-project solution, selected nested/dotted
  targets, C++ execution, solution Debug-to-project Release mappings, clean/rebuild,
  evaluated artifact paths and clangd navigation using project includes/definitions.
  Changed solution inputs invalidate the analysis cache. Real compiler errors are
  parsed into source diagnostics, including Windows short-directory aliases.
  A .NET 8 C# project is restored, built, evaluated and executed using installed packs
  and no NuGet network sources. The official MCP test checks process permissions,
  typed outputs, managed clean/rebuild and unchanged source files.
  Ten native GUI scenarios exercise solution build shortcuts, index preparation,
  C++ header navigation, evaluated artifacts, repeated evaluation, saving live buffers
  before compilation, clean/rebuild, source diagnostics,
  Graphite/light French rendering and direct SDK C# project builds.

- Native Git acceptance: actual branches, safe rename/deletion, clean-buffer reload,
  revision/dirty-buffer guards, stashes, local bare-remote push/fetch/fast-forward pull,
  tracking divergence, structured history/blame and a real conflicting merge with
  base/ours/theirs and guarded resolution. The official MCP adapter exercises these
  services with read/write permission checks and Unicode workspace paths.
  Seven GUI scenarios cover commit/push shortcuts, branches, history, remotes,
  stashes and read-only text conflicts rendered in Graphite EN and light-grey FR.

- Native terminal acceptance: actual ConPTY PowerShell/cmd, interactive stdin/stdout,
  Unicode/wide cells, ANSI foregrounds, resize, bounded scrollback, independent
  sessions, natural exit codes and bounded teardown. GUI tests type actual commands,
  verify computed output and shell/editor key isolation, and render dark/light EN/FR.
- Native DAP acceptance with bundled CodeLLDB: real MSVC/PDB source breakpoint,
  locals and evaluation, step in/out, continue, program output through a native
  reverse-request terminal, exit, pause/stop and attach/detach preserving the attached
  target. Profile/breakpoint metadata persists only in owned validation workspaces.
  GUI scenarios use Rider keys for debug, source navigation, watches, F8, F9, Ctrl+F2
  and run without debugging. Dark/light EN/FR captures were inspected.
- CTest semantic: real clangd type/member search, cross-file semantic rename across
  three files, preservation of shadowed identifiers/comments, project .clang-format,
  Unicode offsets, overlapping edit rejection, all-or-none revision/disk conflicts,
  and undo both before and after explicit save. Registered SDK headers open through
  issued read-only references; arbitrary external sources and SDK writes are refused.
- Desktop semantic acceptance: real Ctrl+N/Ctrl+F12 navigation, Shift+F6 preview,
  Apply/Undo refactoring through Find Action, preservation of the active editor,
  Ctrl+Alt+L formatting and ordinary editor undo. Preview captures cover all six
  themes plus French; changed-line backgrounds are subtle, long lines wrap and
  syntax contrast is adjusted. All source fixture files remain unchanged on disk.
- Actual Unreal SDK navigation: the owned UE 5.3 project opens AActor's Actor.h
  definition under Engine/Source as a read-only editor tab, without changing project
  source. Test completion now waits for the requested asynchronous navigation.
- Command/keymap native tests: Rider, Visual Studio and VS Code presets, context
  isolation, multi-stroke chords, timeout/Escape, custom overrides, conflicting
  binding rejection and file-result ranking. Real desktop key injection verifies
  Rider duplication/deletion/undo, VS/VS Code comment/uncomment chords, current-file
  versus global search, file lookup/opening, double Shift (including modifier rejection),
  action execution, keyboard settings, document close/reopen and go-to-line/history.
  The twelve desktop scenarios use the minimal Windows system PATH;
  disk source files and saved preferences remain unchanged.
- Original CyTrace_Plugin.sln rendered without --open and without ripgrep in PATH:
  actual sources populate the explorer, no empty-tab assertion. ripgrep is resolved
  from the checksum-pinned managed runtime. Default/migrated text scale is 1.15.
- Fresh Unreal GUI acceptance: a new project with no prior Intermediate tree prepares
  its UHT-generated reflection header and managed compilation database automatically.
  The actual editor popup contains GetActorLocation and other AActor methods. Accepting
  the USTRUCT's Value completion changes the live buffer correctly while leaving the
  source file on disk unchanged. The first completion waits for the initial parse,
  avoiding clangd's unrelated early index-only results.
  Reopening reuses the managed cache; removing an owned generated header invalidates
  it and UHT recreates the header, including when its own timestamps were current.

- Native Release build with MSVC 2022, C++20, SDL3 and the external CyUI checkout.
- OpenGL3/SDL3 native viewports: real detached editor and panel windows, independent
  OS IDs, move/resize, native close, View restore and docking back to one OS window.
  Two simultaneous GUI instances have separate Core endpoints/live buffers; closing
  the first leaves the second and its service operational. Preferences remain unchanged.
- CTest solution_unreal: nested folders/configurations, missing/external references,
  UTF-8/BOM/UTF-16, malformed/cyclic solutions and workspace confinement. Unreal
  target/module/plugin discovery, invalid engine/target/platform/configuration guards.
- Actual Unreal 5.3: generate a .sln with UBT, compile an Editor module to its DLL,
  run UnrealHeaderTool for USTRUCT/UPROPERTY, generate compile_commands.json and query
  a USTRUCT member definition through shared clangd. Uses an owned data/validation
  fixture; the user's CyTrace_Plugin .sln/.uproject is inspected/rendered read-only.
- CTest cmake_project: detect installed tools; configure, build a selected target
  and execute its artifact with MSVC; configure/build Clang with Ninja and use its
  native compilation database. Inspect real File API targets/compiler information,
  derive Visual Studio analysis commands, and resolve a symbol through clangd that
  requires project include paths (including spaces), generated headers and macros.
  Reconfigure changed macros at the same database path and verify clangd reloads.
  Check saved profiles, unknown tools/targets, path/generator guards, configure
  failure retention, generated-tree search exclusion and unchanged user .clangd.
- CTest core: real file edits, stale/reopened revisions, disk-change rejection,
  UTF-8/BOM/CRLF/CR preservation, Unicode exclusive file creation, traversal rejection,
  ignore rules, unsaved-buffer search, Git staging/unstaging, nested repository paths,
  actual child-process cancellation and a dynamically loaded plugin.
- CTest LSP: installed clangd through the shared native Engine; cross-file member
  and function definitions, references on an unsaved overlay, declaration exclusion,
  result limits, completion, Unicode file URI/UTF-16 positions, surrogate/bounds
  validation, stale revision rejection and shutdown.
- CTest language_lifecycle: a controlled delayed LSP child, edit during an in-flight
  query, reject the obsolete reply, then stop the server during another blocked query.
  A burst of 2000 document overlays verifies that transport backpressure retries
  synchronization before querying instead of dropping notifications.
- Python integration tests in CyCode's private runtime: live native documents
  and write permissions, official MCP SDK roundtrip/invalid input, and two MCP
  roles connected to one shared Core with private job visibility between roles;
  read-only semantic MCP operations on live unsaved text, revision/schema/permission
  rejection and an unchanged disk fixture.
  Terminal sessions and real CMake-built C++ debugging, locals and evaluation are
  exercised over MCP, including process permissions and explicit session cleanup.
  Semantic rename previews and formatting apply/undo are exercised over
  the official MCP client; the reader cannot apply changes while the writer can,
  and arbitrary external SDK source requests are rejected.
  CMake/toolchain jobs run over the official MCP client, including configure/build,
  typed project outputs, profile inspection and refusal of unprivileged configuration.
  Unreal operations are discovered through MCP; read-only project inspection passes
  while generation/index/build require process.execute and invalid configurations
  are rejected by the public input schema.
- Desktop acceptance: launch the actual GUI from another cwd, let it start its own
  shared CyToolsCore service, connect the official MCP client, edit its visible live
  buffer without saving, invoke the native plugin, capture the modified editor and
  shut down the GUI/Core. The original fixture file remains unchanged.
  Definition/reference jobs also query that GUI's shared clangd after the live edit.
- CMake example: configure, build and execute the supplied trusted example through
  native IDE operations. Result: Hello, CyCode! No timeout or process failure.
- Render Graphite, all five CyUI themes, French workspace and French Options. Inspected
  captures for contrast, readable syntax, docking and translated controls. Original
  folder/file-type icons appear in the explorer, document tabs and quick file lookup;
  Unreal captures check the icons and controls in dark/light themes.
- Desktop navigation acceptance: actual GUI/shared clangd in an owned project,
  definition jump after a supplementary Unicode character (both UTF-16 and codepoint
  cursor checked), References results in French navy and English light themes,
  screenshots and clean shutdown. The fixture remains unchanged.
- Desktop CMake acceptance: configure/build a real owned project under a Unicode
  workspace path with spaces in the build directory, reopen its profile in the GUI,
  verify detected tools/target/database state and render the CMake panel in all five
  themes with EN/FR. The source fixture remains unchanged. CLI Windows arguments
  use the Unicode command line.

Artifacts are under data/reports: theme BMPs, desktop-mcp-live.bmp,
navigation-*.bmp/json, cmake-*.bmp/json, keys-*.json, keymap-settings.bmp,
unreal-completion-*.bmp/json, minimal-path-solution.bmp, build-example.json,
benchmark.json and test output. Download/source/build caches
remain under runtime/ and are excluded from source control.

## Search baseline

Corpus: 10,000 synthetic C++ files, 10,390,027 bytes; five runs per operation on this
Windows machine. Measurements include native executable startup and JSON output.

| Operation | Median | Maximum |
|---|---:|---:|
| Initial file listing/index response | 561 ms | 570 ms |
| Literal search, one target | 152 ms | 155 ms |

These are local baseline measurements, not a guarantee for other projects or disks.
The GUI retains its path list in memory and performs queries off its UI thread.
Document metadata snapshots cache dirty state rather than comparing full buffers
every frame. File rows are clipped to the visible portion of the explorer.

## Explicit limits

- This is not Rider feature parity. The native terminal and DAP C++/.NET debuggers are
  implemented; complete refactoring coverage, test discovery, shader/web previews,
  broader debugger tooling and a marketplace are still pending. Advanced Git branches,
  sync, stashes and ordinary text-conflict resolution are implemented; hunk staging,
  interactive rebase and complex/binary/deletion conflict UI remain pending.
- CMake, MSBuild and UnrealBuildTool are implemented build adapters. Blueprint/asset
  editing, Live Coding and actual UnrealEditor debugging remain pending.
  Unreal 5.3 was exercised on Windows; other versions/platforms remain unverified.
  Other VCS systems have an extension ABI,
  not bundled working providers.
- Local toolchain validation covers MSVC x64/Visual Studio and Clang/Ninja on Windows.
  GCC discovery is implemented but GCC was not installed or executed here. MSVC/Ninja,
  CMake Presets, custom toolchain files, cross compilation, compiler wrappers and
  advanced module/PCH/unity/framework workflows remain unverified or pending.
  File API analysis commands are derived metadata, not a replacement build system.
- UTF-8 editor files are limited to 4 MiB. Shared Core HTTP requests have the SDK's
  1 MiB body limit. Mixed newlines are retained on no-op saves and normalized to the
  detected convention on edits. Legacy text encodings are rejected rather than converted.
- Dirty buffers override disk results for literal search. Regex searches exclude
  dirty buffers and report that limitation. Case folding in dirty literal buffers is
  currently ASCII-oriented. Search previews/row counts and process output are bounded.
- Initial path listing is capped at 20,000 files and refreshed manually; persistent
  indexing and filesystem watchers are future work.
- clangd requires correct compilation flags/compile database for real project analysis.
  Reference results depend on its available index and may be incomplete while it builds.
  Definitions/references omit external locations by default; registered SDK sources
  can be requested and opened read-only through issued references. Other external
  sources remain refused. The current C++ rename does not update Blueprint assets,
  serialized Unreal names or every macro/refactoring case. Complete refactoring
  coverage remains pending.
  Semantic positions/ranges are zero-based UTF-16; text-search columns remain one-based
  UTF-8 byte offsets. Published diagnostics are asynchronous and can lag behind typing.
  Semantic requests are capped at 32 outstanding per backend, with a 15-second
  C++ deadline or 60 seconds for C# including project loading, and LSP
  cancellation on expiry. There is no immediate user cancellation/streaming control yet.
- Core cancellation retains reservations until native operations stop. Cargo/npm/
  standalone-Java tasks terminate their owned native process trees; streaming and
  equivalent immediate controls for CMake/MSBuild/Unreal remain pending.
- Native CMake/Unreal process deadlines are 180 seconds with 8 MiB output. Large
  first Unreal builds may require a longer configurable deadline, still pending.
- Native multi-window behaviour was exercised on Windows. Linux/macOS GUI execution
  remains unverified. ImGui platform viewports have window-manager limitations on
  Linux, particularly Wayland window positioning; no Wayland parity is claimed.
- The default shared MCP configurations represent one read identity and one editing
  identity per GUI instance. Reusing a role shares its job visibility.
- Native plugins are trusted platform-specific binaries. No process sandbox or hot reload.
- Dependencies/tools are development prerequisites. There is no tested public installer,
  updater, signing workflow or clean-machine runtime package.

## Reproduce

Build with app/scripts/build.py. Set CYCODE_TEST_CLI to the absolute built CLI and
run private Python -m unittest discover -s app/tests -p test_mcp.py -v.
Run private Python app/scripts/verify_desktop.py, Python app/scripts/verify_gui.py,
Python app/scripts/verify_navigation.py and app/scripts/verify_build.py --cli ABSOLUTE_CLI. Run benchmark.py with the same
--cli argument to regenerate measurements. Tests use owned temporary workspaces;
they do not edit the user's existing projects.
Run app/scripts/verify_cmake.py --cli ABSOLUTE_CLI for toolchain/profile GUI acceptance.
Run private Python app/scripts/verify_windows.py for two-instance/window lifecycle
acceptance, and app/scripts/verify_unreal.py --engine ABSOLUTE_ENGINE_ROOT for actual
UBT/UHT/clangd acceptance. See [Unreal integration](Unreal.md).
Run app/scripts/verify_semantic.py for semantic editing and six-theme preview renders.
After the owned Unreal completion fixture exists, verify_unreal_navigation.py checks
actual engine-header navigation. Navigation/window acceptance fixtures stay under
data/validation so they also run within the managed workspace permissions.
Run verify_terminal.py and verify_debugger.py for actual execution/debugging GUI
acceptance. See [Execution](Execution.md) for bounds, lifecycle and remaining scope.
Run verify_managed_debugger.py for the .NET 8 console DAP and PTY acceptance.
Run app/scripts/verify_semantic_highlights.py for C++ and cross-project C# semantic
color acceptance in hidden desktop windows. It checks the actual OS hidden flag,
live source text (including tabs), unchanged disk sources and backend token results;
captures are written to data/reports/semantic-highlights. --managed-only limits this
check to C#. CYCODE_TEST_GUI can select a built executable. This suite does not open
visible IDE windows.

Run verify_vcs.py for Git panel/shortcut/history/stash/conflict GUI checks.
Run verify_msbuild.py on Windows for solution/project build, C++ index, diagnostic
and .NET GUI acceptance. See [MSBuild](MSBuild.md) for bounds and remaining coverage.

Latest C# milestone regression checks: C++ desktop rename/apply and format/undo,
actual Unreal AActor read-only navigation and native detached/two-instance lifecycle
passed after the completion transaction fix. The current root executable also opened
CyTrace_Plugin.sln read-only: 57 projects, 20 configurations and four existing missing
project references; no build or project-source modification was performed.

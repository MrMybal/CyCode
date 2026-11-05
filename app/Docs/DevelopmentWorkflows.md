# Development workflows and language extensions

The native Engine owns these operations; the desktop, CLI, CyToolsCore MCP and
permission-checked plugin host call the same services. No additional CyUI module
or second editor implementation is introduced.

## Configured CTest projects

View > Tests and Build > Discover CTest tests inspect a configured build directory
using the actual CTest `--show-only=json-v1` model. The build directory and
configuration come from the CMake panel. Discovery executes trusted project test
definitions but does not compile or execute the listed tests.

The panel provides a name filter, labels, multi-selection, per-test results,
durations and expandable output. Run selected tests and Cancel tests are also
commands available to the palette and custom key bindings. The panel docks with
the build tools and can become a separate native window like other panels.

`tests.discover` issues one current catalogue. `tests.run` accepts its ID and
zero-based selected indices, verifies the current CTest definitions and refuses
unsaved source buffers. CTest runs selected tests serially using native numeric
selection, without shell interpolation. CTest fixture dependencies may execute
additional setup/cleanup tests. Disabled entries are reported separately.

Limits: 5,000 discovered tests / 4 MiB JSON; 1..128 selected tests; per-test timeout
1..120 seconds (60 by default), 180 seconds for the native run including discovery;
64 KiB output per test and 2 MiB combined. Cancellation terminates the owned process
tree. Output limits/timeouts/cancellation do not count as success, and entries
that were not executed remain `not-run`. Source files and test executables remain
project-owned; no task cleanup deletes them. Normal CTest logs remain in the build.

MCP operations `tests_discover`, `tests_run`, `tests_cancel` require
`process.execute`. Core retains its job reservation until native execution stops.
The private bridge correlates cancellation with a unique operation ID, preventing
a queued cancelled job from stopping another actor's run. Native plugins can
request the same services with their declared `process.execute` permission.

This is a CTest integration. Discovery for .NET test frameworks, Unreal Automation,
Maven/Gradle and framework-specific debugging/coverage remains separate work.

## Partial Git staging

Version control > Hunks offers working-tree and staged previews for the active
file. The Diff button's context menu also opens either preview. Select one or
more blocks and choose Stage selected hunks or Unstage selected hunks. Rendering
uses the shared CyUI Diff model and theme palette.

`vcs.hunks` / MCP `vcs_hunks` issues a read-only preview containing numbered blocks.
`vcs.applyHunks` / `vcs_apply_hunks` accepts only a preview ID and issued indices;
it never accepts a client-supplied patch. Apply requires `vcs.write`. Both operations
require selecting the actual repository root. File paths remain workspace-relative
for nested repositories; previews are bound to that repository's identity.
Dirty/conflicted buffers, unresolved merge conflicts,
changed selected-file index contents/diff, unknown/duplicate indices and expired/consumed plans
are refused. Git owns the atomic index lock and validates the patch's old text.
Working files are not rewritten. Temporary host-created patches are removed.

Unrelated staged files are preserved and do not invalidate the preview. The host
queries only the selected index entry instead of listing an entire large repository.
The newest 16 plans live for five minutes; each diff/index listing is limited to
2 MiB and 1,000 blocks. Added/deleted, binary and mode-changing files use whole-file
staging. Arbitrary line selection, complex conflict handling, interactive rebase
and production third-party VCS acceptance remain work items. Nested roots and
linked-worktree controls are implemented with new native/GUI acceptance pending;
see [Git worktrees](GitWorktrees.md).

## Search live buffers

Global literal and regex search both prefer unsaved text over disk contents.
Regex matching uses the same native ripgrep engine through private temporary
numeric snapshots, with cancellation and cleanup. Original filenames/paths are
mapped back into the results; project files are never saved by search.

Unsaved regex snapshots are limited to 128 buffers / 64 MiB. Exceeding that bound
reports an error rather than silently excluding edits. Search output/match limits
remain explicit. Literal case folding is still ASCII-oriented; regex case handling
uses ripgrep. Columns remain one-based UTF-8 byte offsets.

## Python and shaders

Install optional servers without global installation or package lifecycle scripts:

```sh
python app/scripts/setup_languages.py --python --glsl
```

The exact archives and SHA-256 values are in `language-tools.lock.json`:
Pyright 1.1.411 (Node.js required) and GLSL Analyzer 1.7.1. The installer retains
upstream package notices. GLSL archives are pinned for Windows/Linux/macOS x64 and
ARM64; only Windows x64 execution has been tested.

Python `.py/.pyi/.pyw` uses LSP language ID `python`. GLSL
`.glsl/.vert/.frag/.geom/.tesc/.tese/.comp` uses `glsl`. Python provides the actual
Pyright completion/navigation/rename/diagnostics surface. GLSL Analyzer advertises
completion, hover, definitions and formatting, but no document/workspace symbols,
references, rename or semantic tokens; unavailable commands are disabled/rejected.

HLSL `.hlsl/.hlsli/.fx/.fxh` now has a separate configurable `hlsl` backend, rather
than being routed through ordinary C++. Its external server is not installed or
runtime-validated here. Separate typed DXC/glslang compilation is now implemented
through ProjectTasks; its actual compiler/GUI acceptance is pending. GPU preview
remains separate work; see [Shader compilation](ShaderBuilds.md).

Problems > Configure language server accepts a full argv JSON array and settings
object, with local per-workspace persistence and session-only opt-out. See
[Language preferences](LanguagePreferences.md) for storage, revision guards,
activation, reset/forget and shared SDK/plugin contracts. It does not execute a shell.
Explicit empty arguments/settings restore the installed defaults; ordinary
enable/disable/restart preserves the current argv and settings. An installed additional
server starts on first file opening in the trusted desktop workspace unless its
saved preference disables it. Core/MCP
inspection never enables an additional server; use `language_configure` with
`process.execute`. Stderr traces appear under Server log, independently from
explicit protocol/initialization failures.

Upstream sources: [Pyright](https://github.com/microsoft/pyright/tree/1.1.411),
[GLSL Analyzer](https://github.com/nolanderc/glsl_analyzer/tree/v1.7.1).

## ABI2 language-server contributions

A trusted native plugin with `process.execute` may call `host->contribute` during
initialization with kind `languageServer`, this UTF-8 JSON descriptor, and a null
callback:

```json
{
  "id": "vendor.dialect.analysis",
  "title": "C++ dialect analysis",
  "arguments": ["clangd", "--background-index", "--header-insertion=never"],
  "settings": {},
  "languages": [
    {"id": "cpp", "extensions": [".dialect"], "filenames": ["Dialectfile"]}
  ]
}
```

The contribution ID uses the plugin namespace; selectors cannot overlap built-in
backends or another plugin. Extension/name matching folds ASCII case. LSP language
IDs are separate from the plugin's backend ID. The application must supply valid
project analysis flags (for example `-xc++` in compile_commands.json for a dialect).
Lexical fallback colors still come from the shared catalogue or a separate
`syntax` contribution; registering a language server does not install a grammar.
`${pluginDir}` and `${workspace}` tokens expand in argv values only, without a
shell. The native host owns processes, transport, queues, live-buffer versions,
completion sessions, diagnostics, semantic overlays and guarded edit transactions.

Limits: 32 plugin servers, 128 KiB per descriptor, 1..128 argv values of at most
8,192 bytes, settings at most 64 KiB; 1..32 language selectors with at most 128
extensions/names each. Registration cannot have a callback or contextual `when`.
No process is launched by registering/inspecting the contribution. Enable through
the common controls or `language_configure` using its namespaced backend ID.

Disabling the plugin removes routing, stops/joins its server owners and invalidates
completion epochs. Reactivation creates a disabled owner and resynchronizes open
documents. Unchanged definitions retain their owners. Each workspace has its own
registry; no process-global extension map is modified. The binary C ABI2 layout is
unchanged and no STL/ImGui object crosses its boundary.

Plugins also have read-only host requests for `language.status`, `language.complete`,
`language.hover`, `language.signature`, `language.definition`, `language.references`,
`language.symbols`, `language.documentSymbols`, `language.highlight`.
`language.configure` requires `process.execute`. Existing document/resource/edit
permissions continue to apply. These additions do not provide plugin DAP
registration, arbitrary GPU/editor widgets, an installer or a marketplace.

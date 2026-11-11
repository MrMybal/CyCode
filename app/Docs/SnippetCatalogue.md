# Project insertion snippets

Enabled ABI2 plugins can now register inline catalogues alongside the saved
project file. Both use the same grammar, scopes, native issued sessions and edit
transactions. The optional [personal catalogue](UserSnippets.md) sits between project
and plugin entries and is shared by projects of this portable installation. See [Plugin catalogues](#plugin-catalogues)
for registration, ordering, provenance and activation guards.

Save a UTF-8 JSON catalogue at `.cycode/snippets.code-snippets` under the workspace
root. This is persistent project content; CyCode does not create, download or
execute it when inspecting snippets. **Edit > Edit project snippets...** opens it
directly. On first use that explicit command creates the file and initializes an
empty JSON object in its unsaved buffer. Existing files/buffers are preserved.
Edit the definitions and save before requesting the insertion list again.
There is no recursive discovery; personal snippets use a separate fixed installation-owned store.

```json
{
  "C++ class": {
    "prefix": ["class", "cls"],
    "scope": "cpp,c",
    "description": "Class named after its source file",
    "body": ["class ${1:$TM_FILENAME_BASE}", "{", "\t$0", "};"]
  },
  "Verse greeting": {
    "prefix": "greet",
    "scope": "verse",
    "body": "Print(\"${1:Hello}\")$0"
  },
  "Task comment": {
    "prefix": "todo",
    "body": "// TODO: ${1:description}$0"
  }
}
```

With a saved workspace document active, choose **Edit > Insert snippet...** or
find the command in Find Action. Rider uses Ctrl+J; Visual Studio uses
Ctrl+K, Ctrl+X. The VS Code profile exposes the command without a default shortcut.
The shared CyUI Commands palette searches names, prefixes and descriptions;
use the arrow keys/Enter to choose or Escape to dismiss it. Loading, errors,
empty results and catalogue limits are displayed as disabled rows.

This is an explicit insertion list, separate from automatic language-server
suggestions. It also works for files without a language server, including Verse.
One writable saved workspace document and one caret are required. A single
selection can be wrapped as described below. Read-only SDK/debug sources and
untitled documents are unsupported. If the caret, selection or document changes,
request again.

Without a selection, the selected prefix replaces only a matching word prefix immediately before the
caret, using case-sensitive matching. Otherwise insertion starts at the caret.
Text after the caret is retained. For example, requesting `for` after `fo` replaces
`fo`; selecting it after an unrelated word inserts the body without deleting that
word. A selection is never silently discarded. The existing native indentation,
numbered fields, mirrors, choices, [variables/transforms](CompletionVariables.md)
and final caret are reused. Accepting an insertion uses one editor transaction;
save separately. Escape dismissing the palette does not edit or save anything.

## Wrap a selection

Select text and choose **Edit > Surround with snippet...**, or use Ctrl+Alt+J
in the Rider profile. Other profiles expose the action without a default shortcut.
**Insert snippet...** also uses the selection when one exists. For example:

```json
{
  "Parentheses": {
    "prefix": "paren",
    "body": "(${1:$TM_SELECTED_TEXT})$0"
  },
  "Region": {
    "prefix": "region",
    "body": ["#region ${1:name}", "$TM_SELECTED_TEXT", "#endregion$0"]
  }
}
```

The first example wraps the captured text in parentheses and makes it the first
editable field. The second keeps the selection as literal code between two new
lines. These are insertion definitions, not language-aware refactorings; scope
them to the appropriate language when necessary.

A selection may span lines and be made in either direction. Its ordered UTF-16
endpoints must include the request caret at either end, and its source UTF-8 text
must fit in 8 KiB. CyCode stores the original byte range in its issued session and
requires the primary replacement to equal it exactly. The body must actually
resolve `TM_SELECTED_TEXT`; an unused default, an escaped variable or a literal
choice does not count. Incompatible bodies are disabled with a reason in the
palette and refused again by native preparation.

The value is inserted literally: `$1`, backslashes and braces in selected code
cannot create fields. Explicit variable transforms can intentionally alter it.
Wrapping preserves the literal layout of the body and selection, without automatic
reindentation; use language formatting separately if needed. New body lines do
not automatically gain the current indentation. Moving the selection anchor,
even with an unchanged caret, invalidates the open palette and pending acceptance.
Accepting still uses the existing single completion/Undo transaction and does not
save. Ordinary server completions retain their same-line primary-range rule.

## Catalogue format and bounds

The root is an object of named definitions. `prefix` is a string or an array of
one to sixteen nonempty strings; `body` is a string or an array of strings joined
with newlines. Optional `description` and comma-separated `scope` are strings.
An omitted/empty scope applies to every file. Names/prefixes/descriptions must
not contain control characters. Unknown properties and `isFileTemplate:true`
are rejected; false is accepted for compatibility. JSON comments and trailing
commas are not accepted. This is a subset of the VS Code catalogue format, not
an importer for every extension's executable snippet provider.

Scope IDs are case sensitive. The native language ID is used where available:
`c`, `cpp`, `csharp`, `rust`, `java`, `javascript`, `javascriptreact`, `typescript`,
`typescriptreact`, `html`, `css`, `python`, `hlsl`, `glsl`. Other files use their
lower-case extension without the dot, such as `verse`, `log` or `json`.
Extensionless/`.txt` files use `plaintext`; `.md`/`.markdown` use `markdown`.
Custom extensions can therefore have their own scoped definitions without a server.
This is a scope identifier, not a declaration of semantic language support.

The source file is limited to 256 KiB, 256 definitions, eight JSON nesting levels
and 16,384 parser events. Each body is at most 8 KiB or 256 array entries; prefixes
are at most 128 bytes, names 160, descriptions/scopes 1024. Every definition is
validated, including excluded scopes. At most 512 matching proposals are issued;
limits are reported, not presented as a complete list. A body outside the supported
snippet grammar or transform budgets fails preparation without modifying text.

## Plugin catalogues

During ABI2 initialization, check that `struct_size` includes `features` and test
`CYCODE_HOST_FEATURE_SNIPPETS_V2` (16). Then call `contribute` with kind `snippets`,
the following descriptor encoded as UTF-8 JSON, and a null callback. The ID must
start with the manifest plugin ID and a dot; this example assumes `example.code`:

```json
{
  "id": "example.code.snippets",
  "title": "Example code snippets",
  "order": 20,
  "definitions": {
    "Wrap": { "prefix": "wrap", "scope": "cpp,hlsl,glsl", "body": "(${1:$TM_SELECTED_TEXT})$0" }
  }
}
```

`definitions` is a nonempty object using exactly the saved catalogue grammar.
Optional `title` is one line of at most 160 bytes; `order` is an integer between
-10000 and 10000, default 0. The only descriptor keys are `id`, `title`, `order`
and `definitions`. File paths, callbacks, context/when conditions, owner overrides
and executable snippet providers are refused. Language scopes still filter each
definition. Native plugins remain trusted in-process libraries; declaring static
snippets does not sandbox their other code. No additional permission is required
to contribute or query text; applying a native plan still requires workspace.write.

The registry accepts 32 catalogues and 1 MiB of serialized descriptors including
host owner metadata, counting inactive registrations too. Each catalogue retains
the project grammar's 256 KiB / 256 definition / 8 KiB body limits. The project
file comes first, then personal definitions, followed by active plugin catalogues
sorted by `order`, then ID.
At most 512 combined proposals are issued (and the ordinary completion payload
budget still applies). Limits are reported; definitions are validated even after
the result limit or when their language is excluded. Duplicate prefixes/names
remain distinct, without overriding project text. Plugin labels include the
owning plugin ID, and names, descriptions, title and catalogue ID remain searchable.

Every item carries `cycodeSnippetSource` with `kind` (`workspace` or `plugin`),
`catalogue`, `plugin` and `title`. `result.cycodePluginRevision` records the native
registry revision; clients must use the issued session/index, not supply replacement
metadata. `plugins.list.snippetCatalogues` exposes active summaries and definition
counts, without copying bodies into the ordinary extension inventory.

Queries capture the active snapshot outside the Engine mutex, without invoking
plugin callbacks, and recheck its atomic revision before issuance. Native preparation
checks that revision again before and after rendering. Disabling a plugin or a
required dependency, or otherwise changing plugin configuration, invalidates pending
local sessions conservatively. Desktop palettes and asynchronous acceptance also
check the registry. A concrete plan already returned through the SDK retains the
usual source/disk/apply guards; it contains only prepared text, with no plugin callback.
Re-enabling or updating a package requires a fresh query; installed package version
selection follows the existing next-start model. No automatic insertion is performed.

The bundled Unreal provider 1.6.0 uses this feature when the host advertises it.
In C++ files, search `ue_log`/`uelog`, `ue_property`, `ue_function` or `ue_editor`.
These are four definitions (five prefix proposals): UE_LOG with verbosity choices,
a Blueprint property, a Blueprint-callable declaration, and WITH_EDITOR selection
wrapping. They require the plugin to be active and use scope `cpp`; they are offered
in any C++ workspace, without guessing project/reflection context. They add no
includes, implementation, generated headers, semantic analysis or Unreal compilation.
Choose a valid enclosing class/context and review generated code. Older hosts omit
the optional catalogue while retaining existing Unreal contributions.

## Native, SDK/MCP and plugins

`snippets.complete` / SDK `snippets_complete` accepts `path`, `expectedRevision`
and a zero-based UTF-16 `position`, plus an optional ordered `selection` range
with `start`/`end` positions. Nonempty selections use the rules above; an empty
range behaves as ordinary insertion. Coordinates must be nonnegative integers,
within the document and outside surrogate pairs. The document must already be open. It returns
the ordinary `sessionId`/materialized `result.items`, plus `language` and the
workspace-relative `catalogue` path. It neither opens a new document nor enables
or contacts a language server. The same eight-session/five-minute/8 MiB completion
cache includes the saved catalogue baseline in its byte budget.

Pass an issued index/session/revision to `language.completionPrepare` /
`language_completion_prepare`, then use the existing preview/apply/undo services.
Local proposals do not use `language.completionResolve`. The saved catalogue must
still match byte-for-byte during preparation, alongside the ordinary source,
workspace and disk guards. Regex/rendering preparation runs on immutable copies
without retaining the Engine mutex; all guards are repeated before storing a plan.
A prepared concrete plan keeps the usual apply/undo checks.

ABI2 plugins use these same JSON host services; no STL or ImGui object crosses
the boundary. Querying/preparing adds no write permission; applying a plan still
requires `workspace.write`. The separate personal catalogue save requires
`settings.write`. A structured catalogue manager/installation UI,
file templates, smart reindentation, automatic
suggestion merging and interactive field transforms remain separate work.

Compiler-only checks and opt-in source fixtures are described in
[Validation](Validation.md). Actual native/GUI/Undo/keyboard and Linux/macOS
acceptance has not run during the current antivirus suspension.

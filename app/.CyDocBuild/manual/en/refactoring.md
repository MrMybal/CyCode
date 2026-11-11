---
id: refactoring
title: Completion and refactoring
description: Review completion, rename and code-action changes before saving.
section: Work in CyCode
order: 60
parent: editing
revision: 10
---

## Complete code {#completion}

Use Ctrl+Space to request completion from the active language service. Accepting an item can insert its associated imports as one editor transaction. Snippet fields use Tab / Shift+Tab; Escape finishes the snippet. Ctrl+Space leaves the current snippet to request another completion.

## Project insertion snippets {#snippet-catalogue}

Choose **Edit > Edit project snippets...** to open `.cycode/snippets.code-snippets` under the workspace root. On first use this explicit action creates the file with an empty JSON object in its unsaved buffer; existing files/buffers are retained. Edit definitions as UTF-8 JSON and save. Choose **Edit > Insert snippet...**, or find this action in the command palette. Rider uses Ctrl+J; Visual Studio uses Ctrl+K, Ctrl+X. The VS Code profile exposes the action without a default shortcut. Search the list by name, prefix or description, choose with the arrow keys/Enter and dismiss with Escape. This explicit list works without a language server, including for Verse.

```json
{
  "Class": {
    "prefix": ["class", "cls"],
    "scope": "cpp",
    "description": "Class named after the file",
    "body": ["class ${1:$TM_FILENAME_BASE}", "{", "\t$0", "};"]
  },
  "Verse message": {
    "prefix": "msg",
    "scope": "verse",
    "body": "Print(\"${1:Hello}\")$0"
  }
}
```

Names identify definitions. `prefix` is a string or an array; `body` is text or an array joined with newlines. Optional `scope` lists comma-separated language IDs; omit it for every file. Use `cpp`, `csharp`, `rust`, `java`, `javascript`, `typescript`, `html`, `css`, `python`, `hlsl` or `glsl`; JSX/TSX use `javascriptreact`/`typescriptreact`. Other files use their lower-case extension, such as `verse` or `log`; text/extensionless files use `plaintext` and Markdown uses `markdown`. Save changes and request again. Comments, unknown properties and file templates are refused.

Insertion requires a writable saved workspace file, one caret; selection wrapping is described below. Only a case-sensitive matching word prefix before the caret is replaced; otherwise the body is inserted at the caret. Text after it is retained. Numbered fields, variables, indentation and Undo reuse ordinary completion. A changed document, caret or saved catalogue requires a fresh request. The catalogue is bounded to 256 KiB, 256 definitions, 16 prefixes per definition and 512 proposals, with 8 KiB bodies. Errors and limits appear in the list. It does not merge into automatic server suggestions. Personal storage and active plugin catalogues are described below.

## Surround selected code {#snippet-selection}

Select code, then choose **Edit > Surround with snippet...** (Rider Ctrl+Alt+J).
The VS/VS Code profiles expose the command without a default shortcut. Insert
snippet also uses an existing selection. Save a definition such as:

```json
{
  "Parentheses": { "prefix": "paren", "body": "(${1:$TM_SELECTED_TEXT})$0" },
  "Region": { "prefix": "region", "body": ["#region ${1:name}", "$TM_SELECTED_TEXT", "#endregion$0"] }
}
```

TM_SELECTED_TEXT is the original selection, inserted literally: dollar signs,
backslashes, braces and Unicode code do not create snippet fields. The first
example creates one editable field containing the selection. Explicit transforms
can change it. These definitions insert text; restrict their scope as appropriate.

One selection can span lines, in either direction, up to 8 KiB of UTF-8 source.
The request caret must be at an endpoint. The body must actually use the variable:
mentions in unused defaults, escaped text or literal choices do not qualify.
Incompatible bodies are disabled with a reason and refused during preparation.
The replacement must exactly match the issued range. Moving the anchor, caret or
source invalidates the list and pending acceptance, even if the caret stays put.

Wrapping retains the literal body/selection layout without automatic reindentation;
new body lines do not gain the current indentation. Format separately if needed.
It uses the same completion/Undo transaction; Escape dismisses without editing,
and accepting does not save. Server completions do not capture selected text.

## Personal snippets {#user-snippets}

Choose **Edit > Edit user snippets...**, also in Find Action without a default
shortcut. The shared JSON editor supplies draft Undo/Redo/find. Save or Ctrl+S
validates, explicitly saves and closes; insertion does not save the target source
document. Reload and closing/quitting ask before discarding a dirty draft. Errors
keep the draft. A pending storage operation blocks closing; workspace switching
waits for this editor to close. There is no autosave or persistent file Undo after
the dialog closes.

The ordinary desktop/installed CLI use data/settings/snippets/user.code-snippets,
shared by projects in this portable installation. Reading a missing file offers
empty JSON without creating it. Use the project format above, up to 256 KiB and
256 definitions. Readable malformed UTF-8 JSON can be repaired; unreadable,
invalid-UTF-8, binary, oversized or redirected storage is preserved. Correct such
storage externally and Reload. Another instance's save causes a revision conflict;
reload explicitly rather than forcing an overwrite.

Merge order is project, personal, then enabled plugins. Personal entries show
[user] and retain source provenance; duplicates stay distinct. The common 512-item
and payload limits apply. Variables describe the target workspace/document. Native
queries/preparation recheck saved content; the UI uses local refresh generations,
not per-frame file reads or a cross-instance watcher. Concrete SDK plans already
returned retain ordinary apply guards. OS-user storage, cloud synchronization,
external imports and automatic server merging remain unavailable.

Native/SDK/ABI2 clients use snippets_user_read/save through the shared owner. Save
requires settings.write, separate from workspace.write. Embedders opt in with a
fixed absolute directory; default Engine and desktop smoke/test sessions do not
read or create personal storage. These preferences are not temporary task files.

## Create a snippet from selected code {#capture-snippet}

Select one range in a writable saved workspace document, then choose **Edit >
Create user snippet from selection...**, also in Find Action without a default
shortcut. It copies the current buffer, including unsaved edits, into the same
personal JSON editor. Source and saved catalogue remain unchanged. Edit the
generated Selection snippet name, snippet prefix, source-language scope and body,
then explicitly Save/Ctrl+S. Existing definition values are retained; the complete
draft is pretty-printed with two-space indentation. Later JSON edits have draft
Undo/Redo; Discard/Reload/Close keep the existing unsaved-change decisions.

Dollar signs, backslashes, closing braces, line breaks and Unicode are copied
literally. The body ends in $0 without numbered fields. Selection is limited to
8 KiB; the escaped body plus final caret must also fit 8 KiB. A full/invalid or
disabled personal catalogue refuses capture; no file or lock is created. Reload
can open the saved catalogue for repair after an error. Another instance's save
causes the ordinary revision conflict on your explicit save. This is a captured
copy rather than a live link to later source changes.

The shared read-only SDK operation is [snippets_user_draft](doc:operation-snippets-user-draft-snippets-user-draft).

## Plugin snippets {#plugin-snippets}

Enabled ABI2 plugins can supply inline catalogues to the same list. Workspace
definitions come first, then personal definitions, then plugin catalogues by order and ID. Duplicate prefixes
remain distinct; plugin candidates show their owner ID. Search includes names,
prefixes, descriptions and catalogue titles/IDs. Disabling a plugin or required
dependency invalidates pending queries and desktop acceptance. A fresh query reads
the active registry without running a provider callback.

The registry holds at most 32 catalogues / 1 MiB, including inactive registrations.
Each catalogue keeps the grammar and size limits above; the merged list has at
most 512 candidates and the ordinary completion payload budget. Personal storage is fixed by the host; there is no OS-account-wide directory,
external-file import or automatic merge into server completion.
Already returned concrete SDK plans keep normal source/disk/apply guards.

Unreal's active provider supplies ue_log/uelog, ue_property, ue_function and
ue_editor in scope cpp. The last wraps a selection with WITH_EDITOR guards.
These templates also appear in other C++ workspaces when the plugin is enabled;
they do not query reflection, add includes or compile code. Review the enclosing
class and requirements. See [Unreal snippets](doc:unreal#code-snippets) and
[plugin registration](doc:plugin-api#snippet-catalogues).

## Snippet context variables {#snippet-variables}

Server-provided snippets can use the current filename (`TM_FILENAME`, `TM_FILENAME_BASE`), document/workspace paths, current line/word, original line number and single-cursor index. `$TM_FILENAME_BASE` inserts the filename without its last extension; `${TM_CURRENT_WORD:identifier}` supplies a default when the word is empty. A default can contain an editable field, such as `${CUSTOM_NAME:${1:identifier}}`. Variable values are inserted literally, while numbered fields keep their usual Tab navigation and linked editing.

The context comes from the original completion request, before additional imports move lines. Accepting the completion and imports remains one Undo transaction; save separately. TM_SELECTED_TEXT is the explicit local selection above, or empty otherwise. Clipboard, environment/date/random variables and unknown bare variables without defaults are unavailable. A snippet exceeding the supported grammar or size limits is omitted from suggestions or refused during preparation.

## Transform snippet variables {#snippet-transforms}

A language server can return `${VARIABLE/pattern/format/options}`. For example, in `Actor.cpp`, `${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}` inserts `ACTOR`. To start an editable field with that value, use `${1:${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}}`. It is computed once; subsequent edits keep the usual linked fields and Undo transaction.

Formats support the whole match (`$0`), numbered captures (`$1`, `${1}`), conditional text (`${1:+then}`, `${1:-else}`, `${1:else}`, `${1:?then:else}`) and the case methods `upcase`, `downcase`, `capitalize`, `camelcase`, `pascalcase`, `snakecase`, `kebabcase`. Empty captures select the else branch; `:-` and `:else` retain nonempty captures. Escape literal `/` in a pattern as `\/`. Flags are `g`, `i`, `m`, `s`, `u`, each once.

The suggestion list displays the server label for transformed items. Regex is evaluated during native preparation, away from the GUI drawing thread. A malformed expression or exceeded budget refuses preparation without modifying the document. PCRE2 always uses UTF-8/Unicode classes; it is not an ECMAScript engine. Case conversion uses simple Unicode mappings, independent of locale. Word cases normalize case and split word/acronym boundaries. With no match, the original value is retained; else formats apply only to actual matches.

Limits include 256-byte patterns, 16 captures, eight transformations and 64 match attempts per snippet, 8 KiB inputs/outputs and a cooperative 50 ms preparation deadline with additional matching/memory limits. This is a bounded subset of VS Code-style snippets. Interactive field transforms (`${1/pattern/format/options}`) and the remaining variable families are still unavailable. Personal user catalogues are supported as described above.

## Rename and code actions {#rename}

1. Place the caret on the symbol and choose Rename, or open context actions for the current range.
2. Choose an actual server proposal. Disabled actions explain missing support or context.
3. Review Before/After changes and any confirmations in the refactoring view.
4. Apply the change to live buffers, then save when ready.

The IDE refuses stale or overlapping edits and disk conflicts. Changing the source after a preview can require a fresh query. The server determines available refactorings; syntax highlighting alone cannot rename symbols safely.

# Context variables in completion snippets

CyCode resolves these variables in snippets returned by the active language
server or the [project insertion catalogue](SnippetCatalogue.md). Both the suggestion preview and issued-item preparation use the owning
document's current text, request position and workspace. The server cannot supply
another file/context through its item. Existing revision, server-epoch, cursor,
disk-conflict and change-plan checks still apply.

| Variables | Value before the completion is applied |
| --- | --- |
| `TM_FILENAME`, `TM_FILENAME_BASE` | Filename, or filename with its last extension removed |
| `TM_DIRECTORY`, `TM_FILEPATH` | Document directory or full path |
| `RELATIVE_FILEPATH` | Document path relative to the workspace |
| `WORKSPACE_NAME`, `WORKSPACE_FOLDER` | Workspace directory name or full path |
| `TM_CURRENT_LINE`, `TM_CURRENT_WORD` | Original line or lexical word at the request position |
| `TM_LINE_INDEX`, `TM_LINE_NUMBER` | Original zero-based or one-based line number |
| `CURSOR_INDEX`, `CURSOR_NUMBER` | `0` / `1`, for the existing single-cursor completion path |
| `TM_SELECTED_TEXT` | Original explicitly captured local-catalogue selection; empty otherwise |

Paths use the host's native path representation encoded as UTF-8. Current-word
matching uses identifier-like ASCII characters, `$`, `_` and non-ASCII bytes;
it is not a semantic language query. Additional imports can move the resulting
line/caret, but variables describe the original request context.

`$TM_FILENAME` and `${TM_FILENAME}` insert the same value. A default such as
`${TM_CURRENT_WORD:identifier}` is used when the value is empty. An unsupported
variable can have an explicit default, for example
`${CUSTOM_NAME:${1:identifier}}`; its numbered field remains editable.
Defaults can contain nested numbered fields and other supported variables.
An unused default never resolves its variables or introduces editable fields.

```text
class ${1:$TM_FILENAME_BASE} {
    $0
};
```

In `Actor.cpp`, this offers `Actor` as an editable field. Variable values are
literal text: a filename containing `$1`, backslashes or `}` cannot create fields
or another variable. The existing shared Snippet library still owns numbered
fields, mirrors, choices, indentation, UTF-8 byte ranges and the final caret.
The existing native change plan keeps the primary edit and additional imports
in one editor Undo transaction; accepting a completion does not save the file.

## Selected text

The fourteen variables include `TM_SELECTED_TEXT` for explicit project-snippet
wrapping. For example, `(${1:$TM_SELECTED_TEXT})$0` creates parentheses around the
selection and an editable field containing the original text. Values are escaped
before numbered-field parsing, preserving literal code and Unicode. Variable
transforms are explicit and may change that text. A nonempty selection must actually
be resolved by the body, not merely mentioned in an unused branch or escaped text.

The optional native/SDK range is captured only by `snippets.complete`; language
servers cannot invent one through item metadata. It may span lines, is bounded to
8 KiB, and must have the request caret at an endpoint. Without this captured range,
`TM_SELECTED_TEXT` is empty, so `${TM_SELECTED_TEXT:fallback}` uses its default.
Wrapping uses literal body layout without automatic reindentation. See
[Project snippets](SnippetCatalogue.md#wrap-a-selection) for commands and guards.

## Variable transforms

Variable transforms use `${VARIABLE/pattern/format/options}`. In `Actor.cpp`,
`${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}` inserts `ACTOR`. A transform can also be
the initial value of a numbered field:

```text
class ${1:${TM_FILENAME/(.*)\.cpp$/${1:/pascalcase}/}} {
    $0
};
```

This computes a value once when the completion is prepared. Later edits to field
`1` update its ordinary mirrors; they do not rerun the transform. The application
does not evaluate regex on the GUI thread while drawing the suggestion list:
transformed items display their server label instead of a computed-text preview.
The native preparation worker validates and computes the actual edit before the
existing plan can be applied. Invalid patterns can therefore appear in the list
but fail preparation without changing the document.

The format supports `$0` (whole match), `$1` or `${1}` through capture 16,
`${1:+then}`, `${1:-else}`, `${1:else}` and `${1:?then:else}`. Empty or unmatched
captures select the else branch. `:-` and `:else` keep a nonempty capture unchanged.
Branches can contain other capture formats; only the selected branch emits text
or calls a case converter. Escape literal `$`, `}`, `\`, `/` and `:` with `\`.
In the pattern, escape every literal `/` as `\/`, including within character
classes. JSON strings require their own additional backslash escaping.

Case methods are `upcase`, `downcase`, `capitalize`, `camelcase`, `pascalcase`,
`snakecase` and `kebabcase`. They use utf8proc's locale-independent simple Unicode
case mapping: one input codepoint maps to one output codepoint, without full
multi-codepoint or locale-sensitive expansions. `capitalize` changes the first
codepoint only. Word cases normalize letter case, remove punctuation and split
lowercase/digit-to-uppercase and acronym boundaries; for example
`HTTPServer2Value` becomes `http_server2_value` in snake case.

Options are `g` (all matches), `i` (case insensitive), `m` (multiline anchors),
`s` (dot includes newlines) and `u` (accepted explicitly). Each is allowed once.
PCRE2 10.49 always runs in UTF-8/UCP mode, even without `u`, with JavaScript-like
backslash escapes and unmatched backreferences. Common regex syntax works, but
this is not an ECMAScript regex engine: Unicode classes, case folding and advanced
PCRE syntax can differ. Global zero-length matches advance by one UTF-8 codepoint.
If nothing matches, the original variable value is retained; replacement else
formats are evaluated only for actual matches. Unknown transformed variable
names use an empty value. Unsupported bare variables still require a default.

## Limits and compatibility

The context adapter accepts at most 8 KiB of original snippet text and 8 KiB after
literal escaping/substitution, with at most 2048 nodes and 16 nested levels.
The shared parser retains its existing 64 KiB rendered-text and field/choice
limits. A large current line only affects a snippet that actually requests it.
Transform inputs, formats and raw outputs are also bounded to 8 KiB. Each pattern
has at most 256 bytes, 16 captures, 32 parentheses levels and 16 KiB of compiled
code. One snippet shares budgets of eight transforms, 64 match attempts and
100,000 callouts. PCRE2 additionally uses a 20,000 match-step limit, depth 128
and a 1 MiB matching heap limit. A shared 50 ms cooperative deadline is checked
around compilation/matching and during callouts; it is not a hard OS timeout.
JIT and process execution are disabled. Invalid UTF-8, unsupported formats/options
and exhausted budgets reject preparation without applying a partial edit.

Interactive placeholder transforms (`${1/pattern/format/options}`), unknown bare variables, clipboard,
environment, date/time/random variables and commit characters remain unsupported.
Project, personal and enabled-plugin catalogues share this adapter. Personal
storage is shared by this portable installation; OS-account-wide storage,
synchronization and file-path imports remain pending. Unknown names without defaults are refused rather
than converted to editable placeholders. This is a documented subset of the
[VS Code snippet syntax](https://code.visualstudio.com/docs/editing/userdefinedsnippets#_variables),
not a claim of full TextMate compatibility. Context resolution performs no file,
clipboard, environment or process reads.

MSVC/Clang syntax-only checks evaluate production substitution/format assertions without
creating or executing a test program. Native service/widget/Undo and language-server
acceptance, actual PCRE2 matching and utf8proc conversion remain separate and
unexecuted during the antivirus suspension; see [Validation](Validation.md).

# Open an included C/C++ workspace file

Put the caret in a supported literal `#include` directive and choose **Navigate > Open
included file**, Find Action, or the active editor’s context menu. This action has
no new default binding; use keyboard preferences to assign one. It is available
for saved workspace C/C++ documents, including read-only buffers and unsaved edits.
Untitled, SDK/debug references and external source documents are excluded.

```cpp
#include "../Public/Actor.h"
#include <library/Widget.inc>
#include <algorithm>
```

Quoted headers first try the path relative to the source directory. Both quote
and angle forms then consult the existing current workspace index for paths
ending in the requested path components. For example, `library/Widget.inc` can
match `include/library/Widget.inc`. Matches across modules remain separate
choices. Parent segments are supported only by the quoted relative-path probe;
there is no indexed basename guessing for `../...` paths. Extensionless and
`.inc` files are supported, as are other literal filenames. A generated header
can be followed if present in the local/indexed scope; this does not generate it.

A complete, unchanged, unique observation opens through the normal document owner
and preserves unsaved source edits. Otherwise the common CyUI palette shows
**Source-relative** or **Workspace index** provenance, cached path filtering,
arrows/Enter/click, Escape, limits and index status. Closing the palette suppresses
delayed opening. Changing source, document identity/revision, Engine or caret
invalidates the pending context. The existing joined future also covers shutdown
and workspace changes. Drawing the palette performs no filesystem queries. An
empty result points to Go to definition for compiler or external SDK headers.
The shared result list also offers explicit Retry, Refresh lookup index and retry,
path/report copying and contextual local help; see
[Related results](CppCounterparts.md#inspect-related-results). Ordinary lookups do
not refresh the index. Closing an explicitly requested refresh prevents delayed
opening but does not terminate that existing bounded index operation.

## Literal directive and context limits

The native reader inspects the live buffer, not the saved file’s contents. It
validates the requested UTF-16 position, then checks lexical context before the
physical line so a directive inside a block comment, ordinary/character string or
common raw string is not followed. Preceding escaped newlines preserve comment,
string and non-directive continuation context; numeric digit separators are
recognized. The reader is conservative when a raw delimiter is unsupported.

Literal `#include "..."` and `#include <...>` accept whitespace and inline block
comments before `#`, between `#` and `include`, and before the header. Immediate
backslash-LF or backslash-CRLF continuations may split the directive or keyword;
the header-name itself stays on one physical line. The caret may be on any of
the continued components. For example:

```cpp
# /* directive */ in\
clude /* header */ \
  "library/Widget.inc"
```

The returned inner-header UTF-16 range is on the actual header line, even when
the requested caret was on the first component. Physical offsets are retained.
Unescaped multiline comments between directive tokens and whitespace between
backslash and newline remain outside this conservative subset. `in/**/clude`
does not form the keyword. Forward slashes are required in the target. Empty,
absolute, colon/backslash/control-character paths, macros, `include_next`, imports
and unsupported spellings fail explicitly. The prefix is limited to 1 MiB, the
whole logical directive's physical span to 8 KiB and the target to 1024 UTF-8
bytes. Bounds or invalid positions are errors, not partial lexical guesses.
Preprocessor conditions are not evaluated: a supported literal in an inactive
branch is still followed. This implements a bounded subset of
[translation phases](https://eel.is/c++draft/lex.phases) and
[include directives](https://eel.is/c++draft/cpp.include), not a preprocessor.

## Shared index and metadata owner

The workspace file refresh owns one per-stem map of bounded header/source/other
offset buckets into its existing sorted file list. Both header/source switching
and includes reuse it. An include query examines at most 128 entries from the
matching stem/kind bucket, checks the full filename/suffix, copies only retained
matches, then shares the total 128 metadata-probe budget with the relative probe.
Overfull buckets are partial even if retained entries do not match the suffix.
No full-cache traversal, additional scan, process or cache activation is performed.

The shared observation reports `index.state`, watcher `revision`, refresh
`generation`, `truncated` and `unreadable`. Missing/busy/stale caches contribute no
paths. Partial current caches may contribute retained paths; concurrent refresh
or watcher changes mark the result changed. Rechecking after document-lock
reacquisition prevents treating an index changed during that wait as current.
Unavailable/incomplete observations always require explicit choice.

Only regular canonical targets inside the workspace are returned; file symlinks
and external targets are excluded. Windows path comparison ignores case; POSIX
comparison is case-sensitive. Local/index duplicates retain local provenance.
`ready` describes the existing ignore-filtered cache scope, not every file on
disk or compiler correctness. A target can change/disappear before guarded opening.

## SDK and plugin contract

`workspace.include` / SDK `workspace_include` takes `path`, `expectedRevision`,
`expectedDocumentId` and `position`. The document must already be open. The result
returns source `path`/`revision`/`documentId`, requested `position`, `include`
(`target`, `angled`, exact inner-header UTF-16 `range`), candidate `path`/`relation`,
index observation and bounds. ABI2 uses the same permission-free read-only host
service. SDK/plugin callers receive metadata, not a file-opening capability.

This is scoped filename navigation, not compiler include resolution. Compiler
include-directory/search-order interpretation, external Unreal/compiler SDK
headers, macro expansion, project rules and semantic lookup remain outside this
operation. Existing capable-server Go to definition is separate. No source or
project file is edited, saved or generated. No dependency, CyUI module or native
executable target is added. Actual filesystem/server/SDK/plugin/GUI keyboard/DPI
and Linux/macOS acceptance remain unverified; see [Validation](Validation.md).

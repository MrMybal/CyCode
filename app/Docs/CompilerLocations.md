# Compiler output source links

The Build panel's live and final logs reuse CyUI Diagnostics. Clicking a row with
a recognized source location opens that file through the normal guarded document
service. Ordinary rows remain selectable. Opening a source does not edit it,
replace its buffer, save it or add an Undo transaction.

The native `build.location` service is also exported through the existing ABI2
JSON host callback and SDK/MCP operation `build_location`. It receives supplied
text only, needs no permissions or mutation group, runs no compiler and reads no
source contents. Retrieving compiler output still requires `process.execute`.
No new dependency, CyUI module, C ABI or executable target is introduced.

```json
{"text":"src/main.cpp(42,7): error C2065: missing identifier","directory":"."}
```

Successful results contain `mapped:true`, a workspace `path`, one-based `line`
and `column`, `format` (`parentheses` or `colon`) and an empty `reason`. An ordinary
unmapped line returns `mapped:false` and a reason without coordinates. Invalid
requests raise an error. The working directory defaults to the workspace root
and must already exist inside the workspace.

Recognized forms include explicit `file(line,column):`, `file(line):`,
`file:line:column:`, `file:line:` and Rust `--> file:line:column` / `::: ...`.
Quoted paths, spaces, UTF-8 filenames, Windows drive letters, numeric MSBuild
`1>` prefixes and GLSL `ERROR:` / `WARNING:` file prefixes are supported. Numeric
GLSL source IDs such as `ERROR: 0:12:` have no file mapping. The parser is bounded
to one 16 KiB UTF-8 line; NUL, newline and other controls except tab/CR are rejected.
Line numbers are bounded to 10 million and columns to 1 million.

Only an existing regular file inside the current workspace can map. Missing,
external, symlink-escaping, drive-relative and URI paths are not source links.
Paths are checked again when opening. No basename search or guessed directory
is used: live project logs use their validated recorded task directory; native
live logs and combined final logs use the workspace root. Relative paths emitted
from an unrecorded subdirectory can therefore remain unmapped.

Compiler columns are reported coordinates. Their byte/display/Unicode conventions
vary by tool; exact Unicode columns, ANSI/terminal decoding, source maps, caret
inference and external SDK links are not implemented here. Compiler lines are
last-run observations, not proof of the current source revision. This parser does
not replace structured compiler diagnostics or LSP UTF-16 conversion.

Regression source joins the existing opt-in build fixture: formats, nested
working directories, quoted/Unicode/absolute names, missing/external/symlink paths,
bad numbers, controls and request bounds. Sources pass MSVC `/Zs` only. Actual
native filesystem/parser execution, row activation, focus, Unicode columns,
themes/DPI and Linux/macOS acceptance remain pending under the Kaspersky test
suspension. See [Validation](Validation.md).

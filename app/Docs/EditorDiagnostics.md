# In-editor language diagnostics

CyCode connects the shared CyUI CodeEditor's native squiggle/tooltip API to actual
language-server diagnostics. The application owns revision validation and mapping;
the library owns rendering, folding/scrolling, font metrics and hover behavior.
No editor/library source is copied or changed for this integration.

`language.status.servers.<backend>` now supplies `diagnosticGeneration` and
`diagnosticRevisions`. The latter maps a workspace-relative path to its native
`id`/`revision` and the server publication's `uri`. The native owner includes an entry
only when the publication explicitly names the current LSP overlay version and
the sent overlay still matches the document identity and revision. The combined status also
contains eligible mappings from its child servers. These are display metadata,
not permission to change/save text.

The desktop compares that native identity/revision pair with its tab, converts UTF-16 positions
to editor glyphs and rejects out-of-range/reversed ranges and surrogate splits.
Errors, warnings, information and hints use CyUI palette roles; the message and
optional source appear in the existing editor tooltip. Errors retain precedence
over overlapping hints. A zero-length range marks the adjacent real glyph, where
one exists; an empty line has no artificial character added to it.

Annotations are cleared after local edits and when the tab's revision, backend
availability or publication is no longer eligible. External/recovery/refactoring
changes use those same ownership checks. Close/reopen or a replacement identity
starts a new LSP lifetime with a higher wire version, even when queued close/open
updates coalesce; reload resynchronizes its retained identity. A fresh publication or theme change
rebuilds this annotation family. Search/other native annotation families are
preserved. Annotation changes add no text edits or Undo entries.

The LSP allows servers to omit publication versions. Those diagnostics remain in
Problems and in the existing native/MCP status response, but are not underlined:
their range cannot be proven to belong to the current buffer. This is a deliberate
limit of this display adapter. Problems labels unverified open-document
positions and excludes them from keyboard/source navigation; closed-file reports
remain advisory navigation destinations. See [Diagnostic navigation](DiagnosticNavigation.md). It does not fabricate a revision or wait for
diagnostics before supplying completion. Pull-diagnostic providers and missing
version information need further backend work.

Editor annotation work is bounded to 512 considered diagnostics, 64 lines per
range, 2,048 painted line segments and 131,072 glyphs total (4,096 per segment).
Messages are capped at 4 KiB on a UTF-8 boundary. The editor reports that additional
diagnostics are available in Problems when a display budget is exceeded. All
server diagnostics remain available there; full backend catalogues are not
replaced with this bounded display subset.

Source assertions cover astral Unicode and invalid surrogate boundaries,
multiline/point ranges, malformed ranges, preservation of another annotation
family, display budgets, current/stale/unversioned publication routing and theme
refresh. These checks have not been executed after the Kaspersky interruption;
MSVC syntax-only validation generates no test executable. See
[Validation](Validation.md) for the actual build/runtime evidence.

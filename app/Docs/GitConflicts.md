# Git conflict catalogue and reviewed index resolution

CyCode now has three related conflict workflows: structural catalogue/inspection,
reviewed binary or missing-side index resolution, and explicit grouped path choices
for rename conflicts. They share the native Git owner across desktop, SDK/MCP and
ABI2 plugins. The existing three-way working-text editor is retained.

After index resolution, a separate reviewed working-file service now installs raw
stage-zero bytes or physically removes an absent-index path, with persistent
backups and guarded Undo. It handles one file per transaction, without automatic
rename inference or Git filters/EOL conversion. See
[Git working files](GitWorkingFiles.md) for effects and pending runtime validation.

## Inspect actual Git stages

Open **Version control > Conflicts**, or **Resolve conflict** in Changes. The Git
catalogue shows all unresolved paths from complete `ls-files --stage -z` records.
It retains mode/OID/stage records, including SHA-1/SHA-256 object names, and absent
base/ours/theirs entries. An absent side is distinct from an empty file. Paths in
the new service are **relative to the selected repository**; `documentPath` maps
each entry to the workspace for the existing text editor.

**Inspect versions** classifies each stage as text, binary/unsupported encoding,
large, absent or unsupported, with its byte count. Valid UTF-8 text is returned
only up to 4 MiB per stage and 16 MiB of encoded response overall. Binary bytes
never enter editor buffers or JSON strings. Large blobs retain their metadata;
they are not downloaded/read into a text preview. Native blob output must match
its recorded byte count. Files in the working directory are not read by this
inspection. **Open working text conflict** retains the earlier working-buffer
editor; the working file must exist and satisfy the ordinary text-document guards.

The catalogue uses actual Git status codes for stage presence: DD, AU, UD, UA, DU,
AA and UU. These codes do not reconstruct a rename topology. Ours/theirs refer to
stages 2/3; rebase can reverse the branch roles a user expects from those names.

## Choose and review index changes

For each path choose **Stage ours**, **Stage theirs**, **Stage base**, **Stage
deletion**, or **Keep unresolved**. A missing selected side produces an explicitly
reviewed index deletion. **Preview index resolution** displays the exact selected
objects and deletions before **Apply reviewed index resolution** is enabled.

For rename/rename or rename/delete conflicts, select each affected original and
destination path explicitly in the same batch. For example, retain the ours stage
at the chosen destination and stage deletion of the original and unwanted
destination. Up to 32 paths are submitted together. CyCode never guesses which
unrelated files belong to a rename, invents a destination, or uses `--replace` to
erase a conflicting directory/file entry. Case aliases and overlapping requested
paths are refused. Symlink/gitlink/sparse-directory conflicts remain visible but
require an external resolver.

This is **index-only resolution**. The selected local blob OIDs and recorded
executable modes are staged directly without text normalization or clean filters.
Deletion removes the index entry only. Working files, unsaved documents and their
Undo/revisions stay intact. Consequently, the editor can still show conflict
markers or a different binary version after the index is resolved, and a deleted
index path can remain as a local/untracked file. Review staged and working diffs
before committing. There is no automatic checkout, physical rename/deletion,
merge continuation or editor Undo for this action.

## Shared native and extension contracts

| SDK operation | Native / ABI2 host method | Required inputs/effect |
|---|---|---|
| `vcs_conflict_list` | `vcs.conflict.list` | Selected repository; complete catalogue and revision |
| `vcs_conflict_inspect` | `vcs.conflict.inspect` | Repository-relative path and expectedRevision; nullable text/metadata |
| `vcs_conflict_plan` | `vcs.conflict.plan` | Choices, expectedRepository and expectedRevision; exact preview |
| `vcs_conflict_apply` | `vcs.conflict.apply` | Same choices/guards plus expectedPlanRevision; actual process result and catalogue or catalogError |

These typed SDK operations select built-in Git and reject an external provider.
External VCS plugins retain their provider-specific contracts. Read/preview exports
have no mutation permissions; apply requires `vcs.write`, `workspace.write` and
`process.execute`, with the existing workspace mutation group. The native VCS mutex
serializes GUI/MCP/plugin operations. Native ownership remains noncancellable and
the bridge keeps its existing bounded 600-second wait policy. ABI2 stays unchanged;
no additional dependency, executable target or CyUI module is needed.

## Guards, bounds and uncertainty

The preview binds selected repository identity, Git administrative directory
identity, HEAD, symbolic branch, complete listed index records and normalized
choices. Apply rechecks these values and locally available blobs before dispatching
one fixed-argv `git update-index` command. An existing regular index file is used
as the hooks path, preventing hook discovery below a directory. Fsmonitor, prompts,
lazy fetch and transport are disabled for these calls. No shell, filter, checkout,
commit or source-file write is requested.

Git owns its index lock. The preflight is not an atomic compare-and-swap with other
Git applications, so external-writer races remain possible. Postflight compares
all selected **and unselected** mode/OID/stage records against the reviewed result,
and rejects unstable catalogue snapshots. It does not promise byte-identical index
extensions/stat caches or an OS sandbox. Preserve the actual exit code/timeout/
truncation result: even exit zero can carry `catalogError` when the observed index
cannot be confirmed. Inspect and refresh after uncertainty; there is no automatic
retry, cleanup or rollback.

Bounds are an 8 MiB listing, 100,000 index records, 512 unresolved paths, 4,096 bytes
per path and 12,000 aggregate requested path bytes. Each subprocess has a 15-second
timeout, checked against a local 45-second inspection budget; a call already in
progress can finish after that budget. Truncated/incomplete records fail explicitly.

## Validation status

Windows x64 production compilation, MSVC syntax checks and real CyToolsCore
manifest/JSON Schema data validation are used during the current Kaspersky native
execution suspension. Existing opt-in VCS fixtures now specify malformed NUL
records, OIDs, stale plans, missing versions, binary data, modify/delete and
rename/rename groups, unrelated staged records and dirty-buffer preservation.
These native assertions have **not** been executed. Real Git command behavior,
hooks/transport suppression, filesystem/index preservation, GUI keyboard/themes/
DPI, MCP/plugin runtime and Linux/macOS acceptance remain pending. See
[Testing](Testing.md) and [Validation](Validation.md).

Git semantics follow its primary [ls-files documentation](https://git-scm.com/docs/git-ls-files),
[update-index documentation](https://git-scm.com/docs/git-update-index) and
[merge documentation](https://git-scm.com/docs/git-merge).

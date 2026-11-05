# Annotated language-server edits

Language-server fixes, refactorings and symbol renames can now attach passive
labels/descriptions to text edits. These remain ordinary guarded native change
plans: prepare, inspect, apply to live buffers, then Save separately. The common
Engine serves the desktop, real SDK/MCP and permission-checked native plugin host.

The protocol distinguishes descriptive annotations from annotations requiring
confirmation. See the official [LSP text edit contract](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/types/textEdit.md)
and [workspace edit capabilities](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.17/types/workspaceEdit.md).
The client now advertises changeAnnotationSupport and honorsChangeAnnotations
for code actions and rename. It supports version-mapped text document changes,
line-ending normalization and textOnlyTransactional handling; resource-operation
capabilities remain absent.

## Desktop review

Annotated plans always open Refactoring before applying, including corrections
requested with Enter/Apply correction and ordinary automatic formatting paths.
Unannotated actions retain their existing behavior. Change annotations lists each
label; expand its row for the description and affected files. Each required label
has its own confirmation checkbox. Apply changes stays disabled until all required
labels are confirmed. Closing the view does not apply or save source changes.

Confirmations survive redraws of the same plan and clear when the plan ID or
workspace changes. The existing before/after Diff, live-buffer application and
guarded Undo remain in use. Annotated and plain edits can share one transaction;
annotations do not introduce individual per-file commits or a second Undo model.

## Native ownership and bounds

ChangeAnnotations validates definitions/references and substitutes native IDs for
opaque server IDs. storeChange qualifies those IDs with the issued plan ID. An
acknowledgement from another plan, an unknown ID or a duplicate is refused.
Required confirmations are checked before any buffer mutation, followed by the
existing all-document revision/text/disk checks. Failed confirmation leaves the
plan available for review; a later source/disk change still invalidates applying it.

Only annotations on effective edits survive. No-op edits and unused definitions
do not request confirmation. Global plan metadata lists the actual affected files;
changes.preview retains the same IDs but filters each annotation's files to the
requested preview. Normalized text edits retain annotationId alongside their
range, oldText and newText. Undo needs no new acknowledgement.

Bounds: 128 definitions, raw server IDs up to 512 bytes, labels up to 2,048 bytes,
descriptions up to 4,096 bytes and 512 KiB of annotation metadata, including final
issued IDs/file lists. Labels are nonempty; descriptions are optional. NUL text,
invalid confirmation flags and unknown references fail explicitly. Empty raw
server IDs remain valid and are replaced with ordinary nonempty issued IDs.
The existing 100-document, 16 MiB before/after preview, per-file text and UTF-16/
overlap limits still apply. Plans remain process-owned and expire/evict through
the existing change store. No annotation is interpreted as a command, path grant,
Markdown/HTML program or executable plugin contribution.

## SDK and plugins

Prepared plans now include annotations (an empty array for plain plans). Each
entry has id, label, description, needsConfirmation and files. The output field
is optional in the published schema so old plain-plan examples remain compatible.
No operation ID or permission changed; there are still 170 SDK operations.

After inspecting the plan/preview, an SDK caller supplies the required issued IDs:

```json
{
  "id": "change-issued-plan",
  "confirmedAnnotations": ["change-issued-plan:annotation-1"]
}
```

This is the parameter object for changes_apply, not a raw LSP edit. The optional
confirmedAnnotations array holds at most 128 unique strings (1–256 characters).
Omitting it remains valid for plans without required confirmations; the native
Engine enforces which IDs must be present. workspace.write and the existing SDK
workspace-mutations group remain required. The actual CyToolsCore owns identities,
jobs and permissions; no Python editor/parser or alternate scheduler is added.

Native plugins now also receive the refactoring prepare/preview/apply/undo services
through the existing JSON host entry point, with the same metadata/acknowledgements
and workspace permission. Provider read callbacks cannot apply/undo. No C ABI
layout changed.

## Limits and verification

Executable server commands, resource create/delete/rename edits, snippet code-action
edits and writes outside the workspace remain unsupported. The existing pinned
TypeScript empty acknowledgement remains the sole passive command omission; no
additional command is executed. This feature enables annotated text edits from
available backends, not every refactoring offered by Rider or an Unreal asset tool.

Assertions extend existing core, code-action, extension and workflow-UI test targets, with
no additional runner. They cover definition/size/reference guards, effective-edit
grouping, plan-specific confirmations, versioned annotated actions, apply/undo and
GUI approval reset and native-plugin export/permission/read-callback guards.
Current verification is MSVC /Zs plus production compilation
and real SDK schema examples. Native assertions, actual backend/MCP/plugin calls,
checkbox/keyboard/focus behavior, themes/densities/DPI and Linux/macOS remain
unexecuted during the Kaspersky interruption. See [Validation](Validation.md).

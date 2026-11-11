# Personal snippet catalogue

To create a definition from selected source, use **Edit > Create user snippet from
selection...**. It opens a literal, unsaved draft in this same editor. See
[Selection capture](SnippetCapture.md) for source guards, limits and SDK access.

Choose **Edit > Edit user snippets...**, also available in Find Action without a
new default binding. The modal uses the shared CyUI CodeEditor in JSON mode,
the application theme/DPI and the editor's draft Undo/Redo/find controls. Ctrl+S
saves and closes; Undo/Redo buttons are also present. Save is explicit and validates
the complete catalogue. It does not change a project source file or save an inserted
snippet's target document.

The ordinary desktop and installation-aware CLI opt into a fixed file under
`data/settings/snippets/user.code-snippets`. This is shared by the projects of
that portable installation, not an OS account-wide or cloud-synced directory.
No recursive discovery, project-selected path or file import is performed.
An embedding must explicitly supply the third Engine constructor argument, an
absolute application-owned directory. `Engine(workspace)` remains unconfigured;
desktop smoke/test sessions omit storage. The catalogue and lock are not created
by construction, reads or completion queries. Only a successful save creates text
storage; an explicit failed save may have created its directory/retained lock file.
The installation data tree stays outside source/distribution packages.

The format is the same UTF-8 JSON object as [project snippets](SnippetCatalogue.md):
named definitions with prefix/body, optional scope/description and false
isFileTemplate. Variables, transforms, fields, choices and selection wrapping share
the native completion owner. Per catalogue: 256 KiB, 256 definitions, 16 prefixes
per definition and 8 KiB bodies, with the same parser depth/event bounds. A missing
file offers an empty JSON draft without writing it. Valid UTF-8 malformed JSON
remains editable with its parse error so it can be repaired. Unreadable, invalid
UTF-8/NUL, oversized, nonregular or redirected storage is preserved and cannot be
overwritten through this editor/service; correct it externally, then Reload.

The draft is retained on validation, revision or I/O errors. Reload asks before
discarding a dirty draft. Close/Escape/title-close and application quit use a
Save/Discard/Keep editing decision when dirty; a pending read/save blocks closing.
Workspace switching waits until this editor closes. No autosave is performed.
Undo/Redo belongs to the open draft; a committed preferences save has no separate
source ChangePlan or persistent file Undo after the dialog closes.

## Merge and freshness

Insertion order is project definitions, personal definitions, then enabled plugin
catalogues by order/ID. Duplicates remain distinct; personal entries are labelled
`[user]` and have cycodeSnippetSource.kind=user, catalogue=user. The common limit
remains 512 proposals plus the existing completion payload/cache bounds. Personal
definitions use the target document/workspace context, not the catalogue file.
Malformed personal storage fails the query visibly instead of silently omitting
templates. File templates, external imports, OS-user storage, synchronization and
automatic language-server suggestion merging remain separate work.

Each native session records an opaque saved-content revision. Queries and native
preparation reread/compare it; a different instance's saved changes require a fresh
request. The local UI also compares a cheap observation generation, without file
reads on every palette frame. That generation changes only when this Engine reads
or saves the store; it is not a filesystem watcher or cross-instance version.
Already returned concrete SDK plans retain ordinary source/disk/apply guards and
contain prepared text. Saving personal templates does not retroactively edit code.

## Native, SDK/MCP and ABI2

- snippets.userRead / snippets_user_read returns available, editable, valid,
  revision, generation, text and error; no extra permission, no creation.
- snippets.userSave / snippets_user_save requires text and expectedRevision, plus
  the distinct settings.write permission. workspace.write alone is insufficient.
  It is noncancellable after submission and serialized in the SDK user-settings
  group. Native plugin read-only callbacks cannot invoke it.
- Missing storage has revision `absent`. Present readable bytes have a 64-character
  lowercase BLAKE2b-256 content guard using the existing Monocypher dependency.
  Disabled storage has revision `disabled`; unreadable storage has an empty revision.
  Only absent/64-hex tokens can be submitted to save. These are consistency guards,
  not credentials. No caller path is accepted by either operation.

Saves validate before I/O, acquire the existing nonblocking NativeFileLease on a
retained `user.code-snippets.lock`, reread the baseline, compare its exact revision
and use the shared guarded atomic writer. A post-save read confirms the expected
content or reports an explicit unconfirmed write. Readable malformed JSON may be
repaired with a valid matching draft. Multiple cooperating CyCode writers share
the lock; arbitrary external writers do not participate in that lock protocol.
No retry or forced overwrite hides a conflict. SDK, GUI and C ABI JSON host requests
share this owner; no Python editor model or new native executable is introduced.

Compiler/data/manual checks and the native tests not executed during the antivirus
suspension are recorded in [Validation](Validation.md). Linux/macOS and real GUI,
Undo, filesystem locks, hashing and cross-instance acceptance remain unverified.

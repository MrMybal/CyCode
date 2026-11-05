# Installation space and failure outcomes

The Windows portable updater checks installation headroom after validating the
archive, manifests and current owned files. This is separate from the earlier
[download/staging budget](UpdateDownloads.md). It does not change startup checking,
installation consent, save/close handling or all-instance/cache leases.

## Space observations

The native helper resolves the installation volume and reads its allocation unit.
It rounds each extracted file, including manifest.json, up to that unit. Geometry
must be a power of two between 512 bytes and 16 MiB; unknown/unsupported geometry
stops preparation. Caller-available space uses the existing 64-bit observation,
including caller quotas. The 32-bit free-cluster count is never used for capacity.
See Microsoft's [volume-root API](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamew)
and [allocation geometry API](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespacew).

For archive entry count N, directory allowance D and allocation unit U, the initial
additional-space estimate is:

`rounded extracted payload + (N + D + 64) * U + 32 MiB metadata + 32 MiB spare`

D conservatively counts every parent component twice for each new entry
(extraction/destination), and once for each present original (backup destination).
Shared directories may therefore be counted repeatedly. The native parser still
bounds entry count to 4096, total uncompressed bytes to 256 MiB and each file to
64 MiB. Budget arithmetic rejects invalid inputs rather than overflowing. This
estimate does not charge the already-present archive/originals again; ordinary
installation backs up originals by same-volume rename rather than by copying.

The first observation precedes transaction-directory creation and extraction.
After extraction, hash verification and immutable recovery inputs, a second
observation repeats the metadata/directory/spare allowance without charging the
already-extracted payload. It precedes the pending marker and first original move.
Known insufficient space is a distinct installation failure, reporting required
and available bytes; neither observation authorizes installation by itself.

A transaction retains space.json with version, allocation unit, payload sizes,
entry/directory allowances, both budgets and the initial available bytes. A
successful second check adds space-before-commit.json. Failure before transaction
creation cannot have a transaction receipt; helper attempt reporting remains
best effort. Receipts are observations, not reservations or durable consent.

Filesystem metadata, sparse/compressed files, quotas and concurrent activity can
still cause a later write to fail. The allowance is conservative, not an exact
filesystem allocation prediction. There is no preallocation, automatic deletion,
disk-filling probe or full recovery-copy space guarantee. Recovery now has its own
[copy/retention budget and preparation](UpdateRecovery.md), also without reservation. Network-share/unusual
filesystem geometry and actual low-space/quota behavior remain unverified.

## Truthful failure reporting

The existing failed outcome remains compatible. result.json format 1 can now
add an optional failure field; old records without it remain valid:

| Code | Meaning |
|---|---|
| preparation | Validation/extraction/preparation failed before this attempt replaced installed files. |
| space | A known insufficient-space observation blocked replacement. |
| rolled-back | Replacement failed; the complete prior presence/size/SHA-256 baseline was verified and the owned pending marker removed. |
| recovery-required | Replacement started, and restoration or safe completion could not be established. Preserve the marker and all backups. |

Only failed records accept these four codes. Unknown codes or codes on other
states reject that cache observation without deleting its payload. Non-installation
helper failures can still have an unclassified failed outcome. A successful install
followed by restart failure remains restart-failed. EN/FR helper dialogs and the
Updates history use the same application catalogue; clipboard reports add only
the stable failure code rather than raw exception text.

Before the pending marker is committed, an exception never starts rollback and
never claims recovery is needed merely because a journal write also failed.
The helper best-effort records preparation-failed. Once replacement starts, an
ordinary failure reverses known replacements/moves and verifies every affected
path against its original presence, size, independent regular-file status and
SHA-256, including paths it never moved. Conflicts, missing originals or reporting
failures retain the recovery guard and report recovery-required. This is still
a multi-file operation; external races and power loss are not made atomic.

## Pending marker ownership

Creation is exclusive: a pending marker appearing during preparation is never
adopted as an overwrite baseline. Successful install, verified rollback and guided
recovery remove only a bounded, independent regular marker whose exact bytes
still match their own baseline. The Windows handle disallows concurrent writers
and renames while verifying and marking that same opened file for deletion.
Changed, linked, missing or busy markers fail closed. This uses the documented
[handle information API](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle)
and [file disposition contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_disposition_info).
Ancestor checks are observations, not protection against arbitrary hostile
same-user directory races. Keep all transaction data if completion is uncertain.
See [guided recovery](UpdateRecovery.md).

## Build and verification

UpdateBudget/UpdateInstall stay within the existing native updater, with no new
runtime, target, CyUI module, workspace SDK operation or plugin ABI. The helper
retains its minimal links and uses OS dialogs without SDL/ImGui. Ordinary builds
keep BUILD_TESTING=OFF. The opt-in existing core target gains source contracts for
allocation arithmetic, failure-code compatibility, changed/linked/busy marker
retention, preparation failure and verified rollback. MSVC /Zs checks their source;
constexpr assertions evaluate actual arithmetic and code mappings. No fixture
body, updater, recovery helper or GUI is run during the antivirus interruption.

Windows compilation, data-only checks and source audit are recorded in
[Validation](Validation.md). Real low-space, permissions, cancellation, concurrent
writers, power/process interruption, helper restart, EN/FR dialogs, DPI and
Linux/macOS acceptance remain pending. Automatic updating still targets only
Windows x64 portable distributions; this change does not supply a Linux/macOS
transport or installer.

# Interrupted application-update recovery

CyCode now implements explicit, guided restoration for new format-2 update
transactions on Windows x64 portable installations. It adds no executable target,
UI module, third-party dependency or workspace/SDK operation. Native runtime,
filesystem/GUI and power-loss acceptance remain unexecuted; see [Validation](Validation.md).

## Starting recovery

An interrupted replacement keeps data/updates/pending.json and blocks ordinary
CyCode/CLI startup. Do not remove that marker to bypass the block. The application
may itself be absent or a mixture of old/new managed files, so the full IDE does
not start a recovery window or implicitly execute a discovered helper.

Open the affected transaction's RECOVERY.txt. It names the existing staged
CyCodeUpdater.exe in data/updates/download-... . Double-click that helper to open
recovery, or invoke that same staged copy with --recover followed by the exact
installation directory. The installed root helper directs the user to this staged
copy. Running a helper without a pending update only displays an information box;
it does not start an installation.

The helper verifies its own location, prepared metadata and hash against either
recorded package version. It reads the active transaction, presents old/new
versions, restore/remove/already-restored counts and bounded conflict details.
Conflicts block confirmation. A fresh space observation adds estimated required
and caller-available bytes; insufficient/unknown capacity blocks confirmation with
EN/FR guidance. Capacity is not part of the reviewed file baseline or durable
consent: native recovery recomputes it under the exclusive lease. The restore question defaults to No. Declining
changes no installation file. Close every GUI/service instance and other updater
using this installation before accepting. Recovery takes an exclusive native
installation lease and never kills processes or elevates itself.

After successful restoration, a second question defaults to No for restarting
CyCode. A restart rechecks the current manifest-owned executable under a shared
installation lease before direct CreateProcess. Restart failure is reported as a
restart failure after completed restoration. It does not recreate a pending marker.

The native helper uses OS dialogs rather than linking ImGui/SDL/the IDE. A small
adapter reads the application's existing EN/FR catalogue and language preference.
Download staging captures updater-messages.json so messages remain available when
package UI files have moved. Missing/corrupt catalogue data falls back to English.
Backend diagnostic details remain technical text. No credential or environment
read is needed for recovery. The Updates history records Previous installation
restored; catalogue/history/report data uses the existing owners.

## Durable inputs and bounded validation

Before any original file moves, installation writes immutable before.json and
after.json manifest bytes, recovery.json, and RECOVERY.txt into its unique
transaction directory. The plan records original *presence*, hashes and sizes,
and the exact replacement set. The original manifest, CyCode.exe and CyCodeUpdater.exe records are required;
other previously missing managed files remain absent after restoring that baseline.

The pending marker has format 2 and binds the canonical installation's direct
transaction-* directory, release version and SHA-256 of recovery.json. The plan
has format 1; this is independent of package manifest updateFormat 1. JSON journal/
pending writes reuse the existing atomic text writer and disk baseline. Immutable
plan inputs are completed before the pending marker; review never trusts a move
counter or the mutable phase journal to decide what to overwrite.

Plan metadata is capped at 8 MiB, each manifest at 2 MiB and the pending marker at
8 KiB. Each manifest has at most 4095 payload files, 64 MiB per file and 256 MiB
total. Both before/after manifests share the same native validator as installation;
their union rejects case collisions and file/directory prefix changes. Every plan
record must agree with those manifests. Original helper/executable/manifest records
are mandatory. Development checkouts, foreign transactions, protected paths,
traversal, special files, reparse/symlink ancestors, hard-linked payloads and
unsupported/legacy formats are rejected.

Read-only review validates the complete managed union and backup states. Unexpected
sizes are rejected without hashing arbitrary large files. Each hash has a streaming
size limit and cancellation checks per 64 KiB chunk. Unrelated project/data/plugin
files are not scanned or changed. Conflict details retain at most 32 rows while
the total conflict count remains complete. This is a manifest-bounded filesystem
inspection, not a per-frame task or performance benchmark.

## Restoration and interruption

After confirmation, the native service reacquires exclusive ownership and repeats
the full review. The exact pending/plan/path/hash/presence snapshot must still match.
A changed file, missing backup or changed pending marker blocks that reviewed action.

For each original, an already-restored target is left in place. Otherwise a verified
original backup must exist, and the target must be absent or exactly match the known
replacement. A previously absent target may only be absent or the known replacement.
Unknown/local changes block restoration.

All needed originals are first copied into a unique recovery-*/restored tree through
exclusive, bounded, flushed copies and individually reverified. No installed target
moves during this phase. A copy/hash/write/cancellation failure retains prepared
work and original backups, with installed paths unchanged by this attempt. The
complete pending/plan/target/backup baseline is then scanned again; changed context
blocks commit. A second headroom check excludes already-prepared copy payload.

Only after this preparation are verified replacements moved into that same
recovery-*/replaced tree, preserving their bytes. Prepared originals are rechecked
before the corresponding destination moves, then renamed into unoccupied targets
without overwrite. An external edit racing a move is retained and stops recovery.
Original backups are never consumed. Files that were absent before installation
return to absence; the package manifest is restored last. Unknown files and
extension/user data remain untouched. There is no copy after vacating a target.

A final full scan must show the prior baseline everywhere, with the same pending
and plan binding. Only then is recovery-status.json marked complete and pending.json
removed through the verified independent file handle. Changed/linked/busy markers
remain; a successful postflight never authorizes deleting a replacement marker.
See [marker ownership](UpdateInstallation.md). Status records identify the recovery work directory. A failure retains the
pending guard, original backups and recovery copies. A subsequent *new* review can
accept originals already restored by a previous attempt and finish remaining work.
The helper now offers cooperative cancellation between operations/chunks and keeps
the progress window open until the worker stops. Process interruption is handled by
the same later review rather than replaying uncertain counters.

This remains a multi-file restoration, not a crash-atomic directory swap. Atomic
metadata/flushes do not prove power-loss durability of every filesystem/volume.
Neither process/power interruption nor concurrent external writers are accepted
without fresh file validation. No unattended restoration, completed-update downgrade
selector, cache pruning or automatic Linux/macOS installer is added.

Reversible [cache management](UpdateStorage.md) shares a cache lease with the staged
helper and refuses pending installations. It does not move recovery transactions
or original/replacement backups.

## Recovery space and retained preparation

Recovery has its own budget in the existing UpdateBudget owner. It charges only
originals still required by the restore, rounded to the actual volume allocation
unit, minus verified reusable allocation already on disk. Replacements retained by same-volume rename and already-existing backups,
archives and old recovery work are not charged as new payload. Directory allowances
count two parent paths per copy (staging/final) and one per retained replacement;
shared parents may be counted repeatedly. The estimate is:

`allocated copies - verified reusable allocation + (copy count + retained count + parent allowances + 64) * allocation unit + 32 MiB metadata + 32 MiB spare`

Original/installed sets each retain their existing 4096-entry and size bounds;
copy count and retained count are independently bounded to 4096. A zero-action
recovery still charges metadata/spare for journal/marker completion. An unknown
geometry/capacity stops; a known shortfall reports required and available bytes.
The preview does not create recovery work or modify installed files. Its space
fields are observations only: callers cannot authorize a restore by changing them.

Under the exclusive installation lease, the initial check precedes work creation.
After staging every copy and revalidating the full file baseline, the second check
charges the same metadata/directory/spare allowances without charging the now-
existing copies again. Initial space.json and space-before-commit.json receipts
bind the release/plan, counts, allocation unit, payload footprint, required and
available bytes; copiesPrepared identifies the second estimate. Existing files
and retained failed preparations already occupy their disk space.

recovery-status.json records preparing, recovering and complete. Ordinary failures
record preparation-failed if this attempt moved no installed path, otherwise
recovery-needed. Both retain pending.json, original backups and work files; these
phases never replace the full baseline scan used for later review. A disk-space
failure before work creation need not have a new receipt. Reporting is best effort
when storage itself is unavailable. No old work is automatically deleted.

These checks are conservative estimates, not reservations or exact filesystem
allocation predictions. Concurrent writers, quota/metadata changes, permissions
and power loss can still interrupt commit. A later new review derives remaining
copies from verified current targets, not prior counters. Real low-space, copy-
failure, lock/cancellation, native dialogs/restart and Linux/macOS acceptance remain
unverified. Live progress and cooperative stopping are implemented as described
below; their actual native-window acceptance is still pending.

## Reusing verified preparations

A retry can reuse complete original copies prepared by an earlier interrupted
attempt of this exact active transaction. It still starts from the current
pending/plan/target/backup scan and asks for normal explicit restoration consent.
Original backups remain mandatory and are retained; a prepared copy cannot replace
a missing or corrupt authoritative backup. Files already restored are excluded.
There is no automatic recovery, completed-update downgrade or partial-file append.

New recovery work records an immutable work.json identity (format 1), binding the
canonical installation, active transaction directory, version, plan SHA-256 and
exact pending bytes. Immutable prepared.json (format 1) maps original managed
names to recovery-work directory basenames. A fresh attempt can therefore retain
references to several earlier source directories without copying those payloads
again. Names must belong to the original plan; directory values cannot contain
paths, traversal or arbitrary identifiers. The map is a hint, not proof that the
predicted files were ever written.

Both records are created exclusively before publishing the new mutable status
pointer and before starting a copy. If this setup fails, the previous status hint
is preserved. Each retry starts only from the active transaction's last status
hint. It validates the work identity, map, source-directory identities and each
candidate's independent regular-file status, exact size and SHA-256. A mutable
phase such as complete never establishes file readiness or installation completion.

Lookup is bounded: 8 KiB status, 64 KiB per work identity, 4 MiB / 4096 entries in
the preparation map, and 32 distinct work identities including its owner. Invalid
identities count toward this directory limit. Each needed original has at most one
candidate and bounded-size hashing; the original set retains its 256 MiB plus
2 MiB manifest bound. No recursive history/directory walk or arbitrary source path
is followed. Oversized, foreign, special, changed or missing data is ignored and
kept. Required originals are then copied anew into a unique directory under the
ordinary headroom check. Partial earlier copies are never truncated or overwritten.
Limits can reduce reuse; older unreferenced work remains protected.

The preview reports verified available copies, remaining copies from backups and
ignored/limited preparation hints in EN/FR. RecoverySpace keeps logical copy count,
bytes and allocation for the complete remaining restore, plus reusedFiles,
reusedBytes and reusedAllocation. Initial additional headroom deducts only verified
already-existing reused allocation; metadata/directory/spare allowances remain.
The native preflight recomputes these observations under exclusive ownership, so
supplied counts or a changed cache cannot authorize a smaller budget.

A new work map is published before copying, letting a later attempt find completed
files even if the previous process stopped mid-preparation. Valid reused payloads
stay in their earlier directories until guarded commit. Missing/new payloads are
copied into the new directory. Before the first target moves, all selected source
identities and copies are checked again, along with the full installation baseline
and commit headroom. Each source identity/copy is checked again at its move. A
selected file that changes during preparation stops rather than being overwritten
or trusted. As before, an interruption after commit starts may require another
full review; this is not a crash-atomic or hostile-writer-proof operation.

Initial/commit space receipts add actual reuse counts/allocation and hint-limit
flags. Original backups, unused/modified preparations and receipts remain on disk;
there is no cleanup or reclaimed-space claim. A retry may reference older copies,
so work directories must not be deleted manually while recovery is pending.
Legacy preparation directories without these new identity/map records are kept,
but their copies are prepared again from verified originals. Actual resume, low-
space/concurrency, lock/copy/interruption and native dialog acceptance remain
unexecuted; the implementation is source/build/data checked only.

## Visible progress, stopping and result reports

After the existing default-No confirmation, a native Windows progress window starts
the same recovery service on a joined worker. The worker publishes plan/transaction-
bound atomic observations; the window reads them without filesystem/network probes.
Inspection, preparation, prepared-file verification, restoration and final baseline
checking have distinct EN/FR labels. Counts show verified prepared copies and
completed restore/remove entries. Reused copies count as prepared without inflating
bytes copied during this attempt. Copied bytes are actual reported writes, including
a partial failed write; they are not proof of flushing or durable recovery.

Preparation/restoration percentages describe completed files in that phase only.
Unknown/empty totals, inspection, hashing and final checking remain indeterminate;
there is no fabricated overall percentage or ETA. All entries moved still requires
full postflight and exact owned-marker removal before success. The public native
RecoveryProgress is a single-attempt observation tied to the exact plan digest and
transaction. A foreign or already-claimed instance cannot authorize recovery.

Stop recovery, Escape and closing the progress window request the same cooperative
stop. The dialog vetoes closing while work remains, disables repeated stop requests
and shows a stopping message. It never detaches or force-terminates the worker.
Cancellation checkpoints occur between operations and 64 KiB copy/hash chunks;
blocked filesystem calls can delay stopping. A late stop after actual completed
postflight remains a success. Cancellation is a typed internal outcome, distinguished
from ordinary failures. Its result distinguishes no installed path moved by *this
attempt* from an attempt that has already moved installed paths. Earlier attempts
may already have changed the installation. Both stopped cases retain the pending
guard and backups and require a fresh review, without automatic retry or rollback.

The result window offers Copy recovery report on an explicit button only. Its
bounded JSON (at most 32 KiB) contains version/context/plan identity, stable phase,
counts, copied bytes and whether this attempt moved installed paths. Raw exceptions,
environment variables and credentials are excluded; optional technical details are
displayed separately. Clipboard failure is reported while retaining the result.
Successful restoration offers an explicit restart with Close selected by default;
restart still uses the existing verified executable/installation lease.

Once a work directory has been advertised, recovery attempts best-effort creation
of work/recovery-result.json (format 1, at most 8 KiB). It checks that work's original
plan-bound identity and creates the receipt exclusively, never overwriting an unknown
receipt. Fields include version/plan, state (restored/cancelled/failed), observations
and pendingCleared. On interruption it records the stage where work stopped; the
copied result uses the final terminal phase. Early preflight failure or reporting/
storage failure may leave no receipt. Reporting failure cannot turn a known completed
restoration into failure. Receipts/counters are observations, never authority for a
later restore, deletion or restart.

New receipts also contain informational Unix seconds in time. Earlier format-1
receipts remain readable without this field. Once ordinary startup is available,
the IDE's explicit [detailed history](UpdateHistory.md) inspection can show receipts
alongside installation outcomes, with filters/pages and copying. It checks metadata
agreement without rehashing plans, backups or current files; a recorded result is
not proof of current installation completeness.

The thin helper uses the system [TaskDialogIndirect](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-taskdialogindirect)
component and Comctl32.lib, with an explicit embedded Common-Controls v6 manifest
and asInvoker execution level. The approximately 200 ms native callback timer only
reads progress and checks worker readiness. Button callbacks use the documented
[S_FALSE close veto](https://learn.microsoft.com/en-us/windows/win32/controls/tdn-button-clicked).
All window messages stay on the UI thread and the worker is joined before its
review/progress/cancellation state is destroyed. No ImGui/SDL/SDK context, new target
or CyUI module is introduced. System theme, keyboard, scaling, close/cancel timing
and real clipboard/restart behavior remain to be accepted on Windows; automatic
Linux/macOS recovery remains unavailable.

## Older or damaged transactions

Format-1/legacy pending journals cannot supply trustworthy baseline-presence and
replacement records. They remain manual recovery cases. Missing/changed immutable
inputs, helpers or required backups also block guided recovery. Keep all data and
extract a complete distribution into a new directory, then transfer user data
explicitly, or perform a careful manual restoration. Do not delete unknown or
modified files to force a green check. Never clear the marker until installation
completeness is established.

## Verification

Existing opt-in core fixtures now source-specify plan creation through a real
ordinary-failure rollback, read-only preview, known replacement removal, absent-
original restoration, altered plan rejection, cancellation, stale review, modified
destination/backup rejection, all-instance exclusion, interrupted-recovery resume,
retained backups/user files, repeated recovery and legacy-format refusal. These
sources are MSVC /Zs checked only; no native fixture, helper or app is launched.
Additional opt-in fixtures specify allocation/ownership estimates, ignored supplied
space observations and all-copy preparation before a locked first destination,
with unchanged targets/marker and retained plan-bound space receipts. Sixteen new
constexpr assertions evaluate actual recovery budget bounds without executing
fixture bodies. Twelve further assertions evaluate actual resume headroom, including
full/partial reuse, over-deduction, invalid counts, zero-byte files and overflow.
Opt-in fixtures now specify mixed-directory reuse across retries, advisory phase
rejection, changed-copy fallback/preservation, foreign/missing work identity,
traversal/hardlink refusal and mandatory original backups. These native bodies
remain unexecuted. Five executed authoring tests use in-memory ZIP/JSON/hash/PE-header data only.
Sixteen additional constexpr assertions evaluate phase-local progress, zero/invalid
totals, upper bounds, rounding, postflight indeterminacy and all eight phase codes.
Native source fixtures specify typed pre-cancellation, per-attempt progress ownership,
failed/successful receipts, copied-report exclusions, actual copied-byte counts and
zero fresh-copy bytes on a reused preparation. Those bodies are not run. Production
build/resource inspection is recorded separately
in [Validation](Validation.md); actual recovery acceptance remains pending.

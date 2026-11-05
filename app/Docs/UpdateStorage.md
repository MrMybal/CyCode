# Reversible update-cache management

The Application updates view now includes Update storage: explicit background
inspection, reviewed retirement of obsolete download payloads and guarded cache
restoration. This reorganizes the active cache; **it does not free disk space**.
There is no permanent deletion or automatic retention policy. Installation recovery
backups, user settings/projects, current/newer releases and unknown/legacy content
remain protected. Native filesystem/lease/GUI acceptance is pending.

The separate [detailed history](UpdateHistory.md) inspector reads retained prepared/
outcome metadata even after owned payloads move away. It does not reverify payloads,
change retirement eligibility or authorize restoration. Recovery receipts remain
protected and advisory.

## Desktop workflow

1. Open View > Application updates > Update storage, then Inspect update storage.
   Nothing runs automatically at startup or on every frame. Scanning/hashing uses
   a cancellable background future and the main loop retains its ordinary idle wait.
2. The list shows eligible older downloads, retired caches, protection counts and
   budget limits. Review retirement selects one download. The detail shows its
   version, source directory, payload names and recorded sizes. Only after checking
   I reviewed this cache action can Retire reviewed download run.
3. Payloads move into a unique data/updates/retired-* directory. Prepared metadata,
   installation outcomes/error logs and original directories remain in place, so
   ordinary update history stays available. No executable is started.
4. Inspect again, then Review cache restoration to inspect a retained receipt and
   its source/destination/file list. The same acknowledgement controls Restore
   reviewed cache files. Existing destinations, changed files and conflicts block
   restoration. Already-restored payloads need no second move.
5. Results show verified file/recorded-byte counts and the retained receipt path.
   Cancellation or a filesystem failure can leave a partial move; inspect its
   receipt again to restore through the same observed-state checks. No automatic
   retry, process termination, archive execution or workspace edit is added.

Controls use the existing CyUI/Core style and EN/FR application catalogue. The
native owner is part of cycode_updates, with no new target, external dependency,
CyUI module, SDK operation or plugin ABI. GUI, helper and core reuse native path,
manifest, staging metadata, outcome and non-overwriting move implementations.

## Ownership and compatibility

New downloads write prepared.json format 2, retaining the original installation,
version, archive name and release SHA-256 fields for compatibility with existing
staging readers. Its files object additionally records size/SHA-256 for the archive,
copied installed helper and optional staged translation catalogue. This is separate
from package manifest updateFormat 1, recovery pending format 2 and history format 1.
Archive/helper digests come from the release/installed manifest and ordinary final
verification still checks actual files before download success. Legacy downloads
remain readable/installable through existing guards, but are not eligible for
retirement because they lack this explicit cache ownership snapshot.

A retirement candidate must be strictly older than both the running application
and installed manifest. The installation must be Windows x64 portable, with a valid
managed manifest and no pending update. Development checkouts are refused. Only
the exact recorded payload names are eligible: the version/platform archive,
CyCodeUpdater.exe and optional updater-messages.json. All must be ordinary,
single-link files with the recorded sizes/hashes and safe ancestors. Unknown
files/subdirectories, missing/altered payloads, foreign metadata, active/waiting
outcomes and unsupported metadata formats protect the whole entry. Existing
history/error files are retained rather than moved or read into a report.

Inspection visits at most 128 direct entries and retains at most eight candidates
and eight retired summaries. Each inspection phase has a combined 512 MiB payload-
hash budget; archive/helper/catalogue caps remain 256/64/4 MiB. Metadata is separately
bounded (8 KiB prepared/outcome/status, 1 MiB retirement receipt, 2 MiB installed
manifest). Hash/cancellation checks operate in 64 KiB chunks. Rejections and
exhausted budgets are visible; no complete inventory or performance claim is made.
Protected transaction/recovery directories are not traversed or sized.

## Transactions and concurrent use

A mutating action acquires a shared installation lease and an exclusive cache.lock
lease, then repeats the selected review. The installed manifest digest, staged
metadata identity and payload snapshot must still match. New downloads, updater
handoff/helper execution and guided recovery hold a shared cache lease; maintenance
fails immediately if the cache is in use. Acquisition is fail-fast rather than a
blocking lock-order cycle. Independent metadata readers remain read-only; no
installation/source operation or permissions are exported through SDK/MCP.

Before a payload moves, retirement writes retirement.json into its unique retired
work directory. It binds the installation, original download directory/version,
prepared metadata SHA-256 and exact owned file records. status.json is written
atomically against its captured disk baseline; the immutable receipt never depends
on a moved-file counter. Payloads move without replacing existing targets and are
hashed again at destination. Source prepared/outcome/error records remain.

Restoration validates the receipt against the still-bound source metadata and
inspects both locations for each payload. Exactly one location must contain its
known bytes: already at source means unchanged; absent source plus verified retained
file means restorable. Both present, both absent, modified data, unsafe paths or
foreign/changed receipts block the reviewed action. Unknown retained directory
content also blocks the entry. The actual observed state, including status bytes,
forms the fresh-action guard. Final installed-root and full receipt postflight
precede a completed status. The receipt remains as an audit record after restoration.

An external edit racing a move may be retained in the retired directory while the
action stops; it is never silently overwritten or permanently deleted. A process
interruption releases leases and leaves receipt/status/payloads for a fresh review.
This remains a multi-file move, not a power-loss-atomic transaction. Permanent
pruning, incomplete/legacy cache cleanup, pagination beyond the scan budget and
automatic Linux/macOS maintenance are not implemented. Reversible retirement
continues to use disk space; manual removal requires a separate, informed decision.

## Verification

Opt-in native core sources specify read-only inspection, old/current/legacy and
unknown-content protection, active cache leases, changed payload/manifest/pending
state, cancellation, receipt changes, destination conflicts, partial restoration,
retained metadata/backups/user files and repeated-action rejection. Existing GUI
fixture sources assert test-session guards. These are compiler syntax checks only,
not executed native filesystem tests under the Kaspersky suspension. Source/data,
production build, PE resources and whitespace checks appear in
[Validation](Validation.md). No application/helper/test process is launched.

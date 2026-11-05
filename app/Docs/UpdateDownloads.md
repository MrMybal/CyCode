# Resuming interrupted application downloads

The Windows x64 portable updater now keeps interrupted transfer bytes. An explicit
**Download or resume update** action searches a bounded set of local checkpoints
for the exact release asset, resumes when the HTTP response permits it, and only
then creates the existing verified installation candidate. It never resumes a
network transfer or installation automatically at startup.

## Space checks and transfer observations

Before creating a download work folder or opening a transfer, CyCode checks caller-
available space on the installation volume. It repeats the check before creating
a fresh full-download fallback and again before staging helper/catalogue files.
The additional-space estimate is:

`remaining archive bytes + installed helper size + 4 MiB catalogue + 64 KiB journals + 32 MiB spare`

A resumed prefix is already on disk, so it is subtracted from the remaining archive
amount. A full-response fallback needs the entire archive in a new folder while
the earlier partial stays. Completed cached bytes still require staging headroom.
The helper allowance is conservative even when a staged helper already exists.
Bounds reject invalid sizes/offsets before arithmetic; totals stay within the
existing 256 MiB archive / 64 MiB helper limits. Nothing is preallocated or deleted.

Windows reads 64-bit bytes available to the caller through
[GetDiskFreeSpaceExW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw),
including per-user quota limits. An unavailable query stops with a storage error,
rather than pretending unknown space is zero or unlimited. Known insufficient
space has its own EN/FR failure guidance and retained observed/required counters.
These probes run in the existing worker, not on every rendered frame.

This is a download/staging preflight, not a reservation, filesystem allocation
promise or full installation/extraction/recovery budget. Other processes can
consume space afterward, allocation units can add overhead and a real write can
still fail. Existing write checks, guarded preparation and retained files remain
necessary. No pruning, antivirus change or automatic network retry is introduced.

Each attempt has a release-bound status object: immutable target version/size,
atomic phase, initial retained prefix, network-byte count, last required/available
space observation and full-fallback flag. The phases distinguish preparation,
receiving, SHA-256 verification, staging, verified readiness, cancellation and
failure. Reaching the byte total does not offer installation until the existing
complete verifier succeeds. Supplying mismatched telemetry context is refused.

The Updates view shows MiB, bytes received during this attempt, a sampled transfer
rate and approximate remaining **transfer** seconds. Retained prefix bytes never
inflate that rate. Samples update at intervals of at least half a second, smooth
successive rates and drop an estimate after more than three seconds without new
data. Changed/rolled-back clocks, reset counters and non-receiving phases reset
the meter. Invalid bounds or estimates longer than a day remain unspecified.
Hashing/staging/install time is excluded. The meter is main-thread arithmetic;
drawing multiple views does not resample it or query disk/network state.

Last observations stay explicitly labelled with their own target version, even
when later release checks change the current offer. The clipboard report includes
these bounded numeric counters, phase and fallback flag; it still excludes URLs,
credentials and raw exception text. Observations are not an atomic installation
snapshot or persistent transfer history.

## Explicit retry controls and feedback

The normal **Download or resume update** action selects the most advanced exact
matching checkpoint within the existing scan budget. **Start a fresh download**
passes an explicit native `DownloadMode::fresh`: selection returns without even
inspecting old checkpoints, then creates a new uniquely owned work directory.
No previous partial, prepared archive or metadata is deleted or truncated. This
can consume additional disk space. Both actions retain the same installation,
manifest, version, lease, response, size, SHA-256 and final preparation checks.
Fresh mode does not persist in preferences or become a new startup policy.

Native `DownloadError` carries a fixed category independently of its technical
message. Access, network, invalid response, storage, integrity, busy, installation
and other failures select EN/FR titles and next-step guidance through the shared
UpdateFeedback policy. System filesystem exceptions are treated as storage errors;
unclassified exceptions remain other. Cancellation stays neutral. No message
substring matching or automatic credential lookup beyond the existing environment
contract is added. Technical details are collapsed in the Updates view; they are
not copied into the update report.

After an interrupted/failed attempt, the desktop schedules exactly one bounded
local inspection. It never starts another transfer/check, restores installation
consent or reopens an update proposal. The original error/category/cancellation
notice remains visible. A failed or cancelled inspection has separate feedback;
a successful scan refreshes the partial summaries and can rediscover a fully
verified archive. A candidate with the same version but a digest different from
the current known release is discarded. Successful preparation already has its
verified candidate and does not trigger this extra inspection. Closing, smoke and
test-session guards exclude new post-attempt work. Manual Refresh remains available.

The explicit copy report records whether a transfer was attempted, its requested
mode, current failure code and observed progress. It includes no technical exception
text, asset URLs, token values or headers. These are current in-memory observations,
not persistent transfer history; existing helper outcome history remains separate.

## Ownership and checkpoint

UpdateDownload shares binding, local metadata and response validation with the
existing UpdateTransfer and UpdateCache owners. There is no SDK operation, plugin
ABI change, extra runtime, executable target or CyUI module. The SDK stays at 227.
Native automatic transport remains Windows x64 only.

The installation must be a current portable distribution, with no development
checkout or pending installation. Its managed manifest is freshly validated and
the candidate must be newer than both installed and running versions. A shared
installation lease prevents replacement while downloading; the shared cache lease
excludes cache retirement. A fail-fast exclusive `data/updates/download.lock`
serializes all CyCode download writers for that copy, across desktop instances.
No other instance is terminated or waited on indefinitely.

Each new `download-*` directory starts with an atomic `resume.json` (format 1),
binding installation, version, exact platform/name, numeric repository asset API
URL, expected size and GitHub SHA-256. Neither tokens, authorization headers,
redirect URLs, signed query strings nor release notes enter this record.

Discovery reads at most 128 top-level entries and five entries per partial folder.
Metadata is capped at 8 KiB; archives at 256 MiB, helpers at 64 MiB and optional
message catalogues at 4 MiB. Relative/foreign paths, unknown files, symlinks,
reparse points, special files, hard links, oversized payloads and ambiguous
archive-plus-partial states are rejected without removal. Completed prepared
folders and digest-rejected checkpoints are not transfer candidates. The most
advanced matching partial is selected; exhausting the scan can mean starting a
new download while older bytes remain elsewhere.

The actual ordinary file size supplies the offset, rather than a trusted saved
counter. The Windows writer opens without truncation, verifies native file type,
link count and exact length, and prevents other writers/deletion while open.
Failures and cancellation close the handle and retain received bytes. Flushing
on interruption is best effort; a checkpoint does not guarantee power-loss
durability or imply a verified prefix. Only the final complete digest is trusted.

## HTTP response policy

Each attempt starts again at the repository's numeric API endpoint, obtaining a
new redirect when needed. Tokens remain restricted to that repository's API,
and HTTPS/host/redirect/deadline/size guards remain. GitHub documents both direct
streaming and redirected downloads in its
[release asset API](https://docs.github.com/en/rest/releases/assets).
Header retrieval follows the system
[WinHttpQueryHeaders contract](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpqueryheaders).
These documents do not establish live access or range behavior for this private
repository; that still needs actual network acceptance.

For an offset greater than zero, the request sends `Range: bytes=OFFSET-` and
`Accept-Encoding: identity`. Before consuming any body bytes, the final response
must be 206 with one exact `Content-Range: bytes OFFSET-(SIZE-1)/SIZE` and, when
present, matching Content-Length. Encoded bodies, duplicate/malformed headers,
wrong offsets/end/total, wildcards, overflow and unexpected statuses are refused.
A full zero-offset 200 response is also allowed, with exact recorded size checks.
Every stream is bounded to remaining bytes; EOF must produce the complete size.

A valid full 200 response to a nonzero range means the server ignored Range.
CyCode consumes none of that body into the old partial. It retains the old folder,
creates a new owned folder, and makes one full-download fallback attempt. This
can occupy additional disk space. It does not truncate or replace the original.
A network error or cancellation does not automatically loop/retry.

When all bytes are present, including a previous interruption after transfer,
CyCode recomputes SHA-256 over the whole archive before renaming/staging. A digest
failure retains the file and atomically marks its checkpoint rejected, so the next
explicit download starts elsewhere. A correct full archive can finish staging
without another network request. Existing helper/catalogue files are not blindly
overwritten; changed staged helper data stops staging and rejects that checkpoint
so the next explicit retry can start elsewhere. The normal prepared verifier
still rechecks archive and installed/staged helper before presenting installation.
Only the unchanged owned resume marker is removed after successful preparation;
cleanup failure leaves a protected record. Histories, transaction backups and
user files retain their existing owners.

## Desktop observations and verification

Startup and **Refresh downloaded updates** discover bounded partial summaries
without network requests, hashing their prefixes or creating leases. At most
12 summaries are retained, separately labelled incomplete, with observed/expected
byte counts and their folder paths. The copy report includes these counts/paths,
not asset URLs or secrets. Failed/cancelled attempts trigger one local refresh;
observations can still become stale, and manual Refresh reads them again.
A release check obtains the current exact asset binding before a restarted app
can offer a transfer. Download cancellation describes retained bytes, progress
starts at the resumed offset, and smoke/test sessions cannot start transfers.
The existing explicit install/save/close workflow is unchanged.

Thirty compiler-evaluated assertions cover production policies: nine headroom
bounds/offset calculations, two multi-case meter contracts and nineteen HTTP
response assertions. The latter cover: full/partial acceptance,
ignored-range fallback, malformed/overlapping/incomplete ranges, size/header
errors, encodings, overflow and bounds. No test executable is required for those
assertions. Additional opt-in native source fixtures describe checkpoint identity,
readonly observations, caller-space observation/retention, replaced asset/size/digest, unknown files, size limits,
interrupted staging, rejection markers and foreign metadata. Retry follow-ups add
most-advanced/exact-identity selection, fresh-mode noninspection/retention, cancelled
selection and typed response errors. Injected-future GUI sources cover quiet local
refresh, preserved feedback, replaced digests and cancelled/failed inspections.
They are source
checked only, not linked or run under the Kaspersky suspension.

Production builds and data-only packaging/source checks do not establish real
WinHTTP headers, private credentials, interruption/resumption, filesystem locking,
GUI/progress/cancellation or clean-machine behavior. Those acceptance checks,
Linux/macOS builds and power-loss durability remain unverified. See
[Validation](Validation.md), [Application updates](Updates.md) and
[Update storage](UpdateStorage.md).

The helper now applies a separate allocation-aware extraction/pre-replacement
[installation budget](UpdateInstallation.md). Download observations still do not
reserve space or guarantee installation/recovery writes.

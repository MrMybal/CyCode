# Application updates from GitHub Releases

CyCode checks the stable Releases of [MrMybal/CyCode](https://github.com/MrMybal/CyCode/releases)
in the background after a bounded local cache inspection, starting 1.5 seconds after desktop startup. It offers a newer version
without downloading or installing it automatically. The preference defaults on;
View > Application updates can disable it, check manually, open the release,
postpone a proposal or skip a specific version. Manual checks can review a skipped
version. Preferences remain in data/settings/ui-preferences.json, with no credentials.

The initial milestones cover release discovery/presentation, verified download,
explicit portable replacement/restart and compatible release authoring. Follow-up
milestones add verified restart discovery, persistent attempt history and clearer
cancellation/retry states.
Native update services are independent of the workspace engine, SDL, ImGui and
CyToolsCore. No workspace MCP operation or plugin installation permission is added;
the SDK currently has 252 operations. CyUI's existing Core controls provide the UI;
no new CyUI module or third-party runtime is selected.

## Desktop flow

1. An asynchronous check reads the latest stable release. Exact numeric version
   comparison accepts `vMAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH`, without leading
   zeroes or prerelease/build suffixes. Each component is at most 65535. Drafts,
   prereleases, equal versions and downgrades are never proposed as stable updates.
2. The proposal shows the new version and bounded plain-text release notes. Later
   dismisses it for this run; Skip this version persists the suppression. A new
   version is proposed again. Startup connection errors remain in the Updates view
   rather than interrupting editing. No failed check is reported as up to date.
3. Download or resume update starts one bounded background transfer, with progress and
   cancellation. A missing compatible asset or GitHub digest leaves the release
   available through its browser link. Cancellation is observed between bounded
   network operations; it is not instantaneous inside a blocking WinHTTP call.
4. Install update and restart is a second explicit action after successful SHA-256
   verification. It uses the existing save/discard/cancel close flow, including
   unsaved documents. Cancelling that close disarms installation immediately.
5. Only after that flow accepts closing does the desktop launch the staged
   CyCodeUpdater.exe. A launch/validation failure keeps the current UI open.
   The helper waits up to 120 seconds for all GUI/service instances of this copy
   to release their shared installation lease. It never terminates another process.
   CyCode's ordinary teardown still owns language servers, builds and sessions.
6. After installation and lease release, the helper starts the new CyCode normally.
   Existing saved preferences/workspace/session restoration retain ownership of
   reopening the project. A restart failure leaves backups and reports the failure.

Automatic native checking/download/installation currently supports Windows x64.
Other builds compile the shared model/guards and offer GitHub Releases manually;
automatic Linux/macOS transport/packages/installers are not implemented. GUI tests,
smoke sessions and the CLI do not start update network requests. The CLI does hold
an installation lease while serving native workspace operations.

## Repository and transfer contract

The endpoint is fixed to `https://api.github.com/repos/MrMybal/CyCode/releases/latest`.
The default stable selection follows the documented
[GitHub Releases API](https://docs.github.com/en/rest/releases/releases#get-the-latest-release).
Installed copies select `CyCode-VERSION-windows-x64-setup.exe`; portable copies
select `CyCode-VERSION-windows-x64.zip`. Legacy `-update.zip` names remain accepted
for portable cached/release data, but are no longer produced. Assets must be
uploaded, have a positive size no greater
than 256 MiB, and belong to this repository's numeric release-asset endpoint.
At most 1024 assets and 4 MiB of release metadata are accepted. Notes are capped at
32 KiB on a UTF-8 boundary. Only one cache inspection, check or download runs in a desktop instance.
There is no timer that continually polls unchanged metadata or automatically retries
an error. Startup checks run once per process; GitHub rate limiting is reported.

GitHub's `digest: sha256:...` field is required for an installable asset; it is
documented in the [release asset API](https://docs.github.com/en/rest/releases/assets).
The downloaded byte count and SHA-256 must match before staging. The helper hashes
the archive again before extraction. SHA-256 verifies integrity relative to the
authenticated GitHub metadata; it is not an independent publisher signature.

The UI caches advisory installation mode instead of parsing the manifest every
frame; download/launch/apply retain fresh native guards. Network work does not
remove the main loop's normal idle frame wait. No performance benchmark is claimed.

The Windows transport uses WinHTTP and normal certificate validation, HTTPS/443
only, with bounded connect/send/read timeouts and overall deadline checks.
Redirects are handled explicitly, with at most six requests. Only api.github.com,
github.com, release-assets.githubusercontent.com and objects.githubusercontent.com
are accepted. Cookies and implicit authentication are disabled.

For a private repository, set one of the following in the environment before
starting CyCode, in priority order: CYCODE_UPDATE_TOKEN, GH_TOKEN, GITHUB_TOKEN.
An appropriate GitHub token needs access to this repository's Contents (read).
Do not put it in a project, preference file, release asset or chat. It is sent
only as an Authorization header to this repository's API endpoints, never forwarded
to asset/CDN redirects, saved in preferences, placed in argv or printed in logs.
Public releases need no token. Browser login alone does not configure WinHTTP.
404 means no accessible release/repository, not a successful up-to-date result.

## Installation, ownership and recovery

Use [detailed update history](UpdateHistory.md) for explicit metadata inspection,
text/category filters, twelve-row pages and recorded recovery details. Selected
entries or retained snapshots can be copied without reading archive/backup payloads.
Scan limits and unknown dates remain visible; observations do not verify the current
installation or authorize recovery.

In-place installation requires an update-enabled portable package with
manifest.json `updateFormat: 1`, matching platform, and both CyCode.exe and
CyCodeUpdater.exe. A development checkout containing .git is never replaced.
Legacy distributions remain able to check/open a release, but require a first
manual extraction of a current portable package. The installation folder must be
writable, as with the existing portable settings layout.

The helper is copied from the installed package into a unique download directory
in data/updates. Its hash must match the installed manifest before copying and
before launch. No new updater is executed directly from an unverified download.
The helper and the archive must share the bound staging directory; paths and
version arguments are passed directly to CreateProcess, without a shell.

The update ZIP is a stored, single-volume ZIP with no comments/ZIP64/data
descriptors, at most 4096 regular files, 64 MiB per file and 256 MiB total. Its
single root must be `CyCode-VERSION/`. The parser validates central/local header
agreement, CRCs, data extents, case-insensitive duplicate/conflicting paths and
ordinary file types. Traversal, absolute/drive/ADS paths, Windows device names,
symlinks/reparse points, hard-linked existing targets and protected data/runtime/
.git path components are rejected. Archive entries must exactly match the manifest
plus the manifest itself. Every extracted file's size/SHA-256 is verified first.

Only the union of the previous and new manifests' owned files can change. Unknown
files, separately installed extensions, projects, data, settings and recovery data
remain outside that set. Changed package-owned files, including documentation or
bundled providers, block replacement instead of being overwritten silently. Missing
owned files may be repaired; an unknown existing file at a new target blocks it.
Layout/case-only path conflicts may require a manual fresh extraction.

Allocation-aware [installation space checks](UpdateInstallation.md) precede
extraction and replacement, with retained observations and distinct failure codes.
Extraction finishes before originals move. The helper writes a journal and owned pending
marker, moves originals into a transaction backup on the same volume, installs the
new managed files, and verifies the installed hashes including manifest.json.
An ordinary exception triggers reverse restoration, retaining a modified/conflicting
target for manual recovery rather than deleting external changes. Successful
updates keep backups and a completed journal in data/updates/transaction-*.
A rolled-back outcome requires full original presence/size/hash verification;
pending-marker removal requires exact bytes and an independent opened file.
Preparation failures do not claim an incomplete rollback when a journal write fails.

This is a multi-file transaction, not a crash-atomic directory swap; power-loss
durability is not guaranteed. Interrupted replacement retains pending.json,
transaction journals and backups, and blocks ordinary startup. New format-2
transactions now support [guided recovery](UpdateRecovery.md): the staged helper
previews the exact baseline and required/available recovery space, asks for
confirmation, then stages all guarded copies before moving installed targets.
It revalidates context and commit headroom while retaining original backups.
A retry can reuse complete verified preparations from this active transaction
through bounded source maps, while retaining changed or incompatible earlier data.
The helper now shows phase/count progress, supports cooperative stop requests and
keeps the worker joined before closing. Result dialogs offer an explicit copied
report; plan-bound result receipts are best effort. These observations never replace
fresh validation. Real native/GUI acceptance remains pending. It does not launch the full IDE
through the pending block. RECOVERY.txt identifies the staged helper.

Legacy/damaged transactions remain manual cases. Preserve pending.json and all
transaction/recovery files; restore carefully or extract a complete distribution
into a new directory before transferring user data. Clear the guard only after a
complete installation is established. There is no unattended restoration or cache
pruning. Interrupted transfers retain bound partial downloads for explicit retry;
completed verified downloads can be rediscovered. See
[Download resumption](UpdateDownloads.md) for exact range/fallback and integrity rules.

## Reversible storage management

The Updates view now offers explicit [Update storage](UpdateStorage.md) inspection,
reviewed retirement of new-format obsolete downloads and receipt-backed restoration.
Payloads move outside the active cache while metadata/history remain. Shared cache
leases protect downloads/helpers/recovery. Backups and current/legacy/unknown data
remain protected. Retirement retains disk usage; permanent pruning is not added.

## Download space and progress

The existing download worker now checks conservative additional-space headroom
before transfer, before a full-download fallback and before staging. The view
reports last observed caller space, a distinct low-space failure and separate
receiving/hash/staging/readiness phases. It also shows network-only rate and an
approximate remaining transfer time, excluding the retained prefix and later
verification/installation. No per-frame filesystem probe or space reservation is
added. The clipboard report contains bound numeric observations and phase codes.
This is not a full installation/extraction space guarantee; see
[Download observations](UpdateDownloads.md) for exact budgets and limitations.

## Interrupted HTTP downloads

Explicit download actions can resume only a checkpoint matching the current
release version, exact asset API identity/name/platform, size and SHA-256.
Partial summaries are observations, never installation candidates. A shared
installation/cache lease and exclusive per-copy download lease protect staging;
validated 206 ranges precede any append. If a full response ignores the range,
the previous file remains and one full transfer starts in a fresh folder.
Cancellation/error retain received bytes, and complete SHA-256 is mandatory before
normal preparation. Startup never resumes network transfers automatically.
The fresh-download action bypasses existing checkpoint selection without removing
old files. Categorized EN/FR failure guidance and optional technical details explain
retry choices. Failed/cancelled attempts trigger one quiet local refresh that keeps
their result; it never retries the network or reopens a proposal. Copy reports
contain requested mode/category/progress rather than raw exception text.
See [Download resumption](UpdateDownloads.md) for records, budgets and limitations.

## Verified downloads across restarts

Startup inspects local staging asynchronously, even when network checking is
turned off. Turning off the startup preference also suppresses automatic cached
proposals. The Updates view exposes Refresh downloaded updates independently of
GitHub access. It neither creates work directories nor changes cached files.
A network check retains a valid prepared download. A changed digest reported for
the same release invalidates the old in-memory candidate and requires downloading
again. If GitHub reports no newer stable release while a cached candidate remains,
the UI describes that difference instead of claiming the running copy is latest.

Discovery accepts only the installation's own data/updates/download-* directories,
with bounded numeric directory names, exact version/platform archive names and
bound prepared.json metadata (8 KiB maximum). It checks that the version is newer
than both the running application and installed manifest; it rejects development
copies and pending installations. The archive and both staged/installed helpers
must be ordinary, single-link files without reparse/symlink ancestors. Helper
sizes/hashes must match the current installed manifest; the archive SHA-256 is
recomputed. No new helper from inside an archive is executed. The same verification
is used after downloading and again immediately before installer launch.

The scan visits at most 128 directory entries, orders candidates by numeric version,
verifies at most four candidates with a combined archive budget of 512 MiB, and
keeps at most twelve attempt summaries. Helper hashing is separately bounded to
64 MiB per file. Streaming limits also reject growth past the reserved budget,
not only oversized inputs observed before hashing. Cancellation is observed between entries and each 64 KiB hash
chunk. Rejected entries and exhausted budgets are visible; no completeness claim
is made after reaching a budget. Invalid/foreign/corrupt entries remain untouched.
Metadata/archives are not scanned or hashed on every rendered frame.

Restoring a download never restores installation consent or the quit request.
Install update and restart still goes through ordinary document save/close and
all-instance exclusion. A failure at desktop handoff keeps the UI open and clears
the candidate; refresh or a new download can revalidate it. Cancelling network or
local inspection produces a neutral cancelled state, without an up-to-date result.
No request is started or proposal raised by a new startup operation during closing.

## Persistent attempt history

The existing helper writes a small result.json beside prepared.json, using the
shared atomic text-file writer with a disk baseline. It records format 1, bound
version/archive digest, state and Unix timestamp. States distinguish waiting for
instances, installed, failed and installed-but-restart-failed. Outcome reporting
is best effort and cannot turn a successful installation into failure. A power
loss can leave a waiting record; the UI labels it as waiting or interrupted,
without guessing whether a helper is currently alive.

The Updates history reads these records with the same path/size/binding guards.
Failure records keep a valid archive available for an explicit retry. Successful
old-version records remain visible after an upgrade while their packages stop
being installation candidates. The existing update-error.txt contains bounded
local helper error details. Copy update report places versions, states, timestamps,
local staging paths and candidate SHA-256 on the clipboard only after a click;
it does not read environment credentials, release URLs or error-file contents.
Invalid records are counted; a refresh does not delete logs, archives or backups.

These outcomes are diagnostic summaries, separate from transaction journals.
They do not recover a partially replaced installation or bypass pending.json.
The guarded startup block remains; the separate guided-recovery service consumes
new format-2 transactions. Legacy recovery remains manual.

## Authoring a compatible release

Use a clean source checkout with the selected release version consistently set in
CMake and app/CyTool.json. Build Windows Release with BUILD_TESTING=OFF; the CyCode
target includes the production CyCodeUpdater dependency. Then run:

```powershell
python app/scripts/package_windows.py --build runtime/build-modules-clean --configuration Release
```

The packager checks AMD64 PE32+ executable headers and reads the three executables'
PE version resources through system APIs;
it never executes CyCode or cycode_cli for version detection. It refuses dirty
source, missing/mismatched versions, private/unsafe entries and oversized payloads.
The manifest records owned paths, sizes, SHA-256, source revision, license and
pinned dependency identities. It produces a normal compressed human-download ZIP,
a deterministic stored `CyCode-VERSION-windows-x64-update.zip`, and SHA256SUMS.txt.
Both distributions contain the same update-enabled manifest and production helper.

Publish a stable GitHub Release tagged `vVERSION` (or VERSION) with both archives
and SHA256SUMS.txt attached. GitHub supplies the asset digest used by the client;
verify that it matches the locally generated checksum. A source-only release or
one containing only the normal ZIP can be viewed but cannot be installed by this
updater. Building/packaging does not publish or upload anything. This task has not
created a commit, tag or release and has not pushed.

## Validation and remaining acceptance

Five pure Python tests exercise in-memory archive authoring, exact file hashes/sizes,
stored compression, reproducible bytes, PE-header rejection, versions and protected/colliding
paths without writing or launching EXE fixtures. Native regression sources extend
existing opt-in core/module targets: release parsing/digest/URL/platform guards,
rollback on a locked file, successful replacement preserving user data, repeated
version rejection, lease exclusion, ignored/manual proposals, test-session I/O
guards and cancelled install intent. Follow-up sources cover cached rediscovery,
retryable history, cancellation, corrupted helper/archive, foreign metadata, pending
markers, outside paths, retained downloads and replaced asset digests. These are
MSVC /Zs checked, not linked/run.
The existing CI matrix gains the Python data-only tests; it has not been run here.

Production compile, source/schema checks and PE-resource reads
are recorded in [Validation](Validation.md). Native HTTP/download/helper/restart,
filesystem/rollback, multi-instance timing, actual GitHub private access, GUI and
clean-machine acceptance remain pending under the existing Kaspersky suspension.
No app, helper, server, SDK Runtime or native fixture is launched during this work.
The connected GitHub service could not read the repository (404), so no claim of
an authenticated live-release check is made.


## Windows installer and shared portable payload (0.2.3)

Build and compose both formats from a clean source checkout:

```sh
python app/scripts/fetch_dependencies.py
python app/scripts/build.py --distribution --no-install
python app/scripts/package_windows.py --build runtime/build-vs
```

Inno Setup 6.7+ is required; `--iscc PATH_TO_ISCC` selects its compiler. The
distribution build omits optional language/debug toolchain downloads. The package
contains app-local MSVC runtime DLLs and the complete offline CyDoc manual. These
runtime DLLs retain Microsoft's separate terms. Outputs are the installer,
portable ZIP and SHA256SUMS.txt under runtime/releases. The portable ZIP uses
stored entries so the existing native archive validation/transaction can read it.

Setup installs for the current user under `%LOCALAPPDATA%/Programs/CyCode`, adds
Start menu/optional desktop shortcuts and uninstall registration. Settings and
optional tools remain in its writable data/runtime directories and are retained
by uninstall. The setup-owned installed.flag is deliberately outside the portable
managed manifest. Setup/uninstall hold the native installation ownership file
exclusively, refuse a pending recovery, and never forcibly close a running copy.

Installed copies download verified Setup, close through the ordinary save flow,
then hand ownership to Setup and validate the resulting manifest version before
restart. Setup maintains its uninstall/version registration. Portable copies
retain the native managed-file transaction and recovery/rollback. Installed
package modifications are checked before handoff; changing a managed file blocks
the in-app installation until reviewed. The staged helper receives hash-checked
copies of its app-local runtime, with that space included in transfer preparation.

Versions through 0.2.2 only know the historical asset name. Install/extract 0.2.3
once to adopt the edition-aware flow; no older release asset is replaced.
Installer execution and the full online update cycle remain unverified locally
while native acceptance runs are suspended. CI runs native model contracts on
its own runners; those checks do not prove Windows desktop/installer acceptance.


## Consumer-specific update identity {#products}

`CyCode::Updates` is one native static library shared by the application and its
helper. Its identity is immutable for that target graph; no workspace, preference,
cache or network response can switch the product. Configure before adding Core:

```cmake
set(CYCODE_BUILD_UPDATES ON)
set(CYCODE_UPDATES_PRODUCT CyCodeIDE)
set(CYCODE_UPDATES_REPOSITORY MrMybal/CyCodeIDE)
set(CYCODE_UPDATES_VERSION "${PROJECT_VERSION}") # The host version, captured before Core's project().
add_subdirectory("${CYCODE_SOURCE}" "${CMAKE_BINARY_DIR}/core")
target_link_libraries(CyCodeIDE PRIVATE CyCode::Updates)
target_link_libraries(CyCodeIDEUpdater PRIVATE CyCode::Updates)
```

The three CMake variables and corresponding `cycode_updates` target properties
are the exact contract. Product identifiers are safe ASCII basenames (up to 64
characters), repositories are bounded `owner/repository` references, and versions
are numeric MAJOR.MINOR.PATCH with components at most 65535. Nonhistorical products
must supply their repository and host version explicitly. Do not reuse a library
already configured for another product; build separate application distributions.
Read `cycode::updates::productIdentity()` from `cycode/Updates.h`: `product`,
`repository`, `currentVersion`, `application`, `helper`, `installationMarker`,
`installationMarkerText`, `tokenEnvironment`, `requiredFiles` and
`acceptsLegacyMetadata`. Existing update operations retain their signatures.
The exported `updates::repository` now names this configured repository.

For this IDE profile the names are **CyCodeIDE.exe**, **CyCodeIDEUpdater.exe**,
**CyCodeIDE-installed.flag** with the exact bytes `CyCodeIDE per-user installation\n`,
and **CYCODEIDE_UPDATE_TOKEN** (then GH_TOKEN / GITHUB_TOKEN). Releases come only
from MrMybal/CyCodeIDE; assets are `CyCodeIDE-VERSION-windows-x64.zip` or, for an
installed copy, `CyCodeIDE-VERSION-windows-x64-setup.exe`. The stored ZIP root is
`CyCodeIDE-VERSION/`. Shared library configuration does not create the GUI helper:
the host supplies its UI, close/consent flow, installer and normal restart target.
Automatic update transport remains Windows x64; no new workspace MCP operation
is added to the 252-operation catalogue.

New manifests retain `updateFormat: 1`, platform/version/files and add exact
`product` and `repository` fields. New products require both. Matching identifiers
also bind partial/prepared downloads, outcomes, pending installation/recovery
journals, prepared recovery data, retirement records and history. Different
products/repositories, foreign executable names, ZIP roots or installation markers
are refused before replacement; foreign cache records are rejected/protected.
SHA-256, exact managed file sets, path/case/type bounds, leases, rollback and disk
budgets remain in force. New profiles minimally require their own executable,
helper, LICENSE and COPYRIGHT in the manifest. Their host additionally validates
the actual SDK, fonts, providers and documentation it distributes. `extensions/`
and `tools/python/` remain permitted managed paths; `VERSION.md` and VERSION.fr.md
are accepted. Private data/runtime/working caches remain excluded.

With no identity options, the historical MrMybal/CyCode profile retains its old
names, installed.flag bytes, required managed files, token name and compatibility
with unlabelled legacy metadata. Explicit foreign identity fields are refused even
there. A fork repository is a distinct identity and cannot opt into this legacy
exception merely by using the same product basename.

The pure `app/scripts/release_package.py` helpers accept a frozen
`UpdateProduct('CyCodeIDE', 'MrMybal/CyCodeIDE')` via keyword `product`:
`package_path`, `required_files`, `payload`, `validate_payload` and `write_archive`.
Pass host-specific requirements via `required` to payload/validation/archive.
They preserve all supplied providers/MCP bytes, validate manifest hashes/identity
before opening an archive destination, and use the product ZIP root. Use
`update=True` for the stored portable ZIP consumed by native Apply. They do not
execute any packaged code. `windows_installer.compile_installer` accepts the same
`product`, `required` and an explicit host-owned `script` (.iss). It stages the
correct marker and supplies UpdateProduct, UpdateRepository and InstallMarker
preprocessor values. A nonhistorical product cannot silently use CyCode's installer
script/registry identity. The host owns its Inno Setup script and AppId.

The production CyCode and CyCodeIDE update libraries compile on Windows x64.
`verify_update_profiles.py --cmake <cmake>` checks configuration without a compiler,
network, application or fixture executable. Data-only package tests use in-memory
ZIPs. Native profile/foreign-manifest regressions are added to the existing test
suite but are not executed here. Network checks, actual installer/apply/recovery,
GUI acceptance, release publication and Linux/macOS execution remain separate.

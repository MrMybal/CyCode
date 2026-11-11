# Windows antivirus investigation

## 2026-11-10 - sibling temporary names in the Core preview

Atomic text saves now create an independent sibling temporary file rather than
extending the destination's name. Exclusive creation, flushing and atomic
publication remain in place. The same naming policy is applied to CMake/MSBuild
metadata and local endpoint descriptors. Opt-in private reproductions are excluded
from normal production builds (CYCODE_BUILD_PRIVATE_DIAGNOSTICS defaults OFF).

The corrected production CLI passed official SDK/MCP search and clangd navigation
on CyCodeIDE and twelve synthetic save/checkpoint cycles, including stale-revision
rejection and shutdown without unintended save, with Kaspersky active. Earlier
bounded controls on one sample associate the long target-derived temporary name
with the observed classification. This is an empirical compatibility correction;
the vendor's internal rule and a general false-positive verdict remain unknown.
No protection setting or quarantine restoration is required by the package.

The historical investigation below predates this correction. Its statements about
pending authorisation, missing GSI or an unsent report describe those earlier steps,
not the current delivery state. Current setup and update behaviour is documented
in [Core installation](CoreInstallation.md).

## Local GSI collection completed — 2026-11-10

The owner explicitly authorised the official GSI administrator/UAC collection and
its documented network diagnostics after the automatic review requested precise
privacy/elevation consent. The original-path Core remains held and was neither
built nor run. The official GSI 6.2.2.110 executable's Authenticode signature is
Valid (AO Kaspersky Lab), with a recorded download hash; no quarantine, protection
or installation change was made. The collector exited 0. The local ZIP's hash,
central directory, bounded size and all entry CRCs were verified without extraction
or exposing private entry names/content. The archive is ignored by Git.

GSI contains private system inventory and Windows logs. It stays local and separate
from the minimised vendor-review draft, which now references only its availability,
hash and size. No support ticket, data upload or vendor submission was made.
The [official GSI procedure](https://support.kaspersky.com/common/utility/3632)
was used with its documented silent/output/event-log options. GSI is diagnostic
information, not a verdict or an antivirus fix. The original detected sample is
still deleted; detailed vendor tracing has not been enabled and KSN/KPSN connection
status remains unestablished. A completed KsnqTask alone is not a connection test.

Offline review also compares 22 retained Core avp events across 10 exact sample
hashes and four database stamps: their opaque Number is consistently 369941,
without interpreting that undocumented value as a specific rule. The latest full
control has five atomic writes (four publications, one index replacement), then
one obsolete-text removal; no temporary-path cleanup ran. Distinct case roots and
Core versus host initial-publication histories remain explicit confounders.
All 154 original dirty-workspace files and 254 native-source hashes remain intact.
See [the local diagnostic result](Benchmarks/gsi-local-report-20261110.json).

## Four-case campaign completed — 2026-11-10

The owner resumed the prepared four-case comparison. One production build and
exactly four native cases used the same executable at its original path, SHA-256
`d7e08f48392b84059f1f35da6b2a86d2bf93dcde156fa33a1971425f9bcd03eb`.
No protection, compiler option, sample location or production persistence decision
changed. Every case verifies the complete resulting file/hash map and unchanged
source baseline. The three isolated cases use host-preseeded synthetic cache files.
Within the authorised complete-sequence case, the unchanged files-only reproducer
also performs its initial cache publications, providing the prior positive control.
This refinement adds no fifth case, source change or rebuild.

| Case | Actual effect | Trace records | Result |
| --- | --- | --- | --- |
| Write only | New text published; index, original base and old text unchanged | 12 | PID 62308, exit 0; no alert observed |
| Index only | Index replaced; both texts/base unchanged | 14 | PID 56812, exit 0; no alert observed |
| Remove only | Old text removed; current index/text/base unchanged | 2 | PID 24652, exit 0; no alert observed |
| Complete files-only sequence | Initial Core publication, then new text, index replacement and old-text removal | 64 | PID 45656, exit 5; matching avp 22418621/22418622 |

Passing cases completed their 20-second live pre/post-operation and 20-second
post-exit observations plus passive publication monitoring. The full sequence
also completed its 20-second initial observation. Its first matching AV event at
14:19:48.7476635Z was 155.2395 ms after successful old-text removal and 1021.8505 ms
before host-requested shutdown. Official System Watcher reports bind the actual
PID to PDM:Trojan.Win32.Generic / Analyse comportementale, then termination and
deletion. The event reports database release 2026.11.10 09:48:00:000 and opaque
Number 369941; that number is not assigned an undocumented rule meaning here.
All traces are complete, with no failed/open span, cap or clock regression.
Postflight finds no Core process or executable; Kaspersky remains active.

The full process/file history reproduces the alert on the same sample for which
the three isolated effects pass. This narrows the evidence to the observed
sequence/context; it does not prove one API, a particular anti-ransomware rule,
every ordering dependency or a false-positive verdict. Host-preseeded controls
deliberately lack Core initial-publication history, and a bounded non-detection
is not a clean verdict. **No production antivirus correction is accepted.**

The authorised campaign is complete: one build, four cases, no fifth launch or
post-alert rebuild. All 154 original dirty-workspace files and 254 native-source
hashes are preserved. See [the measured campaign](Benchmarks/recovery-operations-20261110.json).
Earlier pending-approval preparation below is archived history and superseded
by this result; its four-case authority is now consumed.

### Vendor-analysis path, prepared locally

The [official false-detection procedure](https://support.kaspersky.com/kaspersky-for-windows/21.27/troubleshooting/other/1870)
specifically requests a GSI report for Windows PDM detections and may request
vendor trace files. The consulted online guide is version 21.27; the tested
installed product is 21.26.4.406. An unsent local draft now describes the exact
sample, selected verdict/PID events, controls, file effects and timing. It excludes
raw vendor history/Windows XML, private paths, accounts, source contents and any
binary. At the close of this native campaign, GSI had not yet been collected
and no diagnostic collector had been downloaded/run. The subsequent local
GSI collection is recorded above. KSN/KPSN status and any requested detailed
trace remain unestablished; no report has been submitted to the vendor.

Kaspersky also documents a [free developer Allowlist Program](https://www.kaspersky.com/partners/allowlist-program).
Its published requirements refer to a digital signature if available, alongside
site/company requirements; this is not a mandatory paid-certificate purchase or
a guarantee that this observed behavioural case will be accepted. No registration
or application-trust/protection change was attempted.

## Operation-by-operation diagnosis prepared — 2026-11-10

There is still no validated antivirus correction. The previous two-case owner
approval was fully consumed: both minimal cases reproduced detection and both
samples were removed. No third case or rebuild is covered by that finite scope.

The private `--diagnostic-file-operation` branch now isolates four exact effects
before Engine construction: `write` publishes the new text only; `index` replaces
only the index while retaining both payloads; `remove` deletes only the obsolete
owned text from an already refreshed fixture; `refresh` performs all three.
The host prepares each fixture before launch. Native readiness only reads and
validates the fixed marker, ordinary paths, known file names and exact contents;
one explicit stdin request then runs the selected effect. The harness validates
the complete resulting file/hash map, including all files which must stay intact.
No production persistence decision, public SDK/MCP method or compiler flag changed.

Both affected C++ units pass MSVC /Zs without objects, linking or Core execution.
Six data-only postcondition tests pass, including a checksum/serialization match
to the already captured native files-only index and negative mutation checks.
All 154 original dirty-workspace files are byte-identical. The campaign gate is
pending a new explicit owner resumption; no binary was rebuilt or run here.

Host-preseeded fixtures deliberately remove the Core's initial publication
history. A passing full refresh would mean this earlier history/context still
matters; it would not certify the application clean. Event times remain publication
times, and a relink after deletion may change the executable hash. The prepared
four-case comparison keeps Kaspersky active, stops each process on an alert and
requires evidence review before any separately approved continuation. No restoration,
relocation, exclusion, installation, upload, publication or GitHub Actions.
See [the bounded preparation](Benchmarks/recovery-operation-preparation-20261110.json).

## Current result — both minimal paths reproduce the behavioural alert

The owner explicitly approved two minimal cases and at most two production
rebuilds, including continuation to the second case after analysis of an alert.
Both cases have now run on Windows x64 with Kaspersky 21.26.4.406 active. Each
process stopped on its new alert; the official System Watcher report binds the
matching PDM:Trojan.Win32.Generic behavioural verdict to its actual child PID.

| Minimal branch in the original CLI | Core components constructed | Trace records / closed spans | First event after old-text removal | Result |
| --- | --- | --- | --- | --- |
| `--diagnostic-recovery-only` | Actual RecoveryStore; no Engine, Core worker, CoreService, SDK or MCP | 70 / 35 | 86.9839 ms | PID 75716, exit 5; avp 22418321/22418322 |
| `--diagnostic-files-only` | Production atomic writer and owned fixture files; no RecoveryStore instance/lease or services | 64 / 32 | 52.2373 ms | PID 113788, exit 5; avp 22418323/22418324 |

Both first observed the initial checkpoint alive for 20 seconds without a new
event. The second checkpoint really replaced the same document's index, removed
only its obsolete text, retained its original base and left its empty source
file unchanged. The traces contain no failed/open spans, truncation or clock
regression. The first alerts precede host shutdown requests by 296.8871 ms and
331.7307 ms respectively. Kaspersky subsequently deleted both executable samples;
later read-only postflight found neither a CLI process nor its executable.

The two relinks used identical 254-file native source manifests and unchanged
production compiler/link settings, but different executable hashes after deletion:

- RecoveryStore-only: `283f45d869c2416eb027c907e26b5b39031065db8334e058148d6c359f60b895`.
- Files-only: `ac4469a3d6c1cef29804b46bacb3743a7ec6a5f2b8a52c3252eac93343b55fb2`.

This is a reduced sufficient execution path **inside the original production
executable**, not a new independent test program or a same-binary A/B comparison.
MCP, the analysis engine and its workers are unnecessary for these reproductions.
Executable identity/reputation and prior file operations remain part of the
context. The exact private vendor rule and a single offending API are unproven;
the event timestamp is its publication time. **No antivirus fix is accepted.**

The private diagnostic modes require `--diagnostic-trace`, a fresh owned synthetic
workspace with an exact marker, two bounded text fixtures and a real empty source
file. They refuse an existing `.cycode` cache and redirected fixture paths. Their
bounded stdin controls only request one refresh and exit. They branch before
Engine construction and add no public SDK/MCP methods or separate editor model.
Normal recovery transactions, rollback/conflict checks and cleanup remain intact.
The previous diagnostic pause remains opt-in, fixed and one-shot.

All 154 original dirty-workspace file hashes and 254 native-source hashes were
preserved. Production builds and the two affected minimal C++ front-end checks
passed; 20 trace, 10 correlation and 4 boundary-snapshot data tests pass. These
checks do not establish antivirus or broader Core runtime acceptance. No native
test executable, protection change, restoration, relocation, upload, installation,
publication or GitHub Actions operation was used. The finite two-case campaign
is complete; no third build or case followed. Raw vendor reports remain local.
See [the measured minimal campaign](Benchmarks/recovery-minimal-20261109.json).

## Completed boundary test — same-document refresh before cleanup

After the earlier preparation was approved, the original production CLI ran one
same-document refresh with a fixed pause after index commit and before cleanup.
During the measured 15000.832 ms pause, the new revision was already committed,
all three payloads existed, both old payloads were byte-identical and no removal
span or new AV event appeared. Actual old-text removal then succeeded, followed
88.6655 ms later by matching avp 22418315/22418316. The official report binds
the behavioural detection to PID 39100. Exit was 5; the first event preceded
requested shutdown by 428.7155 ms. All 130 trace records / 65 spans are complete.
The conditional unpaused case was not started after the alert.

This strengthens cleanup in the observed sequence as the discriminating stage;
it does not prove which kernel operation or proprietary rule causes the verdict.
The diagnostic delay is not a production fix. The once-pending approval in the
archived [preparation record](Benchmarks/recovery-boundary-preparation-20261109.json)
was subsequently provided and consumed; there is no current pending question for
that run. See [the completed boundary result](Benchmarks/recovery-cleanup-boundary-20261109.json).

The installed console's official `REPORT SW2 /RA:<local-file>` and
`REPORT HipsTask /RA:<local-file>` exports were read-only. They expose behavioural
detections and target PIDs, not an offending filesystem stack or internal rule.
Both earlier passing controls and a failing case received the same reported
application-control restriction level, so that grouping alone is insufficient.

## Ordinary Win32 deletion comparison — alert reproduced; experiment reverted

The owner explicitly resumed one production rebuild and exactly the two prepared
comparisons after reviewing the source-only correction. The experimental sample
SHA-256 was `a711faea8b689975e5670de694c042ab4b2383f9691b1d819a88eb422006e483`.
Production compiler/link options were unchanged; no test executable was built.
The recovery caller used DeleteFileW for validated owned files and RemoveDirectoryW
for empty session directories, with no attribute reset or forced fallback. This
tested a hypothesis about the toolset removal path; it was not an established fix.

| Same-sample case | File/trace evidence | Result |
| --- | --- | --- |
| Replace index, retain old payloads | One index replacement, no text/base removal; four payloads retained; 184 complete trace records | Exit 0; no AV event through both 20-second live observations and 20 seconds after normal exit |
| Refresh checkpoint normally | One index replacement, one successful old-text removal, original base retained; two payloads; 126 complete trace records | New matching avp 22418312/22418313; exit 5; campaign stopped |

The first new event at 21:39:03.0460943Z is 79.7783 ms after the final native trace
and **299.5167 ms before the host requested shutdown**. The failing case contains
no native shutdown spans; both traces have no truncation, failed or open spans.
The initial checkpoint had its own clean 20-second observation. The assertions
confirm that actual old text was removed and the current checkpoint/base survived;
cleanup was not disabled. No documents.save, MCP Runtime, CyTools Runtime or project
compiler was used during either case. Passive monitoring continued after stopping.
Postflight found no Core process or executable; Kaspersky protection remained active.

**The ordinary-deletion change did not prevent the detection.** These observations
do not establish a proprietary rule, exact kernel call or false-positive verdict.
The candidate cannot be presented as an antivirus fix. Only this experiment's two
source/test edits were returned to their exact pre-experiment snapshots, preserving
all earlier instrumentation and local changes. The reviewed variant, hashes and
failed evidence remain in the local audit, without restoring/copying a binary.

Before the run, both candidate C++ files passed production-toolset MSVC /Zs, and
18 trace, 10 correlation and 8 comparison-postcondition data tests passed.
The candidate's expanded native recovery assertions were only syntax checked;
they were never linked or executed and are archived with the reverted trial.
All 154 original dirty-workspace baselines and all 252 trial-source hashes were
preserved during the comparison; the later two-file source reversion is explicit.
Source verification passed 84 Python / 43 JSON before recording this outcome.

The new alert reactivates the native build/execution hold. No automatic retry,
quarantine restoration, exclusion, upload, installation, publication or Actions
operation followed. Full MCP/save/restore/discard and other-platform acceptance
remain unestablished. A Windows filesystem/stack capture or detailed vendor
analysis is still needed to distinguish the rule from the observed sequence;
no privileged capture or vendor submission was performed here. See
[the measured experimental outcome](Benchmarks/recovery-cleanup-source-fix-20261109.json).

## Index replacement versus old-text removal — 2026-11-09, 22:17 Europe/Paris

The owner explicitly authorised one rebuild with the lifecycle markers and the
two prepared comparisons. Normal production linking passed, with no test binary,
compiler-option change, sample copy or installation. Both cases used exact SHA-256
`6688d8d173bdee6bc0d6eec46e3b46f962fd8d2c5490df79ae64ed01e77846dc`,
the same owned synthetic project path and fresh owned recovery state per case.
Both first observed one initial checkpoint for 20 seconds with the process alive.

| Case | Recorded file operations | Observation |
| --- | --- | --- |
| Checkpoint a second dirty document | Existing index replaced once; every old checkpoint and payload retained unchanged; four payloads afterward | Exit 0; no AV event through 20 seconds after the discriminating phase and 20 seconds after normal exit |
| Refresh the first dirty document | Existing index replaced once; old text removed once; original base retained; two payloads afterward | New matching AV records; native exit 5; comparison stopped |

The control has 184 complete trace records, including service drain, Engine
shutdown and skipped store cleanup while two checkpoints remain. The failing
case has 124 complete records, with successful index replacement/text removal
and no shutdown marker. Neither log is truncated or has open/failed spans.
No documents.save, MCP Runtime or project compiler was used in either case.

New avp records 22418304/22418305 identify this exact sample. The first, at
21:17:24.1911937Z, is 23.7527 ms after the refresh response, 77.0637 ms after
the final native trace and **774.2203 ms before the host requested shutdown**.
The postcondition checks succeeded: the former empty-file assertion defect did
not recur. Passive event observation continued after the native stop; no retry
or additional case was started. Later postflight found no Core process or binary.
All 154 original local-change baselines and all 252 native source files in the
comparison manifest remained byte-identical.

The strongest current candidate is old-text checkpoint removal in combination
with the preceding recovery writes. Index replacement alone did not reproduce
the alert in this bounded control. This is a single ordered comparison: case order,
reputation, payload/content differences, trace I/O and delayed classification are
not eliminated. It does not establish a single offending Windows call, a vendor
rule, a false positive or a validated fix. Ordinary recovery cleanup remains intact.

### Standard-library deletion path to capture

The build configuration selects MSVC 19.40.33821.0 / toolset 14.40.33807. Its
installed filesystem.cpp routes std::filesystem::remove through __std_fs_remove,
which attempts SetFileInformationByHandle / FileDispositionInfoEx with DELETE,
POSIX_SEMANTICS and IGNORE_READONLY_ATTRIBUTE flags before supported fallbacks.
This is static toolset-source evidence; the actual loaded CRT version/branch
and the vendor's interpretation have not been captured. No CRT, deletion API
or protection setting was changed. See Microsoft's description of
[file disposition flags](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_file_disposition_information_ex).

See [the measured comparison](Benchmarks/recovery-operation-isolation-20261109.json).
The new detection reactivates the native rebuild/execution hold. No quarantine
restoration, exclusion, upload, installation, publication or Actions run followed.

## Continued source and evidence research — no new native execution

The read-only correlator preserves Windows event sub-microsecond timestamp digits
and matches the exact sample SHA-256. Applied to the retained last run, it places
the first AV publication 76.1781 ms after the last native span and 18.6871 ms after
the host requested EOF. The older instrumentation contains no native shutdown
spans. Absence of those spans does not establish that shutdown did not run.

Source review narrows two alternative explanations. In the latest retained state,
the dirty document and its saved checkpoint have the same revision: the final
recovery scan should have no new snapshot or retired entry, and State destruction
skips its empty-session cleanup. WorkspaceFileIndex starts only on a snapshot
request; WorkspaceMonitor filters .cycode changes and updates in-memory revisions.
The recorded five methods do not request a file-index snapshot. These are source
expectations, not an operating-system call capture.

The known detected cases still share existing recovery-index replacement and
old-text payload removal. Explicit document saving, compiler/MCP use, old-baseline
removal and redundant cleanup of a consumed temporary name are not all required.
The responsible individual call, delayed vendor decision and false-positive
status remain unproven; production file operations have not been suppressed.

### Additional lifecycle observations

The opt-in trace now adds service.drain, engine.shutdown, recovery.scan and
recovery.store.shutdown. Engine shutdown covers its destructor body; the later
store span observes its own member teardown separately. An existing nonempty
checkpoint index reports store cleanup as skipped. This logs the normal decision,
without changing retention, Undo, save policy or antivirus settings.
These new labels are OFF by default with the existing 64 KiB fixed-data stderr
sink. At that preparation stage they had passed source/data checks only; their
later production comparison/runtime observations are recorded above.

Correlate explicitly captured evidence without running Core:

```sh
python app/scripts/correlate_antivirus_trace.py --trace native-trace.jsonl --phases phases.jsonl --kaspersky-event avp-event.xml --sha256 EXACT_SAMPLE_SHA256 --output new-correlation.json
```

The tool validates bounds, UTC dates, exact sample/event identity and duplicate
fields, detects native clock regression and copies only whitelisted metadata.
It distinguishes observed lifecycle spans from missing instrumentation, partial
tails and truncated logs, and never labels timestamp order as a cause verdict.

### Available external diagnostic evidence

No matching Application Error/WER record was found in the selected last-incident
window; Sysmon was unavailable. The visible Traces directory contained only an
older dump-writer log, and ordinary read access to the report database was denied.
No protected data was unlocked or decoded, and no quarantine content was read.
WPR/WPA were located, the current token was not administrator, and Procmon was
not found on PATH. No kernel capture or vendor diagnostic trace was started.

[Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon)
can capture filesystem operations and thread stacks. Kaspersky's accessible
[diagnostic guidance](https://support.kaspersky.com/help/Kaspersky/Win21.7/en-US/84091.htm)
describes support-guided component traces; this older help does not verify exact
Win21.26 settings. Logs can carry private data and nothing is uploaded here.

For a future explicitly resumed comparison, a second dirty document checkpoint
can replace the index while retaining the first document and all old payloads.
Compare that with ordinary refresh/old-text removal, with an observation window
before requesting shutdown. This uses existing methods, without a new phase-skip
API. Native reconstruction/execution remains suspended after the new detection.
See [the research record](Benchmarks/recovery-antivirus-research-20261109.json).

## Corrected Core retest — 2026-11-09, 21:34 Europe/Paris

The owner's explicit `retest` resumed one corrected production rebuild and the
three prepared recovery comparisons, with Kaspersky active and a stop at any new
detection. Production linking succeeded without new native test executables,
compiler-option changes or copying/installing the output. The corrected sample is
SHA-256 `d351fc02181ca2e609463d985cf2dde3318799987c168d72ab4a9199e3a9c76f`.

The first case completed five native operations: create, replace, recovery.flush,
second replace and second recovery.flush. No documents.save, MCP Runtime or
project compiler was used. The second revision is durably checkpointed: its new
text exists, its previous text payload is gone and its original empty base remains.
The created project document remains its empty on-disk baseline.

The optional trace contains 74 complete records (37 closed spans), without open
or failed spans or truncation. It shows successful atomic replacement of the
existing recovery index followed by successful old-text removal. There is no
temporary-cleanup attempt against a consumed name, no baseline-payload deletion
and no document-save span. This exercises the normal successful index replacement;
the partial Windows failure branches are not tested by it.

Two new avp records, 22418291/22418292 at 20:34:17.2751051Z/.3074843Z, identify
this exact corrected sample. The first is recorded 23.704 ms after the second
flush response and about 76.178 ms after the final native trace. The native exit
is 5; the binary was absent at later postflight and no Core process remained.
The antivirus problem is **not resolved** by the atomic-save correctness fixes.

### Harness limitation and stop

The diagnostic driver wrongly asserted that the project document must be absent.
documents.create creates an empty disk file. That postcondition failed after the
successful second checkpoint and requested EOF immediately, skipping its planned
8-second pause and 20-second post-exit observation. The AV event was published
18.687 ms **after** this shutdown request. The original driver/report is preserved;
the two empty-baseline assertions were corrected as data/source work, without a
native rerun. This is an interrupted diagnostic, not a passing checkpoint case.

The reproduced detection therefore does not require a documents.save request.
It does not isolate index replacement, payload removal, shutdown or accumulated
activity as the exact trigger. Trace timing, a changed sample, reputation and
single-run coverage remain limitations. No proprietary rule or false-positive
verdict is established. The two later cases (inactive discard and checkpoint/save)
were not started; the new detection reactivates the native rebuild/execution hold.
All 154 original local-change baselines remained byte-identical. No protection,
exclusion, quarantine, upload, installation, publication or Actions change occurred.

See [the sanitized retest record](Benchmarks/recovery-antivirus-retest-20261109.json).
For a later explicitly authorised isolation, checkpointing a second dirty document
while retaining the first would replace the index without deleting an old payload,
using existing operations and normal cleanup. No phase-skipping API is needed.

## Recovery retirement source investigation — 2026-11-09

The discriminating scenario above directs source review to three separate steps:
atomic document replacement, atomic recovery-index replacement and removal of the
owned text/baseline checkpoint files. An empty index and absent payloads after save
show completion, not which Windows API triggered the vendor event.

Two independent correctness defects were found in the shared atomic writer:

- A successful Windows ReplaceFileW/MoveFileExW or POSIX rename consumes the
  temporary pathname. The cleanup guard now relinquishes that name immediately,
  avoiding a redundant deletion attempt and protecting any later occupant of it.
  POSIX link publication still leaves the original temporary name owned and cleaned.
- Windows ReplaceFileW can fail after partially moving files (errors 1176/1177).
  The failed transaction now leaves its replacement artifact for inspection instead
  of deleting it during unwinding. It reports failure, keeps the dirty document
  and does not retire its checkpoint. It does not silently retry or restore files.

The writer also checks close failures before publication. These are source-level
correctness fixes, **not a proven antivirus remedy**. See the documented partial
failure semantics of [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew).

### Optional bounded native trace

The CLI accepts the explicit leading flag `--diagnostic-trace`. Tracing defaults
OFF for the CLI and all library embeddings. Only this host opt-in enables it;
the SDK bridge, method catalogue and ordinary process launch remain unchanged.
It emits fixed JSONL labels to stderr, leaving stdout's protocol untouched.
Nested spans cover document save/replace, recovery retirement/checkpoint/index
commit, text/baseline removal and atomic create/write/flush/close/replace/cleanup.
Each record has a sequence, UTC microseconds, span/parent identifiers, phase,
outcome and numeric system error. No source, path, parameter, token or command
line is recorded. The sink stops at 64 KiB with a fixed truncation marker.
Trace I/O failure cannot unwind a file transaction. It can change timing, so
future comparisons must record whether it was enabled.

This is prepared diagnostic usage, **not authorisation to resume native runs**:

```powershell
# Only after a new explicit owner resumption; owned synthetic workspace only.
cycode_cli.exe --disable-plugins --diagnostic-trace WORKSPACE --stdio 2> native-trace.jsonl
```

Analyse an already captured log without launching Core or uploading data:

```sh
python app/scripts/analyse_native_trace.py --input native-trace.jsonl --output native-trace-analysis.json
```

The reader bounds input, validates the fixed schema/labels, retains interrupted
spans and distinguishes truncation, missing beginnings and unrelated stderr.
It refuses unexpected fields and never copies unrelated log text into its report.
The final span is an observation, not proof of causality: vendor event publication
can lag the triggering operation.

### Remaining diagnostic comparisons

After explicit owner resumption, use the existing recovery operations on synthetic
data, preserving ordinary cleanup and stopping on any new detection:

1. Re-checkpoint a changed buffer with `recovery.flush`, without saving. This
   replaces the index and removes the previous text payload while reusing its base.
2. Create a checkpoint, close its process without saving, then use a new process's
   `recovery.list` and guarded `recovery.discard` on that exact inactive checkpoint.
   This commits its index and removes both payloads without replacing a document.
3. Repeat checkpoint-then-save with the optional trace and correlate its completed
   phases with the AV events. No phase-skipping or cleanup-suppression API is added.

At preparation time these cases were not executed; the corrected retest above
subsequently attempted the first and stopped on a new detection. The native
rebuild/execution hold is active again. No new CLI, installation, publication, protection change
or Actions run was produced for this source investigation.

## Command-order and recovery isolation — 2026-11-09, 21:02 Europe/Paris

The owner explicitly authorised one reconstruction of the deleted CLI and six
independent diagnostic scenarios, with Kaspersky active and a stop at any new
detection. The normal production target was rebuilt once at its original output
path, from unchanged native sources at 34323cc and unchanged compiler settings;
native tests stayed OFF. The rebuilt SHA-256 is
`ebf200e85f47437e67c37df4d7c2996c320e850e3b8fd8da7fdb8858fdcd4832`.
All five executed scenarios used these same bytes and the same synthetic project
path, with fresh owned cache/note state between cases. Earlier evidence is retained.

| Independent scenario | Observed result |
| --- | --- |
| Startup, idle and normal shutdown | Exit 0; no event during the case and 20-second post-exit observation |
| Immediate document create/replace/save before project tools | Exit 0; no observed event |
| Actual MSBuild/clangd analysis and navigation, no document save | Two clean observations; exit 0; no observed event |
| Explicit recovery checkpoint, no document save | One checkpoint observed; exit 0; no observed event |
| Explicit recovery checkpoint, then document save | New avp detection; native exit 5 |
| Save before compiler analysis | Not executed after the new detection |

Before the failing save, recovery.status reported one persisted checkpoint and
zero pending documents. The save returned clean. The first new avp record,
22418285 at 20:02:42.3999287Z (followed by 22418286), matches the rebuilt hash and
was recorded about 24.7 ms after that response, before requested shutdown.
The retained recovery index has zero buffers and its text/base payload files
are absent, consistent with completed native retirement. No MCP, CyToolsCore
Runtime, MSBuild or compiler server ran in this failing scenario. The Core
and its child processes were stopped, and its binary was absent at postflight.
The 154 original local-change baselines remained byte-identical.

The discriminating path is documents.save -> Engine::retireRecovery ->
RecoveryStore::erase -> atomic recovery-index commit and owned-payload removal.
This narrows the investigation beyond generic CLI/MCP or shutdown hypotheses.
It does not identify the exact Windows call/vendor rule or certify a false
positive. Each control was a single run, and event delay, case order and local
reputation remain limitations. No recovery function was disabled or modified.

See [the sanitized command-order record](Benchmarks/command-order-antivirus-20261109.json).
The sixth case stopped as agreed. No retry, quarantine restoration, exclusion,
protection change, upload, publication or GitHub Actions run followed detection.
Native execution/rebuild is suspended again; source/evidence work remains allowed.

## Direct CLI reproduction — 2026-11-09, 20:45 Europe/Paris

The owner explicitly authorised a single diagnostic comparison on this workstation,
with Kaspersky active and a stop at any new detection. The existing production
binary was neither rebuilt, copied, renamed nor restored. A private owned synthetic
C++ project supplied the same planned operations for native stdio, native socket
control and official CyToolsCore MCP, with event monitoring across phases and exit.

The first case, **direct native CLI `--stdio`**, completed fourteen operations:
status, file/text lookup, document read, actual MSBuild evaluation, clangd setup,
two clean version-bound diagnostic observations, one definition/three references,
document creation/replacement and explicit save. No MCP server or CyToolsCore
Runtime was started for this case; reading installed package version metadata
does not start either component. Native plugins were disabled.

Two new avp 4662 records, 22418283/22418284, identify the same SHA-256
`609035c6cadb3d6dd667d55cb2d415092367910cbe1297647ef330e6eefcb1f7` at
19:45:55.5739493Z / 19:45:55.6059317Z. The first event follows the save response
by approximately 26.6 ms and **precedes the harness shutdown request by 440.7 ms**.
The native process exited with code 5; the binary was absent at postflight and
the owned native/clangd processes were gone. The original workspace's 154 saved
local-change baselines remained byte-identical. No protection, upload or Actions
setting was changed.

The comparison stopped automatically. Neither socket control nor MCP was started;
there is no paired performance or successful overall acceptance result. This
establishes that **MCP and the CyToolsCore runtime are not necessary to reproduce
the detection**, and weakens the earlier shutdown-only hypothesis. It does not
identify Kaspersky's proprietary rule, prove a false positive, or prove that save
itself caused the event: event publication may lag earlier activity and this
exact sample had already been detected. Atomic document saving, saved-document
events and recovery retirement are the next native source-review scope.

See [the sanitized diagnostic record](Benchmarks/cli-antivirus-comparison-20261109.json).
Raw event exports, phase/process logs and the synthetic project remain private
runtime evidence. The stop-on-new-detection condition is active again; do not
rebuild, restore, recopy or retry the binary, including an automatic MCP restart.

## Second production incident — 2026-11-09, 19:20 Europe/Paris

The owner explicitly authorised lifting the hold for production compilation,
MCP validation, installation and this release, while retaining a stop on a new
detection. The rebuilt standalone Core installed under its normal product name;
no protection was changed. An official MCP client using the bundled CPython/SDK
discovered 237 domain operations plus 21 controls. Real CyCodeIDE file/text search,
62-unit MSBuild evaluation, two clean version-bound diagnostic observations,
compiler definition/four references and guarded creation/save of Docs/CoreAgents.md
all completed. The full acceptance then failed at host drain. System Watcher
deleted the installed Core and its abnormal exit was recorded. The native source
files used for navigation remained byte-identical.

The owner supplied PDM:Trojan.Win32.Generic / High / Deleted / Process, MD5
`442a69730eff3e458c329d1027d82139`. That MD5 matches the existing production build, SHA-256
`609035c6cadb3d6dd667d55cb2d415092367910cbe1297647ef330e6eefcb1f7`. Original avp event 4662 records 22418264 and 22418265
were recovered at 18:20:18.2897451Z and 18:20:18.3283776Z. No private user,
machine, command or endpoint fields are included in the public benchmark.

MCP is disabled, the installed CLI is absent, and no Core process remains.
No automatic reconstruction, copy, quarantine restore or native retry follows
this detection. Binary release publication is suspended; source/skill packaging
and a clearly labelled agents-only draft remain possible. Exact flagged behaviour
and the vendor verdict are still unknown: neither a false positive nor a clean
release is established. The former controlled authorisation does not waive its
explicit stop-on-new-detection condition.


## Current incident and scope

On 2026-11-09 at 16:07 Europe/Paris, the owner supplied a Kaspersky
`PDM:Trojan.Win32.Generic` alert for the production `cycode_cli.exe`. The official
SDK/MCP acceptance lost the native endpoint between rename Apply and Undo. Disk
source hashes remained intact. Two existing Application-log `avp` events, 4662,
were subsequently recovered at 15:07:39.2735947Z and 15:07:39.3197545Z (16:07:39
Europe/Paris), records 22418236 and 22418237. Both identify the same original
process SHA-256:

`90a9b3ce88c53f26911e6367d7a2a10865966a744508808f88559c38c59e3b27`

Installed Kaspersky version was read as 21.26.4.406; the events retain the database
stamp `2026.11.09 10:03:00:000` without a stated timezone. Vendor name/confidence
codes are retained without interpreting their undocumented meaning. The exact
blocked operation is still unknown. The flagged executable is no longer available
at its original build path. This evidence identifies the sample and time; it does
not establish a clean sample, root cause or false positive.

Local native execution and rebuilding/retrying the detected executable remain
suspended. No exclusions, certificate trust installation, quarantine restoration,
renaming/relocation, packers, obfuscation, protection changes or automatic uploads
are part of this work. GitHub Actions stay paused. Source/data/documentation and
C++ syntax-only checks are permitted; no application executable is generated by
the syntax checks.

## Corrections made in the sources

Windows process launches for search/build tools and language/debug servers now
use `STARTUPINFOEXW` and an explicit `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`. A child
receives only its own standard I/O handles. This prevents unrelated inheritable
application handles and concurrent launches' pipes leaking into another child.
Duplicated stdout/stderr handles are listed once. Invalid/non-inheritable handles
and attribute setup failure refuse launch rather than falling back to broad
inheritance. The attribute storage is aligned and scoped through `CreateProcess`.

The language-server launcher now checks creation/configuration of its owned job,
assignment of the still-suspended child to that job and successful thread resume.
It refuses to execute the server if ownership cannot be established. Partial
pipe creation closes the already-created child endpoints. The generic process
runner also checks resume failure and cleans up the owned child. Termination is
limited to handles/jobs belonging to children created by this application.

These are genuine security and lifecycle corrections. They have **not** been
demonstrated to remove the Kaspersky detection. Pipes, authenticated loopback IPC,
argv-based external tools, draft recovery and safe cancellation remain necessary
application features. They are not removed to change an antivirus verdict.

Reference: [Microsoft handle inheritance](https://learn.microsoft.com/windows/win32/procthread/inheritance).

## Protect the local native credential

The native transport now obtains 32 random bytes from Windows `BCryptGenRandom`
or POSIX `getentropy` and keeps the same 64-character hexadecimal token format.
An OS entropy failure refuses startup. Authentication compares every byte of a
correct-length token without stopping at the first mismatch. This native channel
continues to serve both application and SDK clients; SDK identities, permissions
and scheduling are unchanged.

The credential descriptor is private **before any token is written**. Windows
creates the sibling temporary file with a protected DACL allowing the current
process-token user and LocalSystem; the effective ACL is checked before writing,
so a file system that ignores the requested protection refuses publication.
Atomic replacement moves the protected file, avoiding a merge with an old ACL.
POSIX creates the file exclusively as mode 0600, flushes it and publishes it
atomically; the former chmod-after-publication window is removed. Symlink
descriptors are refused, and independent randomness names the temporary file.

The host must still choose a trusted parent directory. File permissions do not
isolate arbitrary programs running under the same user account; native access
remains a host credential and not a replacement for SDK operation permissions.
These are source-level security corrections, not evidence that Kaspersky's
behavioural classification has changed. Windows MSVC syntax checks pass; native
Windows/POSIX acceptance remains suspended/unexecuted.

## Prepare a local review dossier

```sh
python -B -X utf8 app/scripts/antivirus_evidence.py --executable path/to/cycode_cli.exe --detected-at "2026-11-09 16:07 Europe/Paris" --output path/to/new-local-dossier
```

The command reads an existing artifact without executing or copying it. It never
rebuilds a missing sample, restores quarantine, scans other files automatically,
uploads anything or calls an external scanner. `--known-sha256` can supply the
exact hash from the original antivirus event. A different available file hash
rejects the request: an old passing executable cannot substitute for the detected
one. Missing bytes/hash remain explicitly unavailable. Static PE certificate-table
metadata is not a cryptographic signature/trust verdict.

Optional repeated `--report path/to/explicit-export` records only file names,
sizes and hashes. Report contents, absolute private paths, source files,
credentials and raw logs are excluded. The new output directory contains
`report.json` and a reviewable `vendor-request.txt`; an existing dossier is never
overwritten. Nothing is sent automatically. These files are for local review,
not for automatic inclusion in an application package or public documentation.

An explicit `--kaspersky-event path/to/avp-event.xml` (repeatable) can recover the
original SHA-256 even when the binary has been removed. Use
`--antivirus-version 21.26.4.406` for the product version. The reader accepts bounded
avp Windows Event XML, rejects declarations/entities, unrelated executables,
duplicate fields and mixed-build/conflicting hashes, and retains only whitelisted
metadata. Machine/user names, private process command lines and descriptor paths
are excluded. Opaque vendor codes are not treated as an explanation or verdict.
The read-only Windows event query/export does not open quarantine, decode private
vendor databases, change antivirus settings, rerun the application or upload data.
Raw XML exports stay private; the review dossier contains only their metadata and
hashes. The exact original hash is now known; the detailed behavioural event and
vendor assessment remain missing.

## Resolve the detection without a paid certificate

The owner's policy is to proceed without buying a code-signing certificate.
The Core already embeds product/version resources; no Authenticode signature is
claimed for its unsigned development builds. Checksums identify bytes but do not
replace antivirus assessment or Authenticode trust. A self-signed certificate and
installing a local trusted root are not used to resolve this incident.

Kaspersky documents checking the original object/hash in its Threat Intelligence
Portal and requesting reanalysis when the result is disputed. A PDM escalation
may require the application trace, System Watcher logs, detected file, product and
database versions, original hash and relevant system diagnostics. Review private
information before sharing any export. Do not rerun the blocked application just
to generate logs while the hold is active. The screenshot alone cannot identify
the blocked operation.

- [Official Kaspersky false-detection procedure](https://support.kaspersky.com/common/error/other/1870).
- [Developer Allowlist Program](https://www.kaspersky.com/partners/allowlist-program): participation is free, eligibility requires a current product/company website including a legal address and the program's software conditions. A company signature is mentioned only if available. Admission and a favourable verdict are not guaranteed; no application has been submitted.
- [SignPath Foundation](https://signpath.org/terms) also offers free signing for eligible open-source projects. Its conditions include OSS-only components, verifiable builds and approval. The current private repository/dependency distribution has not been assessed as eligible; no integration or signing claim is made.

No certificate purchase is needed to prepare the dossier or request a vendor
assessment. A later signed release would still need independent antivirus
validation: signing does not establish that an arbitrary behaviour is safe.

## Verification and remaining limits

MSVC `/Zs` syntax-only checks pass for `Process.cpp` and `LanguageClient.cpp`.
Data-only Python tests cover missing samples, hash mismatch, original avp event
identity, mixed-build rejection, malformed/oversized XML, private-field exclusion
and preservation of existing dossiers. Existing native endpoint assertions now
also cover the token format, first/last-byte mismatches and POSIX descriptor
permissions, but those assertions have not been run locally. No new binary, GUI, native fixture, installation or
antivirus setting was involved. Native behaviour, leak/cancellation acceptance,
the vendor verdict and the disappearance of the detection remain unverified.
Resume requires the owner to resolve the antivirus issue and explicitly authorise
native runs; producing another binary to retry is not that resolution.

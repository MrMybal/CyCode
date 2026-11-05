# Update history and recovery receipts

Application updates > Update history offers Inspect detailed update history. This
explicitly runs a cancellable native metadata inspection without opening/hashing an
archive or backup payload, downloading, installing, restoring or running the helper.
The short startup/cache summary remains until this inspection finishes. Native/
filesystem/GUI acceptance is pending; see [Validation](Validation.md).

## Viewing a snapshot

Twelve entries appear per page. The shared CyUI text filter searches versions,
English/translated results, UTC dates, category, state, failure code and directory. Categories show all
entries, those needing attention, installations or recoveries. Waiting/failed/restart-
failed installations and cancelled/failed recoveries need attention. Labels/matches
are cached until the query, category, locale or inspected snapshot changes; there
is no per-frame filesystem probe.

Selection shows the directory and recorded recovery stage, prepared/completed
counts, fresh copied bytes, installed-file moves and pending-marker removal. These
are historical statements about that attempt, not the current installation. Filters,
locale, page changes and successful inspection clear selection. Failed/cancelled
inspection retains the previous snapshot. Ordinary cancellation and joined shutdown
own the future; smoke/GUI test sessions cannot invoke real inspection. No automatic
refresh, retry, network request or proposal is triggered.

Observed rows sort by recorded time descending, then stable directory identity.
New recovery receipts contain informational Unix seconds. Older format-1 receipts
without time sort after dated entries and show Date unavailable; filesystem times
are not substituted. Recorded times do not establish trustworthy chronology.
Installation history is the latest result.json per staged download, not an append-
only record of every state. Recovery has one immutable result per advertised work
attempt; early failure/reporting failure/process termination can leave no receipt.

## Bounded native inspection

UpdateHistory belongs to cycode_updates and reuses shared staging/outcome parsers.
It accepts a Windows portable installation and reads direct download-* metadata
plus direct recovery-* receipts under direct transaction-* directories. Numeric
work names use the existing validator. It does not recurse through restored,
replaced or backup trees, or follow paths provided by receipts. Unsafe/reparse/
symlink ancestors, nonregular/hard-linked metadata, foreign identities, malformed
fields and unsupported outcomes are refused. No lock, history or cache file is
created; no payload is changed.

Each snapshot visits at most 512 direct cache entries and then 128 total direct
transaction children, including unrelated names in both budgets. It retains at
most 128 rows and reads at most 4 MiB of history metadata. Prepared/outcome/result
records are capped at 8 KiB, work identities at 64 KiB, and captured pending text
at 8 KiB. The portable-mode manifest check retains its separate 2 MiB bound.
Cancellation runs between bounded reads/entries; blocked filesystem calls can delay
it. Changing/unreadable records can be rejected. This is an observation across
different moments, not a filesystem transaction.

Enumeration is a bounded prefix. Within it, download and transaction paths have
stable processing order; installation rows can exhaust the row budget first. Time
sorting orders only retained rows. Pages do not fetch omitted records or guarantee
the newest records across the entire cache. Visible rejection/limit notices avoid
claiming an exhaustive audit. Unknown, legacy and omitted data is never deleted.

Recovery work.json format 1 must agree on installation, actual transaction,
version and plan SHA-256. Its captured format-2 pending identity must agree too.
The format-1 result must bind that version/plan and have known phase/outcome codes,
booleans, bounded counters and an optional valid time. Preparation is bounded to
4096, actionable union entries to 8192 and fresh copy bytes to 258 MiB. Counts must
be ordered; completed entries require recorded installed-file moves. A restored
receipt requires complete phase, completed counts and pendingCleared. Failed/
cancelled receipts cannot claim complete marker removal.

This checks metadata consistency only. It does not hash recovery.json, backups,
prepared copies or installed files. A locally edited coherent receipt remains
advisory. Real actions still require full native validation and explicit consent;
see [Recovery](UpdateRecovery.md) and [Installation](UpdateInstallation.md).

## Copying results

Copy selected history entry includes one cached row. Copy update report adds the
entire retained history snapshot when inspected, alongside the existing report.
Filters/pages affect selection/display only; full reports include all retained
rows. Clipboard/formatting failure is caught and displayed.

Native history JSON is capped at 256 KiB, has format 1 and observationOnly=true,
and contains the installation once, relative record/transaction paths, typed
outcomes, optional times, counters and rejection/limit/budget observations. Raw
metadata extras, captured pending bytes, technical errors, environment values and
credentials are excluded. Copying is explicit; there is no automatic export/upload.
Reports cannot authorize recovery, deletion or restart.

## Integration and validation

UpdateHistory/UpdateHistoryPolicy join the existing native updater library and
UpdateHistoryUI the existing UI library. No executable target, external dependency,
CyUI module, SDK operation or plugin ABI is added (227 operations / ABI2). The
native owner stays free of SDL/ImGui/SDK links. EN/FR use the generated catalogue;
stock controls inherit themes and dimensions use cyui::dp/style. The helper retains
Comctl32 v6/asInvoker.

Twenty compiler-evaluated assertions cover attention/phase/outcome/count policies.
Opt-in native bodies specify metadata-only discovery beyond twelve rows with absent
payloads, no created locks, sorted/unknown times, selected/full report exclusions,
foreign/oversized/invalid records, cancellation and budgets. An existing GUI fixture
specifies test-session exclusion. Those bodies are syntax-checked only, never
linked/run under the antivirus suspension. Actual filesystem/cancellation/races,
keyboard/filter/page/selection/clipboard, themes/density/DPI, clean-machine and
Linux/macOS acceptance remain unexecuted. The UI offers no restore/delete action.

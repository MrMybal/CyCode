# Workspace monitoring and path index

The native Engine owns WorkspaceMonitor. Windows uses recursive
ReadDirectoryChangesW, an overlapped worker, a 60 KiB notification buffer and
100 ms stop checks. An overflow advances both counters and forces reconciliation.
No path-event queue or callback can grow without a bound. Shutdown cancels the
pending read and joins its owner before releasing the directory handle.

The non-Windows implementation currently polls directory metadata once per second
(up to 200,000 entries, 64 directory levels, no symlink traversal). Windows falls
back to it if notifications cannot start or fail. Its backend/error is visible in
workspace.poll. Only Windows native notifications have been executed locally;
Linux/macOS polling is implemented but unverified. Polling a large tree has a cost;
native Linux/macOS watchers remain planned.

The desktop reconciles in a background future every 500 ms. A clean document whose
disk bytes changed reloads through the same expectedRevision guard as an explicit
reload. The editor receives the new revision, retains its cursor and records the
replacement through its existing Undo path. Dirty documents retain their live text
and saved baseline. Removed, unreadable, oversized or invalid UTF-8 files retain
their last valid buffer too. A visible warning accompanies diskConflict metadata.
Saving still rejects external changes. An explicit discard/reload is required to
replace a dirty buffer; no automatic last-writer-wins save is added.

Every two seconds a reconciliation also checks open documents, even if notifications
were coalesced or the path is in an index-excluded directory. Directory notifications
do not follow a symlink to grant new document access: Engine.resolve still checks
canonical workspace containment before reading. Images and external read-only SDK
sources retain their own reload lifecycle. Unsaved-buffer checkpoints are owned
by a separate Engine worker; see [Recovery](Recovery.md). The watcher never treats
checkpoint writes as project edits or automatically saves dirty buffers.

workspace.files builds a sorted ripgrep path snapshot off the UI thread, respecting
the existing ignore rules and excluding .git, runtime, data and .cycode. The cache
holds up to 200,000 paths; ripgrep output is bounded to 32 MiB/30 seconds. Structural
events and .gitignore/.ignore modifications invalidate it. Changes to global Git
excludes or .git/info/exclude require an explicit refresh. The cache is in memory;
there is no claim of a persistent path or symbol index.

Clients request pages with offset (default 0), limit (1..20,000) and refresh
(default false). Each response contains files, truncated, nextOffset, total,
indexRevision and required positive generation. Continue at nextOffset until null;
keep the same (indexRevision, generation) pair or discard the assembled pages and
restart. A manual rebuild advances the process-local generation even if the
watcher's indexRevision is unchanged. These counters describe observations, not
a frozen/persistent snapshot handle. Pages have an additional conservative 8 MiB
JSON budget. truncated may mean another page exists or the overall snapshot
exceeded its bounds. The desktop compares both counters while assembling pages
asynchronously, retains a bounded tree and retries after a concurrent change.
Manual Refresh always rebuilds.

workspace.poll / workspace_poll returns watcher counters, the extension-registry
revision, reloaded paths, conflicts, checkpoint health and watcher status. It only reloads clean
buffers; it does not save or discard dirty edits. These methods use the same native
service from GUI, CLI and CyToolsCore/MCP.

CTest workspace_monitor uses actual filesystem operations in an owned temporary
workspace: clean edits, dirty conflicts, deletion/recreation, invalid content,
Unicode paths, pagination and cache invalidation. It does not alter user projects.

## Unsaved Unicode search

Dirty/untitled text replaces on-disk matches for the same document. Byte-exact
literal and all-ASCII insensitive queries keep the native in-memory path. When
insensitive matching involves non-ASCII text, the existing numeric snapshot owner
uses ripgrep fixed strings, sharing Unicode simple folding with disk results.
Regex queries retain the same owner and syntax. No PCRE2 runtime or new process
model is added. Sigma/final sigma, Kelvin and capital sharp-S match their simple
equivalents; multi-character folding and Unicode normalization are not performed.

The snapshot route retains 128-buffer / 64 MiB and 15-second / 8 MiB subprocess
bounds, cancellation, dirty-source precedence and one-based UTF-8 byte columns.
ASCII folding is locale independent. A 512-byte result preview backs up to a
complete UTF-8 scalar instead of returning malformed JSON text. Cleanup removes
only recorded numeric files inside the verified temporary folder, leaving unknown
entries intact; it never recursively removes a redirected folder.

Public-API cases in the existing opt-in core fixture assert accents, sigma,
Kelvin/ASCII queries, sharp-S behavior, literal punctuation, exact sensitivity,
byte positions, scalar-safe preview serialization and unchanged source bytes.
They are compiled syntax-only locally; actual execution remains a CI gate.

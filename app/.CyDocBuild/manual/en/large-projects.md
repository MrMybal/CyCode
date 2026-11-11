---
id: large-projects
title: Large projects and paged text
description: Native streaming data, paged agent documents and coherent disk-backed transactions.
section: Start here
parent: native-core
order: 266
revision: 1
---


## Storage and ownership {#storage}

The project Engine owns both ordinary live buffers and the new paged UTF-8
storage profile. A path cannot be open in both profiles. CyToolsCore still owns
SDK/MCP identities, jobs and permissions; no Python document model is introduced.
Private immutable payloads are kept under `.cycode/working-data` and leased by
live documents and change plans. They are not task artifacts or project saves.
Ordinary shutdown removes only unchanged process-owned files; modified or
unrecognised files are retained. Interrupted working data is not automatically
recovered or garbage-collected. Keep `.cycode` out of version control/backups
intended for publication; payloads can contain source text.

The ordinary text/GUI/LSP profile retains its 4 MiB compatibility policy. For
larger source/log/text data, agents explicitly choose the paged profile below.
It has no chosen 4 MiB whole-file ceiling: data is imported and validated in
64 KiB working blocks, then referenced by pieces. It requires disk headroom for
its private baseline and edits. UTF-8 is validated incrementally, NUL/binary and
invalid encodings are refused, and original BOM/CRLF bytes are retained.

## Agent read and edit workflow {#agents}

1. Call `documents_read_page` with a workspace path and a byte page length
   from 4 to 262,144. Initial open imports a private baseline without overwriting
   the source. Read returned `id`, `revision`, `bytes`, `storage` and `nextOffset`.
2. Continue using `offset: nextOffset` and the current `expectedRevision`.
   UTF-8 code points are never split. These offsets are bytes, not UTF-16 or lines.
3. Use `documents_prepare_byte_edit` on that live revision with a byte `offset`,
   removed byte `length` and bounded inserted `text`. It prepares a change only.
4. Inspect `changes_preview` with a page `length` and independent
   `beforeOffset`/`afterOffset`. Follow `nextBeforeOffset`/`nextAfterOffset`.
   Small ordinary previews remain compatible; full large previews are refused
   with guidance rather than silently truncated.
5. Use the common `changes_apply` and `changes_undo` transaction owner.
   Neither saves the source. Re-read the current paged revision after a change.
6. Explicitly call `documents_save_paged` with that revision to stream through
   the shared atomic writer. Exact saved-source digests and immutable payloads
   are checked; known external conflicts and read-only sources are refused.
   Close with `documents_close_paged`; dirty documents require explicit discard.

Long initial import, edit preparation and save release the Engine mutex while a
per-document I/O reservation refuses conflicting actions. These operations and
publication are currently noncancellable; the SDK truthfully advertises that.
Previews and transactions expire with the native Core; this profile has no LSP,
line-based editor API, Save as, GUI adapter or crash-recovery implementation yet.
A successful paged text edit does not establish compiler analysis of a huge file.

The existing `search_text` includes unsaved paged text in literal and regex
searches: stream a private snapshot, suppress stale disk matches, use the shared
cancellable ripgrep owner, then recheck the live revision. This needs temporary
disk headroom and keeps the existing result/time/output budgets. It does not
turn paged byte positions into LSP positions.

Compiler queries require paged edits to be saved or closed, because this profile
has no LSP overlays. Global literal/regex text search still includes those edits.
`analysis_inspect` can inspect a clean, already-open paged C/C++ source using its
native revision and real compiler command; source baselines use streaming digests,
not a complete text copy. Dirty/busy paged buffers refuse disk-based inspections.
This is clang-tidy/Clang Static Analyzer execution, not live clangd navigation.

## Compiler databases and progress {#analysis}

Compiler database inventory and inspections visit one command object at a time.
BLAKE2b-256 file digests use 64 KiB buffers; no 64 MiB whole-database ceiling
remains on these paths. JSON framing is strict, accepts an initial UTF-8 BOM,
checks cancellation and limits one command object to an 8 MiB working budget.
Compiler/LSP tools retain their own resource limits and requirements.

Checkpoint format 2 publishes an atomic manifest referring to immutable pages
of up to 256 paths/records. Only new/changed record pages are written for a batch;
old pages remain valid until the manifest is published. Aggregate progress can
exceed the old 32 MiB monolith. A page is bounded to 16 MiB and the manifest to
32 MiB; these are per-object working budgets. Format 1 remains readable and is
migrated when progress is saved. Old observations remain historical/unknown.
Digests, context guards and single-writer leases are retained. Unreferenced pages
are retained; there is no automatic destructive cleanup.

Immutable analysis rows are shared between native session copies. Path indices
and observations still consume memory proportional to their count; this stage
does not establish a constant-memory project index or persist clangd's ASTs.

## Large transactions {#transactions}

Ordinary compiler previews and rename baselines now spool immutable payloads to
private disk instead of retaining another full before/after/disk copy per file.
The aggregate 16 MiB refactoring cutoff is removed. Edits remain compiler-issued;
no textual rename substitute or partial global plan is introduced.

Apply validates every affected live revision, disk baseline and stored payload,
and stages all ordinary resulting strings before any buffer mutation. Ordinary
buffer commits use string swaps; paged buffers swap immutable piece snapshots.
Undo uses the same transaction and guards. Language notifications follow the
complete native commit. Ordinary target buffers and the staged output set still
need RAM, and large transaction validation can hold the Engine mutex. Disk space,
protocol frames, backend limits and allocation failures remain real constraints.
No hardware-unlimited or filesystem-race/power-loss guarantee is claimed.

## Reproduce and interpret evidence {#validation}

```text
python app/scripts/verify_large_project_capacity.py --run-native --core . --executable <Core> --clangd <clangd> --clang-tidy <clang-tidy> --output <new-directory>
```

The explicit Windows production runner uses the official MCP client/runtime,
real MSBuild compiler commands and clangd/clang-tidy. It creates source data,
never a native fixture executable or GUI. psutil is optional; without it memory
sampling is reported unavailable. The fixtures cover a 96 MiB UTF-8 document,
70 MiB real-command database padded with legal JSON whitespace, more than 32 MiB
of seeded unknown historical metadata, and a real nine-file compiler rename with
more than 16 MiB of before/after text. Seeded cache rows are storage data, never
compiler-proof evidence. Inspect the retained report for actual outcomes.

Linux/macOS and graphical integration are deferred. GitHub Actions stays paused.
No automatic installation, release or push follows these local changes.

The Windows production checks passed: 96 MiB paged text with BOM/CRLF/Unicode,
unsaved literal/regex search, read-only/conflict refusal and Apply/Save/Undo;
5 MiB clean paged C++ inspected by the real clang-tidy; 70 MiB real-command
JSON inventory/inspection; 35 MiB seeded unknown progress metadata with corruption
and nesting refusal; and a real nine-file rename with 20 MiB of before/after text.
The Core's sampled RSS increase for the 96 MiB text case was under 1 MiB, without
a language server running. This is a scoped fixture, not whole-project memory.
The retained evidence identifies each binary and its exact validation scope.

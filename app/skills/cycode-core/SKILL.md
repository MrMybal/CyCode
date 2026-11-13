---
name: cycode-core
description: Use a connected CyCode Core MCP server to investigate and edit a code project with compiler navigation, references, diagnostics and guarded changes. Applies to CyCodeIDE development and other projects hosted by Core.
---

# CyCode Core

Prefer the connected Core for compiler-backed questions about symbols, usages,
types and project configuration. Use text search for literal content and file
lookup for paths; neither proves semantic coverage. Ordinary file tools remain
useful for unsupported formats, configuration and a disconnected Core.

## Connect and discover

Find the configured `cycode_core` server (the client may prefix its tool names).
If its tools are unavailable, read `references/connection.md`; do not invent
calls, launch a replacement host implicitly or report the service as connected.

Read the real `cy_describe`, `cy_list_operations`, `cy_describe_operation` and
`cy_can_run` results before using an operation. Start with `op_service_status`
and `op_language_status`. Server readiness does not mean that every source is
indexed. A registered tool does not prove its compiler/server is installed.
An idle/lazy backend with available:false has not necessarily failed: inspect its
configured state and run an appropriate authorised compiler request before
reporting an installed backend as unavailable.

Domain tools accept native arguments inside `parameters`. They return an issued
`jobId`; poll `cy_get_job` until terminal, then inspect `outputs` or the structured
error. Cancel your own job through `cy_cancel_job`. Never guess IDs or revisions.
Do not put workspace sources into the SDK's temporary task-file lifecycle.

## Investigate efficiently

- Use the project index for project-wide lookups instead of shell grep/rg loops:
  `op_index_symbols` to find where a type, function, field, macro or console
  variable is declared (fuzzy/CamelHump names, `Scope::Name`, or an empty query
  with `paths` to list a folder), `op_index_outline` for one file's structure,
  and `op_search_text` for text, narrowed with `paths`/`extensions`, summarized
  with `output:"files"` and paged with `nextCursor`. Each match names its
  enclosing declaration; add `context` (2-4) to see the surrounding lines.
- `op_index_symbols` comes from a fast lexical scanner (no preprocessor or
  template resolution). When a task needs every definition of a name, confirm
  the list with one `op_search_text` for that name before answering.
- A `search_text` result with `engine:"index"` and `coverage.filteredByIndex:true`
  already covers every indexed file (same matches as ripgrep with the Core's
  ignore rules): do not repeat that search with shell rg. `engine:"scan"` or a
  `fallback` means the index was not used; `coverage.complete:false` lists what
  was skipped.
- Quote code with `op_index_read` instead of shell reads: `symbol` returns the
  whole body of a function or type found by the index (`"FCCDManager::Init"`),
  `ranges` reads up to 20 line ranges at once, each with the functions/types it
  crosses. Use `op_index_outline` to locate lines in a large file.
- Batch work to save round trips: one `op_search_text` with `queries` (up to 10)
  instead of several searches, one `op_index_read` with `ranges` or `symbol`
  instead of several reads, generous `count` rather than many small pages. The first use on a large project builds
  the index (reported cost); later sessions reload it in seconds.
- Narrow by project, path, document or symbol before requesting large results.
  Page `workspace_files` using both `generation` and `indexRevision` guards.
- Read live documents and use their current revision and zero-based UTF-16
  positions for definition, declaration, type-definition, references, hover,
  symbols and hierarchies. Check actual backend capabilities first.
- For C/C++, use the real compilation database and generated headers. Unreal
  engine-only roots require real UBT compiler commands; do not fabricate flags.
- Prepare the relevant scope with `analysis_prepare`. Follow its continuation
  while pending and page metadata with `analysis_results`. These checkpoints
  remain in the current process unless the advertised contract supports persist.
  For a durable compilation campaign, opt into persist:true, retain its issued
  checkpoint and discover saved progress with analysis_checkpoints. Restore
  through analysis_prepare checkpoint.
  Restored prefix diagnostics are unknown historical-checkpoint observations,
  never current compiler proof. Observe historicalObserved/currentPassObserved,
  per-file errors and currentPath/currentFileMilliseconds. Read live documents
  before using revisions; unopened failures have revision zero. A changed compiler
  context needs a fresh inventory.
- Distinguish a compiler result, a partial index and a textual candidate in the
  answer. A completed preparation does not certify exhaustive Unreal coverage.

## Large text and previews

When an ordinary read exceeds its buffer policy, discover `documents_read_page`
and use its native paged profile. Continue with `nextOffset` and the current
revision. Offsets are UTF-8 bytes, not the UTF-16 positions of ordinary LSP calls.
For line-based reading, discover `documents_read_lines`: lines are zero-based,
characters are UTF-16, an initial BOM is excluded, and byte cursors remain separate.
Use its nextLine/nextCharacter with the current revision; a long line spans pages.
`documents_save_paged_as` publishes a new workspace file and rebinds the identity;
it preserves the original, refuses overwrites and invalidates old path-bound plans.
Save or close paged edits before compiler queries: there is no LSP overlay.
`analysis_inspect` can use a clean paged C/C++ revision and a real compiler command
without loading the whole source into RAM. A file cannot be open in both profiles; never discard an existing dirty buffer
merely to switch storage. Paged text currently has no LSP overlay. Explicit durable recovery is available
when advertised: `documents_checkpoint_paged`, then after restart discover
`documents_paged_checkpoints`, inspect `documents_preview_paged_checkpoint`, reopen
the original and prepare `documents_restore_paged_checkpoint` on its live revision.
Review/Apply/Undo remain explicit; checkpoints never save sources. Changed disk
baselines require reconciliation. There is no automatic per-keystroke checkpoint.

Prepare a bounded byte edit with `documents_prepare_byte_edit`, then use the same
`changes_preview/apply/undo` owner. Large previews need independent before/after
byte cursors and bounded pages; follow the returned next cursors. Neither Apply
nor Undo saves a source. Explicit publication uses `documents_save_paged` and
current revision; close uses `documents_close_paged`. Inspect canCancel: initial
import/save and transaction commit are currently noncancellable. Keep source
paths outside the SDK temporary task lifecycle. Read the maintained
`app/Docs/LargeProjects.md` for storage/backend constraints.

## Debugging evidence

Use the advertised DAP capabilities and current session/stop/thread guards.
A stopped event can precede the thread/stack inspection; wait for actual rows.
Exception-filter acknowledgement is not a verified binding. Inspect
`exceptionBreakpointStatus` when present, including every verified/message row.
Do not claim an exception stop from an enabled checkbox or advertised filter.
On a reviewed MSVC target, `_CxxThrowException` is an explicit function-breakpoint
recipe, preserving the existing function list/expected guard; it is not a silent
fallback or native exceptionInfo. A local attach/detach result proves that local
target only, not remote debugging or other adapters.

## Change code

Prefer a compiler-backed rename/code action when supported. Prepare and inspect
the issued plan, then use `changes_preview`, `changes_apply` and `changes_undo`
with the exact required guards. Keep saving explicit. Re-read after conflicts;
do not silently substitute a global text replacement for semantic refactoring.
Use live buffers consistently; a separate standalone MCP host does not share
the IDE's unsaved buffers. Attach to its shared service when that is required.

Operation permissions remain authoritative. Build/process, VCS, plugin and
settings writes are distinct from source edits. A skill grants no permissions.
If an antivirus or approval system blocks the host, report the actual failure;
do not disable protection, relocate a flagged executable or rebuild in a loop.

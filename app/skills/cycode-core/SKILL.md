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

Domain tools accept native arguments inside `parameters`. They return an issued
`jobId`; poll `cy_get_job` until terminal, then inspect `outputs` or the structured
error. Cancel your own job through `cy_cancel_job`. Never guess IDs or revisions.
Do not put workspace sources into the SDK's temporary task-file lifecycle.

## Investigate efficiently

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

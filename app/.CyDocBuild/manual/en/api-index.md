---
id: api-index
title: Project index
description: Public operations for Project index.
section: API and extensions
parent: api-links
order: 124
revision: 2
---

Public operations for Project index.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`index_status`](doc:operation-index-status-index-status) — Report the persistent project index without starting it — phase (idle, loading, reconciling, enumerating, indexing, persisting, compacting, ready, failed, disabled), storage directory, generation, files, symbols, shard and byte counts, skipped binary/oversize files, overlay of changes applied since the last generation, watcher backend/revision and the last build/load/reconcile/compaction timings.
- [`index_prepare`](doc:operation-index-prepare-index-prepare) — Load the persisted project index (reconciling files changed since it was written) or build it on first use, then wait up to waitMs (default 10 min).
- [`index_symbols`](doc:operation-index-symbols-index-symbols) — Find declarations across the whole project from the persistent index, Rider-style — exact, prefix, CamelHump initials (FPBDJS), substring and subsequence ranking; "Scope::Name" or "Scope.Name" also filters by enclosing scope.
- [`index_read`](doc:operation-index-read-index-read) — Read numbered lines in one call — one range (path, line, count), up to 20 ranges (ranges; an unreadable range is reported in place with error), or the whole body of a declaration found by the index (symbol, e.g.
- [`index_outline`](doc:operation-index-outline-index-outline) — Summarize one file structure without clangd — nested declarations (id/parent, kind, name, scope, line range, signature), counts per kind, #include/import targets, line and byte totals.

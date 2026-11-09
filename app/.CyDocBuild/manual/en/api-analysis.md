---
id: api-analysis
title: Analysis preparation
description: Public operations for Analysis preparation.
section: API and extensions
parent: api-links
order: 123
revision: 2
---

Public operations for Analysis preparation.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`analysis_status`](doc:operation-analysis-status-analysis-status) — Read aggregate native semantic-preparation progress without starting work.
- [`analysis_prepare`](doc:operation-analysis-prepare-analysis-prepare) — Prepare explicit existing sources without a fixed file-count ceiling, or enumerate the active C++ compilation database (optional workspace pathPrefix).
- [`analysis_results`](doc:operation-analysis-results-analysis-results) — Read a metadata page from the latest native preparation checkpoint.

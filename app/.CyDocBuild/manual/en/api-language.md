---
id: api-language
title: Language services
description: Public operations for Language services.
section: API and extensions
parent: api-links
order: 103
revision: 2
---

Public operations for Language services.

These contracts describe the native service exposed by the official SDK/MCP integration. Availability depends on the configured backend, project and permissions. Consult the [common contract](doc:api-links#contract) for revisions, previews and asynchronous jobs.

## Operations {#operations}

- [`language_hierarchy_prepare`](doc:operation-language-hierarchy-prepare-language-hierarchy-prepare) — Prepare an actual LSP call/type hierarchy at a workspace document UTF-16 position and current revision.
- [`language_hierarchy_expand`](doc:operation-language-hierarchy-expand-language-hierarchy-expand) — Expand an issued hierarchy item using its original server and opaque data.
- [`language_hierarchy_status`](doc:operation-language-hierarchy-status-language-hierarchy-status) — Check whether a cached hierarchy context is still valid without sending a language-server request.
- [`language_symbols`](doc:operation-language-symbols-language-symbols) — Search actual configured project symbols, optionally filtered to types.
- [`language_symbol_resolve`](doc:operation-language-symbol-resolve-language-symbol-resolve) — Resolve the location.range of one issued URI-only workspace symbol through its original backend.
- [`language_document_symbols`](doc:operation-language-document-symbols-language-document-symbols) — Read document structure from the live buffer, preserving hierarchy as snapshot-local node/parent/depth and fullRange alongside the legacy navigation range.
- [`language_rename`](doc:operation-language-rename-language-rename) — Prepare a semantic rename preview across references.
- [`language_format`](doc:operation-language-format-language-format) — Prepare formatting edits from the selected language server.
- [`language_configure`](doc:operation-language-configure-language-configure) — Enable, restart or disable a built-in or plugin language server in this trusted workspace.
- [`language_preferences`](doc:operation-language-preferences-language-preferences) — Read and refresh per-workspace language preferences from application-owned local storage.
- [`language_preferences_forget`](doc:operation-language-preferences-forget-language-preferences-forget) — Forget one saved language preference with an expected revision.
- [`language_status`](doc:operation-language-status-language-status) — Inspect all shared language servers, availability, advertised methods and published UTF-16 diagnostics.
- [`language_highlight`](doc:operation-language-highlight-language-highlight) — Read semantic color classifications from the shared language server and current live document.
- [`language_complete`](doc:operation-language-complete-language-complete) — Request completion items from live buffers.
- [`language_completion_resolve`](doc:operation-language-completion-resolve-language-completion-resolve) — Resolve an item issued by language_complete using the actual server.
- [`language_completion_prepare`](doc:operation-language-completion-prepare-language-completion-prepare) — Prepare one atomic live-buffer transaction for an issued completion plus its additional imports.
- [`language_code_actions`](doc:operation-language-code-actions-language-code-actions) — List actual language-server fixes/refactorings for a live UTF-16 range and expectedRevision.
- [`language_code_action_prepare`](doc:operation-language-code-action-prepare-language-code-action-prepare) — Resolve if needed and prepare an issued code action as a shared native change plan.
- [`language_hover`](doc:operation-language-hover-language-hover) — Read bounded symbol documentation from the actual language server at a zero-based UTF-16 position.
- [`language_signature`](doc:operation-language-signature-language-signature) — Read actual callable signatures, overloads, active parameter and documentation at a zero-based UTF-16 position.
- [`language_declaration`](doc:operation-language-declaration-language-declaration) — Read actual server-provided declaration locations from the live UTF-16 source position.
- [`language_type_definition`](doc:operation-language-type-definition-language-type-definition) — Read actual server-provided type definition locations from the live UTF-16 source position.
- [`language_implementation`](doc:operation-language-implementation-language-implementation) — Read actual server-provided implementation locations from the live UTF-16 source position.
- [`language_document_highlights`](doc:operation-language-document-highlights-language-document-highlights) — Read in-document occurrences from the actual documentHighlight provider.
- [`language_definition`](doc:operation-language-definition-language-definition) — Find definitions using live buffers and actual project references.
- [`language_references`](doc:operation-language-references-language-references) — Find references using shared servers and unsaved buffers.

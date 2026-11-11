---
id: api-changes
title: Transactions de modification
description: Opérations publiques pour Transactions de modification.
section: API et extensions
parent: api-links
order: 115
sourceRevision: 2
---

Opérations publiques pour Transactions de modification.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`changes_preview`](doc:operation-changes-preview-changes-preview) — Lire un aperçu immuable avant/après. Les gros aperçus exigent des pages avec beforeOffset/afterOffset en octets indépendants et length ; suivre nextBeforeOffset/nextAfterOffset. Les éditions ordinaires restent UTF-16 ; celles des documents paginés utilisent les octets UTF-8.
- [`changes_apply`](doc:operation-changes-apply-changes-apply) — Appliquer un ensemble de changements préparé après validation des révisions.
- [`changes_undo`](doc:operation-changes-undo-changes-undo) — Annuler un ensemble de changements appliqué et encore annulable.

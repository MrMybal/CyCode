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
- [`changes_status`](doc:operation-changes-status-changes-status) — Lire un plan natif émis et son dernier reçu de commit pour rapprocher une réponse perdue, sans relire le texte courant.
- [`changes_history_status`](doc:operation-changes-history-status-changes-history-status) — Consulter les têtes Undo/Redo partagées, leur révision et les limites de rétention du Core.
- [`changes_history_prepare`](doc:operation-changes-history-prepare-changes-history-prepare) — Préparer l’annulation ou le rétablissement natif avec les identités et révisions courantes de tous les documents ; examiner puis appliquer explicitement le plan.
- [`changes_apply`](doc:operation-changes-apply-changes-apply) — Appliquer un ensemble de changements préparé après validation des révisions.
- [`changes_undo`](doc:operation-changes-undo-changes-undo) — Annuler un ensemble de changements appliqué et encore annulable.

---
id: api-recovery
title: Récupération des documents
description: Opérations publiques pour Récupération des documents.
section: API et extensions
parent: api-links
order: 118
sourceRevision: 2
---

Opérations publiques pour Récupération des documents.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`recovery_status`](doc:operation-recovery-status-recovery-status) — Consulter l’état du service de récupération des documents.
- [`recovery_list`](doc:operation-recovery-list-recovery-list) — Lister les instantanés de documents récupérables.
- [`recovery_preview`](doc:operation-recovery-preview-recovery-preview) — Prévisualiser un instantané de récupération sans modifier le document.
- [`recovery_restore`](doc:operation-recovery-restore-recovery-restore) — Restaurer un instantané de récupération sélectionné.
- [`recovery_discard`](doc:operation-recovery-discard-recovery-discard) — Supprimer un instantané de récupération sélectionné.
- [`recovery_flush`](doc:operation-recovery-flush-recovery-flush) — Demander l’écriture des instantanés de récupération en attente.

---
id: api-resources
title: Ressources du projet
description: Opérations publiques pour Ressources du projet.
section: API et extensions
parent: api-links
order: 108
sourceRevision: 2
---

Opérations publiques pour Ressources du projet.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`resources_preview`](doc:operation-resources-preview-resources-preview) — Préparer une opération sur les ressources de l’espace de travail.
- [`resources_apply`](doc:operation-resources-apply-resources-apply) — Appliquer une opération de ressources précédemment préparée.
- [`resources_undo`](doc:operation-resources-undo-resources-undo) — Annuler une opération de ressources prise en charge.
- [`resources_trash`](doc:operation-resources-trash-resources-trash) — Consulter les ressources conservées dans la corbeille de l’application.
- [`resources_children`](doc:operation-resources-children-resources-children) — Lister les enfants d’un dossier de ressources.
- [`resources_events`](doc:operation-resources-events-resources-events) — Lire les changements observés dans les ressources de l’espace de travail.

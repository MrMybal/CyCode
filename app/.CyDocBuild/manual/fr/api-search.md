---
id: api-search
title: Recherche de texte
description: Opérations publiques pour Recherche de texte.
section: API et extensions
parent: api-links
order: 117
sourceRevision: 2
---

Opérations publiques pour Recherche de texte.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`search_text`](doc:operation-search-text-search-text) — Rechercher le texte du projet avec les tampons non sauvegardés prioritaires. Les documents paginés modifiés sont copiés progressivement dans des instantanés privés pour la recherche littérale ou regex, puis leur révision est revérifiée. Aucun chargement complet paginé en RAM ni sauvegarde source ; budgets de sortie/temps et annulation ripgrep conservés.

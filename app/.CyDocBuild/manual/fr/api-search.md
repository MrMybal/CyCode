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

- [`search_text`](doc:operation-search-text-search-text) — Rechercher le texte du projet avec l’index natif persistant du projet, une requête ou jusqu’à 10 requêtes par appel (queries), chaque résultat avec la fonction ou le type qui le contient et, au besoin, des lignes de contexte. Les listes de trigrammes choisissent les fichiers candidats, puis chaque résultat est revérifié sur le contenu actuel (les tampons non enregistrés priment). Filtres paths et extensions, pagination par nextCursor, output=files pour une ligne par fichier, maxBytes pour borner une page. Tant que l’index n’est pas prêt, la recherche passe par ripgrep et l’indique (engine, fallback) ; une application interactive ne l’attend jamais. Les documents paginés modifiés imposent le chemin ripgrep. Les fichiers du projet ne sont pas modifiés.

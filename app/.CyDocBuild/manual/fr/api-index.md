---
id: api-index
title: Index du projet
description: Opérations publiques pour Index du projet.
section: API et extensions
parent: api-links
order: 124
sourceRevision: 2
---

Opérations publiques pour Index du projet.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`index_status`](doc:operation-index-status-index-status) — Indiquer l’état de l’index persistant du projet sans le démarrer  — phase, préparation en cours, nombre de fichiers et de symboles, taille, génération, dossier de stockage et dernier rapprochement.
- [`index_prepare`](doc:operation-index-prepare-index-prepare) — Charger l’index persistant du projet (en rapprochant les fichiers modifiés depuis son écriture) ou le construire à la première utilisation, puis attendre jusqu’à waitMs. rebuild=true reconstruit une nouvelle génération. Le stockage reste hors du projet.
- [`index_symbols`](doc:operation-index-symbols-index-symbols) — Trouver des déclarations dans tout le projet à partir de l’index persistant, à la manière de Rider  — correspondance exacte, préfixe, initiales CamelHump, sous-chaîne et sous-séquence, Scope::Nom, filtres de type, de chemin et d’extension ; une requête vide avec paths liste un dossier. Analyse lexicale rapide, sans préprocesseur ni résolution des templates.
- [`index_read`](doc:operation-index-read-index-read) — Lire des lignes numérotées avec les déclarations qu’elles traversent  — une plage, jusqu’à 20 plages (ranges) ou le corps d’un symbole trouvé par l’index (symbol). Les lignes très longues sont coupées et chaque plage signale ses erreurs séparément.
- [`index_outline`](doc:operation-index-outline-index-outline) — Donner la structure d’un fichier (espaces de noms, types, fonctions, champs, macros Unreal) avec lignes de début et de fin et ses inclusions, à partir du tampon ouvert ou du disque.

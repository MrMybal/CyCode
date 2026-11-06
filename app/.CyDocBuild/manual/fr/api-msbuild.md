---
id: api-msbuild
title: Solutions et MSBuild
description: Opérations publiques pour Solutions et MSBuild.
section: API et extensions
parent: api-links
order: 110
sourceRevision: 2
---

Opérations publiques pour Solutions et MSBuild.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`msbuild_inspect`](doc:operation-msbuild-inspect-msbuild-inspect) — Examiner une solution ou un projet MSBuild.
- [`msbuild_tools`](doc:operation-msbuild-tools-msbuild-tools) — Détecter les outils MSBuild disponibles.
- [`msbuild_build`](doc:operation-msbuild-build-msbuild-build) — Compiler une solution ou un projet MSBuild.
- [`msbuild_rebuild`](doc:operation-msbuild-rebuild-msbuild-rebuild) — Recompiler entièrement la cible MSBuild choisie.
- [`msbuild_clean`](doc:operation-msbuild-clean-msbuild-clean) — Nettoyer les sorties de la cible MSBuild choisie.
- [`msbuild_restore`](doc:operation-msbuild-restore-msbuild-restore) — Restaurer les dépendances du projet MSBuild.
- [`msbuild_evaluate`](doc:operation-msbuild-evaluate-msbuild-evaluate) — Évaluer les propriétés et les éléments publics d’un projet MSBuild.
- [`msbuild_prepare`](doc:operation-msbuild-prepare-msbuild-prepare) — Préparer les informations de projet nécessaires aux services de langage MSBuild.

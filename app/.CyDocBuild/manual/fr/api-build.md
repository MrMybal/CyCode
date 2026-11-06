---
id: api-build
title: Compilation CMake
description: Opérations publiques pour Compilation CMake.
section: API et extensions
parent: api-links
order: 102
sourceRevision: 2
---

Opérations publiques pour Compilation CMake.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`build_preset_plan`](doc:operation-build-preset-plan-build-preset-plan) — Préparer l’exécution d’un preset CMake et examiner ses arguments.
- [`build_preset_run`](doc:operation-build-preset-run-build-preset-run) — Exécuter le plan CMake précédemment préparé.
- [`build_inspect`](doc:operation-build-inspect-build-inspect) — Examiner les informations de compilation CMake du projet.
- [`build_presets`](doc:operation-build-presets-build-presets) — Lister les presets CMake disponibles et leurs relations.
- [`build_status`](doc:operation-build-status-build-status) — Consulter l’état de la compilation active.
- [`build_location`](doc:operation-build-location-build-location) — Résoudre un emplacement de diagnostic dans la sortie de compilation.
- [`build_output`](doc:operation-build-output-build-output) — Lire une portion bornée de la sortie de compilation.
- [`build_cancel`](doc:operation-build-cancel-build-cancel) — Demander l’arrêt du processus de compilation actif.
- [`build_configure`](doc:operation-build-configure-build-configure) — Configurer un projet CMake avec les outils disponibles.
- [`build_run`](doc:operation-build-run-build-run) — Compiler une cible CMake du projet.

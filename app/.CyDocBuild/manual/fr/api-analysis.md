---
id: api-analysis
title: Préparation de l’analyse
description: Opérations publiques pour Préparation de l’analyse.
section: API et extensions
parent: api-links
order: 123
sourceRevision: 2
---

Opérations publiques pour Préparation de l’analyse.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`analysis_inspect`](doc:operation-analysis-inspect-analysis-inspect) — Inspecter une unité C++ sauvegardée avec clang-tidy et sa vraie commande de compilation. Les profils classiques et paginés propres sont acceptés avec leur révision ; les gros fichiers utilisent des empreintes progressives sans copie intégrale du texte. Refuser les tampons paginés modifiés/occupés ; signaler les résultats périmés, tronqués ou interrompus, sans correction ou écriture source.
- [`analysis_status`](doc:operation-analysis-status-analysis-status) — Consulter la progression native, le fichier courant et son temps de traitement sans démarrer une nouvelle analyse.
- [`analysis_prepare`](doc:operation-analysis-prepare-analysis-prepare) — Analyser progressivement les sources ou les unités C++, enregistrer explicitement la progression et reprendre un instantané ; les preuves historiques sont invalidées, les erreurs isolées signalées et la couverture reste explicite.
- [`analysis_checkpoints`](doc:operation-analysis-checkpoints-analysis-checkpoints) — Lister les progressions d’analyse enregistrées dans cet espace de travail, sans lancer d’analyse ni réutiliser les diagnostics historiques comme preuve actuelle.
- [`analysis_results`](doc:operation-analysis-results-analysis-results) — Consulter une page des observations sémantiques de la session native, sans assimiler ces résultats à une couverture exhaustive.

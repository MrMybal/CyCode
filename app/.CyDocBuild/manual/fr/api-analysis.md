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

- [`analysis_inspect`](doc:operation-analysis-inspect-analysis-inspect) — Inspecter une unité C++ enregistrée avec clang-tidy et sa vraie commande de compilation, sans modifier les sources ; résultats protégés et annulation du processus.
- [`analysis_status`](doc:operation-analysis-status-analysis-status) — Consulter la progression de préparation sémantique sans démarrer de travail.
- [`analysis_prepare`](doc:operation-analysis-prepare-analysis-prepare) — Analyser progressivement les sources ou les unités de compilation C++, avec reprise par lots et mesure explicite de couverture.
- [`analysis_results`](doc:operation-analysis-results-analysis-results) — Consulter une page des observations sémantiques de la session native, sans assimiler ces résultats à une couverture exhaustive.

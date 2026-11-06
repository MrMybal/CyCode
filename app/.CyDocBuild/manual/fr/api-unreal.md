---
id: api-unreal
title: Unreal Engine
description: Opérations publiques pour Unreal Engine.
section: API et extensions
parent: api-links
order: 100
sourceRevision: 2
---

Opérations publiques pour Unreal Engine.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`unreal_automation_plan`](doc:operation-unreal-automation-plan-unreal-automation-plan) — Préparer un aperçu d’une cuisson ou d’un empaquetage Unreal Engine 5 sans exécuter les outils.
- [`unreal_automation_run`](doc:operation-unreal-automation-run-unreal-automation-run) — Exécuter l’aperçu Unreal Automation précédemment examiné et encore valide.
- [`unreal_prepare`](doc:operation-unreal-prepare-unreal-prepare) — Préparer les informations de compilation et les services C++ du projet Unreal.
- [`unreal_inspect`](doc:operation-unreal-inspect-unreal-inspect) — Examiner le projet Unreal, ses modules, ses cibles et le moteur associé.
- [`unreal_artifacts`](doc:operation-unreal-artifacts-unreal-artifacts) — Lister les exécutables et les artefacts Unreal découverts pour le projet.
- [`unreal_launch_profile`](doc:operation-unreal-launch-profile-unreal-launch-profile) — Préparer un profil de lancement Unreal à partir d’une cible découverte.
- [`unreal_generate`](doc:operation-unreal-generate-unreal-generate) — Générer les fichiers de projet Unreal avec les outils du moteur.
- [`unreal_database`](doc:operation-unreal-database-unreal-database) — Générer la base de commandes de compilation C++ avec UnrealBuildTool.
- [`unreal_build`](doc:operation-unreal-build-unreal-build) — Compiler une cible Unreal avec la configuration et la plateforme choisies.

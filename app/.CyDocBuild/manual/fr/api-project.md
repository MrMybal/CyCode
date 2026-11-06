---
id: api-project
title: Tâches du projet
description: Opérations publiques pour Tâches du projet.
section: API et extensions
parent: api-links
order: 109
sourceRevision: 2
---

Opérations publiques pour Tâches du projet.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`project_inspect`](doc:operation-project-inspect-project-inspect) — Examiner le modèle et les actions disponibles du projet courant.
- [`project_status`](doc:operation-project-status-project-status) — Consulter l’état d’une tâche du projet.
- [`project_output`](doc:operation-project-output-project-output) — Lire une portion bornée de la sortie d’une tâche du projet.
- [`project_configure`](doc:operation-project-configure-project-configure) — Configurer le projet avec l’adaptateur disponible.
- [`project_build`](doc:operation-project-build-project-build) — Lancer la compilation du projet avec son adaptateur.
- [`project_test`](doc:operation-project-test-project-test) — Lancer la commande de tests déclarée par le projet.
- [`project_install`](doc:operation-project-install-project-install) — Lancer l’installation des dépendances avec l’outil déclaré par le projet.
- [`project_script`](doc:operation-project-script-project-script) — Exécuter un script explicitement déclaré par le projet.
- [`project_launch`](doc:operation-project-launch-project-launch) — Lancer une cible exécutable découverte dans le projet.
- [`project_debug_target`](doc:operation-project-debug-target-project-debug-target) — Préparer une cible de débogage à partir du projet.
- [`project_cancel`](doc:operation-project-cancel-project-cancel) — Demander l’arrêt de la tâche du projet en cours.

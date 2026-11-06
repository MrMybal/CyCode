---
id: api-packages
title: Paquets d’extension
description: Opérations publiques pour Paquets d’extension.
section: API et extensions
parent: api-links
order: 104
sourceRevision: 2
---

Opérations publiques pour Paquets d’extension.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`packages_list`](doc:operation-packages-list-packages-list) — Lister les paquets d’extension connus du gestionnaire.
- [`packages_inspect`](doc:operation-packages-inspect-packages-inspect) — Examiner un paquet d’extension et ses métadonnées.
- [`packages_install`](doc:operation-packages-install-packages-install) — Installer un paquet d’extension selon les règles de confiance configurées.
- [`packages_remove`](doc:operation-packages-remove-packages-remove) — Retirer un paquet d’extension installé.
- [`packages_rollback`](doc:operation-packages-rollback-packages-rollback) — Restaurer la version précédente d’un paquet lorsque celle-ci est disponible.
- [`packages_clean`](doc:operation-packages-clean-packages-clean) — Nettoyer les éléments de paquets autorisés par le gestionnaire.

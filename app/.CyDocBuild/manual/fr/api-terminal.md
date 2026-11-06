---
id: api-terminal
title: Terminaux
description: Opérations publiques pour Terminaux.
section: API et extensions
parent: api-links
order: 113
sourceRevision: 2
---

Opérations publiques pour Terminaux.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`terminal_create`](doc:operation-terminal-create-terminal-create) — Créer une session de terminal native.
- [`terminal_list`](doc:operation-terminal-list-terminal-list) — Lister les sessions de terminal natives.
- [`terminal_read`](doc:operation-terminal-read-terminal-read) — Lire une portion bornée de la sortie d’un terminal.
- [`terminal_write`](doc:operation-terminal-write-terminal-write) — Envoyer du texte à une session de terminal explicitement choisie.
- [`terminal_key`](doc:operation-terminal-key-terminal-key) — Envoyer une touche prise en charge à une session de terminal.
- [`terminal_resize`](doc:operation-terminal-resize-terminal-resize) — Modifier les dimensions de la session de terminal.
- [`terminal_clear`](doc:operation-terminal-clear-terminal-clear) — Effacer la sortie conservée pour une session de terminal.
- [`terminal_close`](doc:operation-terminal-close-terminal-close) — Fermer une session de terminal et arrêter son processus.

---
id: api-documents
title: Documents
description: Opérations publiques pour Documents.
section: API et extensions
parent: api-links
order: 111
sourceRevision: 2
---

Opérations publiques pour Documents.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`documents_reload`](doc:operation-documents-reload-documents-reload) — Recharger un document depuis le disque en vérifiant les changements locaux.
- [`documents_list`](doc:operation-documents-list-documents-list) — Lister les documents ouverts et leurs révisions.
- [`documents_read`](doc:operation-documents-read-documents-read) — Lire le contenu courant d’un document natif ouvert.
- [`documents_create`](doc:operation-documents-create-documents-create) — Créer un document de projet avec le contenu fourni.
- [`documents_create_untitled`](doc:operation-documents-create-untitled-documents-create-untitled) — Créer un document sans fichier dans la session native.
- [`documents_save_as`](doc:operation-documents-save-as-documents-save-as) — Enregistrer un document sous un chemin explicitement choisi.
- [`documents_replace`](doc:operation-documents-replace-documents-replace) — Remplacer le contenu d’un document avec validation de sa révision.
- [`documents_save`](doc:operation-documents-save-documents-save) — Enregistrer les changements du document sur le disque.
- [`documents_close`](doc:operation-documents-close-documents-close) — Fermer un document selon la politique de changements non enregistrés.

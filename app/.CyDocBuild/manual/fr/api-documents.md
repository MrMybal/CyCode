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
- [`documents_read_page`](doc:operation-documents-read-page-documents-read-page) — Ouvrir un document UTF-8 paginé natif et lire des plages de 4 à 262 144 octets sans charger tout le fichier en mémoire. La suite exige la révision vivante. Ni coordonnées UTF-16, ni LSP, ni sauvegarde implicite ; import initial non annulable.
- [`documents_checkpoint_paged`](doc:operation-documents-checkpoint-paged-documents-checkpoint-paged) — Persister explicitement un instantané paginé privé et son manifeste atomique, sans sauvegarder le fichier source.
- [`documents_paged_checkpoints`](doc:operation-documents-paged-checkpoints-documents-paged-checkpoints) — Découvrir les checkpoints paginés persistants par leurs métadonnées, sans supprimer de données ni présenter les anciennes révisions comme des droits d’édition courants.
- [`documents_preview_paged_checkpoint`](doc:operation-documents-preview-paged-checkpoint-documents-preview-paged-checkpoint) — Vérifier l’empreinte du contenu d’un checkpoint paginé et consulter une page UTF-8 bornée avec l’état de la référence disque d’origine.
- [`documents_restore_paged_checkpoint`](doc:operation-documents-restore-paged-checkpoint-documents-restore-paged-checkpoint) — Préparer la restauration d’un checkpoint paginé dans le tampon vivant avec sa révision actuelle, puis réutiliser aperçu, Apply et Undo sans sauvegarde implicite.
- [`documents_read_lines`](doc:operation-documents-read-lines-documents-read-lines) — Lire un document paginé par ligne et colonne UTF-16 avec des curseurs distincts en octets, un index natif parcimonieux et une révision courante.
- [`documents_save_paged_as`](doc:operation-documents-save-paged-as-documents-save-paged-as) — Sauvegarder progressivement un document paginé dans un nouveau fichier du projet et réassocier son identité, sans écraser une destination ou modifier le fichier original.
- [`documents_prepare_byte_edit`](doc:operation-documents-prepare-byte-edit-documents-prepare-byte-edit) — Préparer une modification en octets UTF-8 du document paginé vivant, avec sa révision. Les morceaux immuables utilisent le stockage privé natif ; examiner puis appliquer/annuler avec les transactions communes, sans sauvegarde source.
- [`documents_save_paged`](doc:operation-documents-save-paged-documents-save-paged) — Sauvegarder explicitement la révision du document paginé par écriture atomique progressive. Vérifier l’empreinte disque, les morceaux privés et la lecture seule ; publication non annulable, sans Enregistrer sous ni récupération après crash.
- [`documents_close_paged`](doc:operation-documents-close-paged-documents-close-paged) — Fermer le document paginé avec sa révision. Les modifications non sauvegardées exigent discard=true ; aucune sauvegarde ou suppression de la source. Les aperçus vivants peuvent conserver leurs morceaux privés.
- [`documents_read`](doc:operation-documents-read-documents-read) — Lire le contenu courant d’un document natif ouvert.
- [`documents_create`](doc:operation-documents-create-documents-create) — Créer un document de projet avec le contenu fourni.
- [`documents_create_untitled`](doc:operation-documents-create-untitled-documents-create-untitled) — Créer un document sans fichier dans la session native.
- [`documents_save_as`](doc:operation-documents-save-as-documents-save-as) — Enregistrer un document sous un chemin explicitement choisi.
- [`documents_replace`](doc:operation-documents-replace-documents-replace) — Remplacer le contenu d’un document avec validation de sa révision.
- [`documents_save`](doc:operation-documents-save-documents-save) — Enregistrer les changements du document sur le disque.
- [`documents_close`](doc:operation-documents-close-documents-close) — Fermer un document selon la politique de changements non enregistrés.

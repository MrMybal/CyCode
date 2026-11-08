---
id: api-snippets
title: Extraits personnalisés
description: Opérations publiques pour Extraits personnalisés.
section: API et extensions
parent: api-links
order: 121
sourceRevision: 2
---

Opérations publiques pour Extraits personnalisés.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`snippets_complete`](doc:operation-snippets-complete-snippets-complete) — Fusionner les catalogues du projet, personnel configuré par l’hôte, puis des plugins ABI2 actifs. Les doublons conservent leur provenance. Revérifie le contenu enregistré et le registre avant émission/préparation ; le stockage personnel invalide est signalé. Les contrôles de sélection, champs liés, limites de 512 propositions et transactions natives restent communs.
- [`snippets_user_read`](doc:operation-snippets-user-read-snippets-user-read) — Lire le catalogue personnel fixe configuré par l’hôte, partagé entre les projets de cette installation. La lecture ne crée aucun fichier et retourne disponibilité, validité, révision opaque, génération locale, texte et erreur. Un hôte sans configuration ne lit aucun stockage personnel.
- [`snippets_user_draft`](doc:operation-snippets-user-draft-snippets-user-draft) — Capturer une sélection UTF-16 non vide d’un document déjà ouvert dans un brouillon d’extrait personnel littéral. Vérifie révision, plage, échappement et limites du catalogue ; conserve les valeurs existantes, génère nom/préfixe/portée et met en forme le JSON. Retourne le catalogue initial et le brouillon sans créer ni enregistrer de fichier, verrou ou plan. Enregistrez séparément avec catalogue.revision et settings.write après examen.
- [`snippets_user_save`](doc:operation-snippets-user-save-snippets-user-save) — Valider puis enregistrer explicitement le catalogue personnel avec settings.write, distinct de workspace.write. Vérifie la révision sous verrou non bloquant conservé, remplace atomiquement et confirme le contenu. Aucun chemin fourni par l’appelant ; une erreur préserve le brouillon et exige une relecture, sans écrasement forcé.

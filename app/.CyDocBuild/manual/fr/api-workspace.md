---
id: api-workspace
title: Espace de travail
description: Opérations publiques pour Espace de travail.
section: API et extensions
parent: api-links
order: 116
sourceRevision: 2
---

Opérations publiques pour Espace de travail.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`workspace_describe`](doc:operation-workspace-describe-workspace-describe) — Décrire l’espace de travail natif et les services disponibles.
- [`workspace_counterparts`](doc:operation-workspace-counterparts-workspace-counterparts) — Rechercher les fichiers en-tête/source C/C++ associés à un document déjà ouvert, avec révision et identité attendues. Complète les voisins et agencements conventionnels par les mêmes noms de base de l’index de fichiers déjà à jour, sans LSP, processus, actualisation ni parcours récursif. Conserve les libellés locaux et signale les autres comme workspace-index. Au plus 128 chemins copiés et 128 vérifications au total. Retourne index (état, révision, génération), candidats et limites ; un cache absent, occupé ou périmé est écarté, un cache partiel reste signalé et tout changement pendant la requête empêche l’ouverture automatique. Revérifie aussi la source. Les noms ne prouvent pas une association sémantique et le cache respecte ses règles d’exclusion. Aucun fichier ouvert implicitement, modifié ou enregistré.
- [`workspace_include`](doc:operation-workspace-include-workspace-include) — Rechercher les fichiers du projet correspondant au #include littéral dans la directive logique du curseur, avec révision et identité source attendues. Utilise le texte ouvert non enregistré ; vérifie position UTF-16 et contexte lexical borné. Accepte commentaires blocs sur une ligne entre les jetons et continuations immédiates antislash-LF/CRLF ; le nom d’en-tête reste physique. Retourne curseur demandé et plage UTF-16 réelle de l’en-tête même sur une autre ligne, cible/type, candidats et état révisionné de l’index. Les guillemets essaient le chemin relatif puis, comme les chevrons, les suffixes du cache actuel. Aucun lancement ni rafraîchissement implicite ; limites préfixe 1 Mio, directive 8 Kio, cible 1024 octets et 128 vérifications. Macros, conditions, commentaires multilignes non échappés, ordre du compilateur et SDK externes ne sont pas résolus. Aucun fichier modifié, enregistré ou ouvert implicitement ; service ABI2 identique en lecture seule.
- [`workspace_files`](doc:operation-workspace-files-workspace-files) — Lister les fichiers indexés par pages bornées. Chaque page contient indexRevision et une generation positive obligatoire  — comparer les deux avant de combiner des pages, rejeter les paires mélangées puis recommencer. Une reconstruction manuelle avance la génération même sans changement de révision du système de fichiers. Ces compteurs sont locaux au processus, pas un identifiant de cliché figé.
- [`workspace_poll`](doc:operation-workspace-poll-workspace-poll) — Actualiser les informations natives de l’espace de travail.

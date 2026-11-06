---
id: refactoring
title: Complétion et refactorisation
description: Examinez la complétion, le renommage et les actions de code avant d’enregistrer.
section: Travailler dans CyCode
order: 60
parent: editing
sourceRevision: 2
---

## Compléter le code {#completion}

Utilisez Ctrl+Espace pour demander une complétion au service de langage actif. L’acceptation d’un élément peut insérer ses imports associés dans une seule transaction de l’éditeur. Parcourez les champs d’extrait avec Tab / Maj+Tab ; Échap termine l’extrait. Ctrl+Espace quitte l’extrait courant pour demander une autre complétion.

## Renommer et appliquer des actions {#rename}

1. Placez le curseur sur le symbole et choisissez Renommer, ou ouvrez les actions contextuelles de la plage courante.
2. Sélectionnez une proposition réelle du serveur. Les actions désactivées indiquent le contexte ou la capacité manquante.
3. Examinez les changements Avant/Après et les confirmations dans la vue de refactorisation.
4. Appliquez les changements aux textes en mémoire, puis enregistrez lorsque vous le souhaitez.

L’IDE refuse les modifications périmées, les chevauchements et les conflits sur disque. Une modification du code après l’aperçu peut demander une nouvelle requête. Le serveur détermine les refactorisations disponibles ; la coloration seule ne permet pas de renommer correctement les symboles.

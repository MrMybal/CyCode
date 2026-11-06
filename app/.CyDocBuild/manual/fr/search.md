---
id: search
title: Chercher du texte et des fichiers
description: Utilisez les recherches dans le document, le projet et partout.
section: Travailler dans CyCode
order: 80
sourceRevision: 2
---

## Choisir le périmètre {#find}

| Raccourci | Périmètre recherché |
| --- | --- |
| Ctrl+F | Le document texte courant |
| Ctrl+Maj+F | Le texte de l’espace de travail |
| Ctrl+Alt+F | Les noms de fichiers |
| Double Maj | Les fichiers, actions, fichiers récents et symboles disponibles |

La vue de recherche globale propose des options de texte et des filtres de fichiers. Sélectionnez un résultat pour consulter son emplacement. Les recherches dans le document et dans le projet conservent leurs périmètres distincts ; un onglet d’image ne possède pas de recherche de texte.

## Résultats et actions {#results}

Utilisez Haut/Bas et Entrée dans la recherche partout. Préfixez une requête avec `>` pour les actions, `@` pour les membres ou `#` pour les symboles. Les symboles nécessitent un serveur actif et son index du projet. Consultez la [navigation dans le code](doc:navigation#symbols) pour les définitions, références et occurrences.

Une liste peut conserver une observation antérieure après la modification d’un fichier. Actualisez la recherche si un emplacement n’est plus courant. Un message du compilateur reste copiable même si sa position ne peut pas être associée à un fichier de l’espace de travail.

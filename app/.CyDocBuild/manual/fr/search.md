---
id: search
title: Chercher du texte et des fichiers
description: Utilisez les recherches dans le document, le projet et partout.
section: Travailler dans CyCode
order: 80
sourceRevision: 3
---

## Choisir le périmètre {#find}

| Raccourci | Périmètre recherché |
| --- | --- |
| Ctrl+F | Le document texte courant |
| Ctrl+Maj+F | Le texte de l’espace de travail |
| Ctrl+Alt+F | Les noms de fichiers |
| Double Maj | Les fichiers, actions, fichiers récents et symboles disponibles |

La vue de recherche globale propose des options de texte et des filtres de fichiers. Sélectionnez un résultat pour consulter son emplacement. Les recherches dans le document et dans le projet conservent leurs périmètres distincts ; un onglet d’image ne possède pas de recherche de texte.


Le texte non enregistré remplace les résultats du disque pour ce document. La recherche globale sans distinction de casse suit les mêmes règles Unicode simples pour les textes enregistrés et non enregistrés : lettres accentuées, variantes du sigma grec et symbole kelvin sont reconnus sans modifier les colonnes de résultat exprimées en octets. Elle n’effectue pas de normalisation Unicode et ne transforme pas `ß` en `ss`. La recherche littérale avec distinction de casse compare les octets exacts.

Les recherches regex et Unicode sans distinction de casse utilisent des instantanés temporaires à noms numériques, limités à 128 buffers / 64 Mio, avec les limites de sortie et l’annulation existantes. Elles n’enregistrent jamais les sources du projet. Les aperçus s’arrêtent sur un caractère UTF-8 complet dans une limite de 512 octets.

## Résultats et actions {#results}

Utilisez Haut/Bas et Entrée dans la recherche partout. Préfixez une requête avec `>` pour les actions, `@` pour les membres ou `#` pour les symboles. Les symboles nécessitent un serveur actif et son index du projet. Consultez la [navigation dans le code](doc:navigation#symbols) pour les définitions, références et occurrences.

Une liste peut conserver une observation antérieure après la modification d’un fichier. Actualisez la recherche si un emplacement n’est plus courant. Un message du compilateur reste copiable même si sa position ne peut pas être associée à un fichier de l’espace de travail.

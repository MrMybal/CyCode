---
id: editing
title: Modifier et enregistrer les fichiers
description: Gérez les onglets, les brouillons et les modifications externes.
section: Travailler dans CyCode
order: 40
sourceRevision: 2
---

## Documents et brouillons {#documents}

Ouvrez un fichier depuis Projet, la recherche par nom ou un glisser-déposer. L’action Nouveau crée un document sans nom. Modifiez-le normalement, puis utilisez Enregistrer sous pour choisir une destination dans l’espace de travail. Une référence de brouillon n’est pas un chemin sur disque.

Ctrl+S enregistre le document texte actif. Tout enregistrer traite les documents ouverts modifiés. Renommer un symbole ou appliquer une refactorisation modifie d’abord les textes en mémoire ; l’enregistrement reste une action distincte. Undo/Redo appartient à l’éditeur, pas aux commits Git ni à l’exécution du débogueur.

## Encodage et fins de ligne {#encoding}

Le service natif de documents de CyCode prend en charge UTF-8, avec ou sans BOM, et conserve les fins de ligne LF, CRLF ou CR détectées. Convertissez les autres encodages avec un outil de texte adapté avant de les modifier ici. Examinez un conflit avant d’écraser un fichier modifié ailleurs ; le rechargement peut abandonner des changements non enregistrés et demande une confirmation adaptée.

## Disposition de l’éditeur {#layout}

Utilisez les onglets, le repli des blocs, les retours visuels à la ligne et la taille du texte pour faciliter la lecture. Les curseurs et sélections multiples agissent sur le document en mémoire. Les textes sont limités à 4 Mio. Les [images](doc:images#preview) utilisent un aperçu en lecture seule avec d’autres commandes.

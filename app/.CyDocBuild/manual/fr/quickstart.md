---
id: quickstart
title: Ouvrir son premier projet
description: Ouvrez un dossier, une solution Visual Studio ou un projet Unreal.
section: Bien démarrer
order: 10
parent: overview
sourceRevision: 2
---

## Ouvrir un projet {#first-work}

1. Ouvrez CyCode. Dans le menu Fichier, ouvrez un dossier de travail ou utilisez l’action d’ouverture de solution pour un fichier `.sln`.
2. Sélectionnez la racine du projet, plutôt qu’un dossier de compilation. Pour Unreal, choisissez le `.uproject` ou sa solution.
3. Développez le panneau Projet et ouvrez un fichier source. Les entrées générées de la solution et les fichiers physiques de l’espace de travail sont des vues distinctes.
4. Sélectionnez la cible et la configuration avant de compiler. L’ouverture du projet n’installe pas son compilateur ou son moteur.
5. Enregistrez une modification avec Ctrl+S. Utilisez la [recherche globale](doc:search#find) pour trouver le code associé.

Pour CMake, Cargo, npm, Maven et Gradle, poursuivez avec les [outils de projet](doc:projects#projects). Les entrées Unreal générées manquantes peuvent être recréées avec les [outils Unreal](doc:unreal#generated).

## Fermer un espace de travail {#close}

Lisez les demandes Enregistrer / Ignorer / Annuler avant de fermer. Annuler conserve la session ouverte. Fermer un onglet non enregistré n’équivaut pas à supprimer un fichier du disque. Lancez une seconde instance pour garder un autre projet accessible.

---
id: projects
title: Compiler et exécuter des projets
description: Choisissez un adaptateur de projet, une cible et une configuration.
section: Travailler dans CyCode
order: 120
sourceRevision: 2
---

## Choisir le projet {#projects}

CyCode reconnaît les projets CMake, les solutions Visual Studio, les projets Unreal et les descripteurs Cargo, npm, Java autonome, Maven et Gradle. Ouvrez la racine du projet, puis utilisez la vue de compilation correspondante ou les tâches de projet. Sélectionnez le dossier, la configuration et la cible avant une action.

Les tâches graphiques enregistrent d’abord les documents modifiés de l’espace de travail. L’annulation d’un enregistrement empêche le lancement. Les opérations SDK natives exigent des documents enregistrés et réconciliés avant d’exécuter les tâches du projet.

## Projets Unreal {#unreal}

Unreal utilise son moteur, UnrealBuildTool et ses règles de cibles. L’ouverture par une solution ne le transforme pas en projet CMake ordinaire. Poursuivez avec la [configuration Unreal](doc:unreal#engine).

## Un petit projet C++ {#example}

Pour un espace CMake, configurez-le dans la vue CMake, consultez les cibles réelles, compilez un exécutable sélectionné et préparez son profil de débogage. La configuration exécute les scripts du projet auquel vous faites confiance. Consultez [CMake et MSBuild](doc:build#configure).

La sortie et Problèmes relient les messages reconnus du compilateur aux fichiers source. L’annulation demande l’arrêt des processus appartenant à la tâche ; elle ne supprime pas les résultats du projet.

---
id: unreal
title: Projets Unreal Engine
description: Ouvrez les solutions, régénérez les données du projet et préparez les lancements Unreal.
section: Travailler dans CyCode
order: 140
parent: projects
sourceRevision: 2
---

## Choisir le moteur {#engine}

Ouvrez le `.uproject` ou son `.sln`. Vérifiez la racine du moteur, sa version, la cible, la plateforme et la configuration détectées dans la vue Unreal. La complétion nécessite les bons en-têtes du moteur et le contexte de compilation ; les mots-clés C++ ordinaires ne suffisent pas à analyser les macros Unreal.

## Données générées du projet {#generated}

Utilisez la génération des fichiers de projet lorsque des entrées de solution sont absentes ou périmées. Générez la base de compilation destinée à clangd lorsque nécessaire. Choisissez une cible de jeu ou d’éditeur existante avant de compiler avec UnrealBuildTool. La génération et la compilation exécutent les outils du moteur installé auquel vous faites confiance.

## Lancer et créer un paquet {#launch}

Préparez un profil de lancement d’éditeur, de jeu ou de serveur non cuit, puis examinez son programme, sa carte, ses arguments et sa configuration dans le débogueur partagé. La compilation graphique avant débogage utilise la cible associée. Préparer un profil ne démarre pas Unreal.

Les actions UE5 de cuisson et de paquet proposent un aperçu explicite, une liste de cartes, une configuration, les options de conteneur et un délai maximal. Examinez les dossiers de sortie et de préparation avant l’exécution. Ces actions ne déploient, ne signent et ne téléversent pas le jeu. L’édition des assets Blueprint et une intégration complète de type RiderLink ne sont pas fournies.

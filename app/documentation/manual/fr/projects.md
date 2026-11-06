---
id: projects
title: Projets et outils
description: Projets et outils — manuel local de l’application
section: Guides
order: 45
sourceRevision: 2
---

## Projets et outils {#projects}

CyCode peut ouvrir les espaces de travail, solutions `.sln` et descripteurs Unreal pris en charge. CMake/MSBuild, Cargo, npm, JVM et shaders ont leurs propres workflows et prérequis : lisez le guide spécialisé avant de lancer une commande. La découverte d’un outil n’établit pas que tous les scénarios sont validés. Les cibles, arguments, sorties et annulations gardent leurs propriétaires natifs.

## Unreal {#unreal}

Vérifiez la version moteur, le projet, les cibles et les données d’analyse. Build, Live Coding et automatisation n’ont pas les mêmes prérequis. Les guides Unreal, UnrealLaunch et UnrealAutomation conservent leurs limites et résultats de validation.

## Exemple distribué {#example}

Consultez le [fichier CMake](doc:reference-app-examples-hello-cmakelists-txt) et le [point d’entrée C++](doc:source-api-sources-main-cpp) du projet minimal fourni. Sa compilation utilise votre compilateur ; la génération de l’aide ne l’exécute pas.

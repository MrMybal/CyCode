---
id: unreal
title: Projets Unreal Engine
description: Ouvrez les solutions, régénérez les données du projet et préparez les lancements Unreal.
section: Travailler dans CyCode
order: 140
parent: projects
sourceRevision: 3
---

## Choisir le moteur {#engine}

Ouvrez le `.uproject` ou son `.sln`. Vérifiez la racine du moteur, sa version, la cible, la plateforme et la configuration détectées dans la vue Unreal. La complétion nécessite les bons en-têtes du moteur et le contexte de compilation ; les mots-clés C++ ordinaires ne suffisent pas à analyser les macros Unreal.

## Données générées du projet {#generated}

Utilisez la génération des fichiers de projet lorsque des entrées de solution sont absentes ou périmées. Générez la base de compilation destinée à clangd lorsque nécessaire. Choisissez une cible de jeu ou d’éditeur existante avant de compiler avec UnrealBuildTool. La génération et la compilation exécutent les outils du moteur installé auquel vous faites confiance.

## Extraits d’insertion C++ {#code-snippets}

Le fournisseur Unreal intégré 1.6.0, lorsqu’il est actif, ajoute quatre définitions
à la palette commune : ue_log/uelog, ue_property, ue_function et ue_editor. UE_LOG
propose catégorie/message modifiables et choix de verbosité ; les déclarations
exposent les métadonnées Blueprint. ue_editor encadre le texte sélectionné avec
WITH_EDITOR, en gardant sa disposition littérale. Utilisez Rider Ctrl+J pour insérer
ou Ctrl+Alt+J pour encadrer une sélection.

Les entrées utilisent la portée cpp et affichent le propriétaire cycode.unreal.
Elles sont disponibles dans tout espace C++ quand le plugin est actif. Elles
n’ajoutent pas d’includes, ne fournissent pas d’implémentation, n’interrogent pas la
réflexion et ne vérifient pas la compilation Unreal. Choisissez une classe/un contexte
adapté et examinez le code. Désactiver le fournisseur révoque les propositions en
attente. Voir les [extraits de plugins](doc:refactoring#plugin-snippets).

## Lancer et créer un paquet {#launch}

Préparez un profil de lancement d’éditeur, de jeu ou de serveur non cuit, puis examinez son programme, sa carte, ses arguments et sa configuration dans le débogueur partagé. La compilation graphique avant débogage utilise la cible associée. Préparer un profil ne démarre pas Unreal.

Les actions UE5 de cuisson et de paquet proposent un aperçu explicite, une liste de cartes, une configuration, les options de conteneur et un délai maximal. Examinez les dossiers de sortie et de préparation avant l’exécution. Ces actions ne déploient, ne signent et ne téléversent pas le jeu. L’édition des assets Blueprint et une intégration complète de type RiderLink ne sont pas fournies.

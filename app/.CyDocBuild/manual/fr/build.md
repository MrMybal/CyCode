---
id: build
title: CMake et MSBuild
description: Configurez les outils et compilez une cible réellement sélectionnée.
section: Travailler dans CyCode
order: 130
parent: projects
sourceRevision: 4
---

## Configurer et compiler {#configure}

Pour CMake, choisissez les dossiers source et de compilation, la configuration, le générateur et le compilateur. Lancez la configuration, puis consultez les cibles rapportées par la File API de CMake. Utilisez un préréglage de configuration ou de compilation existant si nécessaire. Les préréglages de tests, de paquet et de workflow possèdent leurs propres actions d’aperçu et d’exécution.

Pour un `.sln` classique, examinez la solution dans MSBuild, sélectionnez la configuration et la plateforme, puis choisissez compiler, reconstruire, nettoyer ou restaurer. MSBuild et les SDK correspondants doivent être installés. Une solution Unreal conserve le parcours [UnrealBuildTool](doc:unreal#generated).

## Conserver le compilateur sélectionné {#toolchains}

La détection CMake conserve les noms de pilotes comme `clang++` et `g++`, y compris les liens symboliques. Les pilotes C et C++ peuvent partager un exécutable tout en choisissant des bibliothèques de liaison différentes. Si un ancien outil mémorisé est indisponible, actualisez la détection et sélectionnez à nouveau le compilateur installé. Si CyCode signale un changement de compilateur ou de générateur, choisissez un nouveau dossier de compilation et configurez-le ; les fichiers déjà construits restent présents. Le mode automatique et les préréglages du projet conservent leurs propres choix de compilateur.

## Lire la sortie {#output}

La vue Compilation suit la tâche native et conserve une portion limitée de sa sortie. Problèmes extrait les emplacements reconnus du compilateur. Annuler arrête l’opération courante appartenant à l’IDE ; les fichiers déjà créés restent présents. Un processus terminé avec un code de sortie non nul est un échec de compilation, même si sa sortie a été récupérée.

Les journaux conservent le texte UTF-8 et décodent les anciennes lignes de console Windows lorsque nécessaire. Les octets invalides peuvent être remplacés à l’affichage. Cela concerne aussi le résultat final : un chemin de projet accentué ne doit pas faire échouer une compilation terminée pendant l’affichage de son journal. Préférez des outils configurés en UTF-8 si une sortie mélange plusieurs encodages.

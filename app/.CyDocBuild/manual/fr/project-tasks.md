---
id: project-tasks
title: Exécuter les tâches des projets multilangages
description: Choisir les tâches Cargo, npm ou JVM et comprendre leur sortie et leur arrêt.
section: Travailler dans CyCode
parent: projects
order: 122
sourceRevision: 1
---

## Choisir le service de tâches {#tasks}

Ouvrez **Affichage > Tâches du projet**, puis sélectionnez le dossier réel, la
configuration, la cible et les arguments. La détection reconnaît Cargo, npm,
Java autonome, Maven et Gradle. Compiler/Exécuter utilise l’adaptateur choisi.
L’interface enregistre les documents modifiés avant une tâche ; annuler cet
enregistrement empêche son exécution. Examiner un projet n’installe pas ses
dépendances et ne l’exécute pas.

| Projet | Choix et effets |
| --- | --- |
| Cargo | Paquet/projet entier, bin/example/lib/test/bench, fonctionnalités, profils et options offline/locked ; les compilations exécutent les scripts et macros procédurales du projet |
| npm | Scripts build/test/nommés existants ; installation des dépendances séparée et explicite |
| Java | `javac` UTF-8, traitement d’annotations désactivé, nouveau dossier de classes ; sélectionner une entrée compilée public static main(String[]) |
| Maven / Gradle | Objectifs/tâches réels, module optionnel et classpaths résolus ; la logique de compilation du projet s’exécute |
| Shader | Compilation explicite DXC/glslang ; [réglages shader](doc:shaders#compile), sans prévisualisation GPU |

Cargo lance un véritable binaire ou exemple exécutable compilé, sans deviner son
nom ni utiliser un script de compilation. L’installation npm utilise `npm ci`
avec un fichier de verrouillage, sinon `npm install`, avec les scripts habituels
du projet. Les profils Java emploient la vraie classe principale choisie et son
classpath ; préparer un profil est distinct du démarrage de l’adaptateur de débogage.

## Lire la sortie en direct {#output}

Dans Compilation, choisissez **Sortie de compilation native** ou **Sortie des
tâches du projet**. Chacune garde sa propre fin de journal bornée. La sortie des
tâches suit Cargo/npm/JVM/shaders ; les terminaux persistants ont leur propre
cycle de vie. Les messages Cargo affichent le texte lisible du compilateur,
sans inventaire de métadonnées. Une ligne trop longue est signalée et omise ;
les anciens octets affichés peuvent être retirés sans arrêter l’outil.

Cliquez sur une source reconnue pour ouvrir un fichier existant du projet.
Les journaux en direct utilisent leur dossier de travail validé ; les chemins
non pris en charge restent du texte. Un journal observe la dernière exécution,
sans garantir que ses positions sont encore courantes.
Voir [les diagnostics](doc:diagnostics#output).

## Arrêter et recommencer {#cancel}

Annuler vise l’arbre de processus de compilation/test/script en cours. Attendez
la fin de l’état actif/en arrêt ; un accusé de réception ne prouve pas l’arrêt.
Les serveurs de développement appartiennent à un terminal persistant et s’arrêtent
depuis celui-ci. Les sorties déjà créées restent en place. Une compilation échouée
efface les métadonnées des exécutables réutilisables ; la suivante appelle
toujours l’outil réel. Délai et sortie sont bornés, sans file de fond illimitée.

Les [opérations du projet](doc:api-project#operations) séparent état/sortie et
exécution. Une réponse de sortie modifiée remplace l’ancienne fin de journal ;
un client SDK ne doit pas l’ajouter entière comme une différence incrémentale.

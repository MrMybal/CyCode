---
id: rust
title: Rust et Cargo
description: Choisissez les paquets Cargo, profils, fonctionnalités et cibles exécutables.
section: Travailler dans CyCode
order: 150
parent: languages
sourceRevision: 2
---

## Tâches Cargo {#cargo}

Ouvrez `Cargo.toml` ou son dossier de travail. Dans les tâches de projet, choisissez le paquet ou tout l’espace de travail, le type de cible, le profil et les fonctionnalités. La compilation utilise les métadonnées réelles et les messages de Cargo. Exécuter sélectionne un binaire ou un exemple exécutable ; Déboguer compile une cible avant de la transmettre au débogueur partagé.

Utilisez Cargo installé ou la chaîne privée configurée par l’application. Les scripts de compilation et macros procédurales s’exécutent lors de la compilation volontaire d’un projet de confiance. Rust-analyzer est un service de langage distinct ; ses réglages initiaux d’analyse ne lancent pas automatiquement ces règles.

Les tests Cargo utilisent les outils du projet. Pour le panneau CTest structuré de l’IDE, consultez les [tests](doc:testing#tests).

---
id: shortcuts
title: Profils de raccourcis
description: Choisissez Rider, Visual Studio ou VS Code et personnalisez les commandes.
section: Bien démarrer
order: 100
parent: settings
sourceRevision: 3
---

## Touches communes {#shortcuts}

| Raccourci | Action |
| --- | --- |
| Ctrl+S | Enregistrer le document texte actif |
| Ctrl+F | Chercher dans le document |
| Ctrl+Maj+F | Chercher du texte dans l’espace de travail |
| Ctrl+Alt+F | Chercher un nom de fichier |
| Double Maj | Rechercher partout |
| Ctrl+F1 / Maj+F1 | Manuel / aide contextuelle |
| Alt+F12 | Terminal |

## Choisir un profil {#profiles}

Les options de raccourcis proposent Rider / ReSharper, Visual Studio et Visual Studio Code. Les associations personnalisées sont prioritaires. Les séquences telles que Ctrl+K, Ctrl+C doivent se terminer en 1,5 seconde ; Échap annule la séquence.

| Action | Rider | Visual Studio | VS Code |
| --- | --- | --- | --- |
| Renommer un symbole | Maj+F6 | Ctrl+R, Ctrl+R | F2 |
| Formater le document | Ctrl+Alt+L | Ctrl+K, Ctrl+D | Maj+Alt+F |
| Actions contextuelles | Alt+Entrée | Ctrl+. | Ctrl+. |
| [Insérer un extrait du projet](doc:refactoring#snippet-catalogue) | Ctrl+J | Ctrl+K, Ctrl+X | À définir dans les préférences |
| [Encadrer avec un extrait](doc:refactoring#snippet-selection) | Ctrl+Alt+J | À définir dans les préférences | À définir dans les préférences |
| Structure du fichier | Ctrl+F12 | Ctrl+1, Ctrl+M | Ctrl+Maj+O |

Rider utilise Ctrl+F9 pour Compiler et Maj+F10 pour Exécuter. Ctrl+Maj+B rejoint la définition du type dans Rider ; ce n’est pas un second raccourci de compilation. F1 ouvre l’aide, sauf dans VS Code où il ouvre la palette d’actions. Ces profils utilisent des touches de style Windows/Linux ; aucun profil macOS dédié n’est fourni.

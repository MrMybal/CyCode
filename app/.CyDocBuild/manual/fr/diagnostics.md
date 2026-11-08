---
id: diagnostics
title: Lire les diagnostics et les positions du compilateur
description: Filtrer les problèmes et distinguer les positions actuelles des anciennes sorties.
section: Travailler dans CyCode
parent: navigation
order: 92
sourceRevision: 1
---

## Examiner Problèmes {#problems}

Ouvrez Problèmes, filtrez texte et sévérité, puis sélectionnez un diagnostic
pour lire son message complet, sa provenance et sa position. Copier le diagnostic
conserve aussi les messages impossibles à ouvrir. Les anciennes positions serveur,
ou sans révision, dans un fichier ouvert peuvent indiquer **Position non vérifiée**
et rester lisibles sans navigation. Les positions de fichiers fermés ou de dernière
compilation sont des observations, sans preuve de fraîcheur.

Suivant/Précédent suit la liste filtrée en boucle. Les variantes du fichier courant
suivent l’ordre ligne/colonne depuis la ligne active. Rider utilise F2/Maj+F2 dans
le fichier et Alt+F2/Alt+Maj+F2 entre les positions rapportées. Visual Studio/VS Code
utilisent F8/Maj+F8 dans le projet ; F2 reste Renommer pour VS Code.
Les raccourcis personnels restent prioritaires.

## Suivre la sortie du compilateur {#output}

Les lignes des journaux de Compilation peuvent ouvrir les sources reconnues sans
les modifier ni enregistrer. Les formes comprennent `file(line,column):`,
`file:line:` et Rust `--> file:line:column`. Chemins entre guillemets, UTF-8 et espaces
sont acceptés. Un identifiant GLSL numérique comme `ERROR: 0:12:` ne désigne pas un fichier.

```text
src/main.cpp(42,7): error C2065: missing identifier
src/main.cpp:42:7: error: missing identifier
```

Seuls les fichiers ordinaires existants du projet sont associés. Chemins externes,
absents et URI restent du texte ; aucun nom de base n’est deviné. Les journaux de
tâches en direct utilisent leur dossier de travail enregistré, les autres journaux
pris en charge la racine du projet. Les conventions de colonne varient ; colonnes
d’affichage Unicode exactes, source maps et échappements de terminal arbitraires
ne sont pas déduits. Recompilez ou actualisez le serveur après changement du code.
Voir [la sortie des tâches](doc:project-tasks#output) et
[build_location](doc:operation-build-location-build-location).

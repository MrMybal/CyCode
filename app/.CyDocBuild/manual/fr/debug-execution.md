---
id: debug-execution
title: Choisir comment exécuter un programme en pause
description: Distinguer pas, cibles choisies, exécution au curseur et déplacement du pointeur d’instruction.
section: Travailler dans CyCode
parent: debugging
order: 191
sourceRevision: 1
---

## Contrôles d’exécution {#stepping}

Mettez le programme en pause, choisissez fil/frame puis **Débogueur > Contrôles
d’exécution**. Le choix par défaut omet la granularité ; instruction source,
ligne source et instruction machine exigent la capacité annoncée. Uniquement le
fil choisi exige aussi sa capacité. Les raccourcis de pas utilisent ces choix.
Continuer utilise l’option de fil, sans granularité de pas.

Pas arrière et Continuer en arrière exigent un véritable historique du débogueur.
CyCode ne les simule pas par l’annulation du texte. Redémarrer la frame choisie
exige une frame inspectée admissible et la capacité du service ; cela peut répéter
entrées/sorties et autres effets. Le dernier état distingue soumission, accusé
de réception et résultat non confirmé. Une requête acceptée n’est pas une pause achevée.

## Choisir un appel {#step-targets}

Utilisez **Exécution > Choisir une cible de pas entrant**, puis lisez les cibles.
Sélectionnez une fonction réellement retournée et demandez explicitement d’y
entrer. Lire ou sélectionner ne relance jamais le programme ; aucun premier choix
n’est exécuté automatiquement. Actualisez après changement de frame, fil ou pause.
Les références expirent à l’exécution ou à l’actualisation et ne se réutilisent
pas après une tentative incertaine.

## Exécuter au curseur ou définir l’instruction suivante {#destinations}

Exécuter au curseur reprend temporairement vers une position admissible.
**Définir l’instruction suivante** demande plutôt les destinations de la véritable
source de la frame choisie. Entrez une ligne à partir de 1 et une colonne UTF-16
optionnelle (0 signifie non précisée dans le formulaire), lisez les destinations,
sélectionnez-en une puis appliquez explicitement le déplacement. Cela peut sauter
du code ou répéter des effets ; ce n’est ni une exécution au curseur ni une annulation.

```diagram
{"type":"flow","title":"Une action de débogage contrôlée","nodes":[{"id":"pause","label":"Pause et choix de frame","x":0,"y":0},{"id":"read","label":"Lire les choix du service","x":250,"y":0},{"id":"choose","label":"Examiner une cible","x":500,"y":0},{"id":"execute","label":"Exécuter explicitement","x":750,"y":0}],"edges":[{"from":"pause","to":"read"},{"from":"read","to":"choose"},{"from":"choose","to":"execute"}]}
```

Changer de frame/fil/session invalide les anciens choix. Le service reste
responsable de la correspondance source/exécutable ; modifier une source ne
recompile pas le programme en pause. Voir [l’inspection](doc:debug-inspection#inspect),
[les points d’arrêt](doc:debug-breakpoints#data) et [les opérations](doc:api-debug#operations).

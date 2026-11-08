---
id: navigation
title: Parcourir les symboles et problèmes
description: Utilisez Structure, Références, Problèmes et les occurrences.
section: Travailler dans CyCode
order: 90
parent: search
sourceRevision: 7
---

## Symboles et emplacements {#symbols}

Utilisez les actions de navigation vers la définition, la déclaration, la définition de type ou l’implémentation lorsque le serveur annonce cette capacité. Références affiche les emplacements retournés et propose des filtres, une actualisation et la copie des positions. Structure affiche les symboles du fichier courant ; le suivi du curseur et l’ordre d’affichage facilitent les grands fichiers.

La palette de symboles permet de choisir un serveur actif. Un résultat dont l’emplacement est déterminé à la sélection doit être résolu avant la navigation. Actualisez la requête lorsque le code ou le serveur change.

## Problèmes et occurrences {#problems}

Problèmes regroupe les messages du compilateur et des serveurs de langage. Filtrez par sévérité ou texte, sélectionnez un message pour ses détails, puis rejoignez un emplacement courant reconnu. Les positions indicatives ou périmées restent lisibles sans déplacer le curseur.

La mise en évidence des occurrences demande les plages de texte, lecture et écriture rapportées. Son inspecteur permet de passer à l’occurrence suivante ou précédente et de sélectionner les plages. Les occurrences textuelles peuvent être moins précises que les références sémantiques. Consultez le [profil de raccourcis](doc:shortcuts#shortcuts) pour les touches actives.

## Basculer entre en-tête et source C/C++ {#header-source}

Choisissez **Navigation > Basculer entre en-tête et source** avec un fichier C/C++
enregistré actif. Rider utilise Alt+O ; Visual Studio Ctrl+K, Ctrl+O. La palette
d’actions propose aussi la commande ; le profil VS Code n’a pas de nouveau
raccourci par défaut. Un résultat complet unique s’ouvre directement, conservant
les modifications source non enregistrées. Les résultats multiples ou partiels
utilisent la palette commune avec recherche, flèches/Entrée/clic et Échap.
Annuler une liste en attente empêche une réponse tardive d’ouvrir un document.

Le service local essaie les mêmes noms de base, les voisins et le plus proche
agencement Public/Private/Classes ou include/includes/src/source, en conservant les
sous-dossiers et en proposant un repli à la racine correspondante. Il effectue au
plus 128 lectures de métadonnées et seize ancêtres, sans serveur de langage,
ripgrep ni parcours récursif. Noms générés, liens symboliques de fichiers et cibles
externes sont exclus ; les limites et chemins illisibles sont signalés.

La navigation consulte aussi les noms de base de l’index de fichiers déjà à jour :
`src/Widget.cpp` peut trouver `include/library/Widget.hpp`, d’autres agencements et
les extensions indexées à casse mixte. La consultation utilise des positions en
cache, sans parcourir tous les fichiers à chaque commande, déclencher une
actualisation ou l’attendre. Les correspondances locales gardent leur libellé ;
les autres indiquent **Index du projet**. Les mêmes noms dans plusieurs modules
restent des choix distincts et ne prouvent pas une association sémantique.

Le plafond commun de 128 vérifications reste applicable. Un index indisponible,
occupé, périmé, partiel ou modifié est signalé et impose un choix explicite, même
pour un résultat unique. Actualisez les fichiers du projet puis réessayez si
nécessaire. Un cache complet inchangé permet l’ouverture directe d’un résultat ;
son périmètre respecte les règles d’exclusion et ne couvre pas les fichiers
ignorés. Aucun index sémantique, rapprochement de noms différents ou accès SDK
supplémentaire n’est ajouté.

L’opération SDK/ABI2 en lecture seule est [workspace_counterparts](doc:operation-workspace-counterparts-workspace-counterparts).



## Ouvrir un fichier inclus en C/C++ {#open-include}

Placez le curseur dans une directive `#include` littérale prise en charge puis choisissez **Navigation >
Ouvrir le fichier inclus**, la palette d’actions ou le menu contextuel de l’éditeur
actif. Vous pouvez définir un raccourci dans les préférences clavier ; aucun
raccourci par défaut n’est ajouté. Les modifications source non enregistrées sont
utilisées et conservées.

Avec des guillemets, `#include "../Public/Actor.h"` vérifie d’abord le chemin relatif
à la source. Guillemets et chevrons consultent aussi les chemins dont la fin
correspond aux composants littéraux dans l’index déjà à jour :
`<library/Widget.inc>` peut trouver `include/library/Widget.inc`. Les fichiers sans
extension et les en-têtes générés existants sont éligibles dans ce périmètre.
Les segments parents utilisent uniquement le chemin relatif entre guillemets.
Les correspondances multiples restent des choix, libellés **Relatif à la source**
ou **Index du projet**. Aucun rafraîchissement ni parcours récursif n’est lancé.
Un index indisponible, occupé, périmé, partiel ou modifié est signalé et empêche
l’ouverture automatique ; un résultat complet unique inchangé s’ouvre normalement.
Échap annule ; changer la source ou le curseur invalide le contexte en attente.

Le lecteur de contexte borné exclut les corps des commentaires, chaînes et chaînes
brutes courantes ; il reconnaît les sauts échappés et séparateurs numériques.
Les commentaires blocs sur une ligne peuvent séparer les jetons. Une continuation
immédiate antislash-LF/CRLF peut couper la directive ou le mot-clé. Le nom d’en-tête
reste sur une ligne physique. Le curseur peut être sur tout composant prolongé :

```cpp
# /* directive */ in\
clude /* header */ \
  "library/Widget.inc"
```

La plage UTF-16 interne rapportée reste sur la ligne physique réelle de l’en-tête,
même si le curseur demandé est sur la première ligne. Limites : préfixe de 1 Mio,
étendue physique totale de 8 Kio pour la directive logique, cible de 1024 octets.
Macros, include_next/import, commentaires multilignes non échappés entre les
jetons, espace entre antislash et saut, nom d’en-tête prolongé, chemins absolus/avec
antislash et écritures non prises en charge échouent explicitement. Les conditions
ne sont pas évaluées. Liens symboliques de fichiers et cibles externes sont exclus.
Les plafonds de 128 entrées par groupe et 128 vérifications au total restent
explicites. Les correspondances filtrées par les exclusions ne prouvent ni l’ordre
de recherche du compilateur ni une résolution sémantique ; les racines SDK
Unreal/compilateur externes restent hors de cette action locale.

L’opération commune en lecture seule est [workspace_include](doc:operation-workspace-include-workspace-include),
avec révision/identité source, curseur demandé, cible/type/plage littéraux et état
de l’index.


## Examiner les résultats et obtenir de l’aide {#related-results}

Choisissez **Navigation > Rechercher les fichiers C/C++ associés...**, la palette
d’actions ou le menu contextuel de l’éditeur actif pour examiner les fichiers
associés, même s’il n’y en a qu’un. Cette commande n’a pas de raccourci par défaut.
La liste commune sert aussi aux résultats multiples ou partiels des inclusions et
de la bascule en-tête/source. Filtrez les chemins en cache, utilisez les flèches,
Entrée ou un clic ; Échap ferme la liste.

- **Relancer cette recherche** relance la requête sans parcours de fichiers.
- **Actualiser l’index de recherche et relancer** reconstruit explicitement
  l’index normal puis relance la requête. Il faut un rapport valide et aucune
  actualisation normale du bureau en cours. L’index garde ses limites de
  30 secondes / 32 Mio / 200 000 chemins. Une recherche ordinaire ne le rafraîchit
  jamais. Fermer empêche l’ouverture tardive ; l’actualisation explicitement lancée
  reste suivie et n’est pas interrompue.
- **Copier tous les chemins des résultats** copie tous les candidats, indépendamment du filtre.
- **Copier le rapport de recherche (JSON)** copie métadonnées, provenance et
  couverture, y compris un rapport vide valide. Le texte source n’est pas copié.
- **Aide pour cette navigation** ouvre cette section locale, ou la section
  inclusion/en-tête-source pour les commandes directes. L’aide est aussi accessible
  pendant le chargement.

Le raccourci **Aide contextuelle** configuré sur une seule combinaison fonctionne
dans cette liste ; la valeur par défaut est **Maj+F1** dans les trois profils.
Les raccourcis en plusieurs combinaisons restent bloqués dans cette fenêtre ;
utilisez alors l’action d’aide visible. Les actions visibles de copie ou d’aide ferment la liste ; le raccourci d’aide
la garde ouverte. Le texte source est conservé. Les gardes d’identité/révision source, onglet actif et
moteur restent applicables ; les inclusions vérifient aussi le curseur.
Réessayer conserve le mode d’inspection.

La pagination des fichiers compare aussi `indexRevision` et `generation` ;
une actualisation manuelle avance la génération même si la révision du système de
fichiers est inchangée. Les clients API doivent rejeter les paires mélangées puis
recommencer. Voir [workspace_files](doc:operation-workspace-files-workspace-files).

Consultez aussi [Structure/hiérarchies](doc:symbols#structure) et [la navigation des diagnostics](doc:diagnostics#problems).

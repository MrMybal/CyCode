---
id: symbols
title: Explorer la structure et les hiérarchies
description: Utiliser les symboles, références et appels rapportés par le serveur avec leur contexte source.
section: Travailler dans CyCode
parent: navigation
order: 91
sourceRevision: 1
---

## Structure et palette de symboles {#structure}

Ouvrez **Affichage > Structure** pour les symboles du fichier actif. Filtrez,
triez par source ou nom, suivez le curseur ou localisez explicitement son symbole.
Le filtre garde le contexte des parents. Entrée ou un clic rejoint la position ;
déplacer seulement la sélection ne déclenche pas de saut. Noms, types et
imbrication viennent du serveur. Une réponse plate ne devient pas un arbre de
classes inventé.

Type, symbole et Recherche universelle peuvent utiliser le choix automatique ou
un serveur actif explicite. Un résultat dont la position est résolue à la
sélection exige une requête séparée. Attendez-la ; actualisez après changement
de source, projet ou serveur. Les identifiants des nœuds ne survivent pas au redémarrage.

## Références et occurrences {#references}

Définition, déclaration, définition de type et implémentation exigent la capacité
correspondante du serveur. Références propose filtrage en cache, sélection au
clavier, copie des positions et actualisation explicite au curseur. Une observation
périmée peut rester lisible même lorsque la navigation est indisponible.

La mise en évidence au curseur utilise les plages de texte/lecture/écriture
rapportées. Son panneau propose suivant/précédent et sélection des plages admises.
Les occurrences textuelles peuvent être moins précises que les références
sémantiques. Ces contours ne remplacent pas les couleurs de syntaxe. Une sélection
refuse les plages partielles, superposées ou périmées avant de changer les curseurs.
La recherche lexicale reste une autre action.

## Hiérarchies d’appels et de types {#hierarchy}

Placez le curseur sur une fonction/un type puis choisissez **Navigation >
Hiérarchie d’appels** ou **Hiérarchie de types**. Affichage > Hiérarchie rouvre le
panneau. Choisissez appelants/appelés ou types de base/dérivés. Déplier demande une
branche au serveur d’origine ; récursions et limites restent visibles.

Entrée, double clic ou Ouvrir le symbole rejoint la déclaration. Sélectionner un
site d’appel rejoint cet appel : les sites entrants appartiennent à l’appelant,
les sortants à la fonction source dépliée. Actualiser au curseur crée une nouvelle
observation après une modification ou un changement de service. Aucun nom similaire
ne suffit à inventer une hiérarchie. Voir [les profils de touches](doc:shortcuts#profiles)
et [les opérations de langage](doc:api-language#operations).

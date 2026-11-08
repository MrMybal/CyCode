---
id: debug-breakpoints
title: Points d’arrêt de données et d’instructions
description: Examiner les listes de remplacement complètes et leur vérification avant de reprendre.
section: Travailler dans CyCode
parent: debugging
order: 192
sourceRevision: 1
---

## Surveiller un accès aux données {#data}

Dans **Débogueur > Points d’arrêt de données**, mettez en pause et examinez une
variable/expression de la frame choisie ou un enfant d’une portée affichée.
Lisez la description réelle et les modes d’accès proposés. Le mode par défaut
omet le choix au lieu de deviner Écriture. Conditions et nombre de passages
exigent leurs propres capacités.

Ajoutez toutes les cibles voulues à la **Liste de remplacement**, puis remplacez
les points d’arrêt de données. Cela remplace l’ensemble, pas seulement la nouvelle
ligne. Nouvelle liste abandonne le brouillon et observe la configuration actuelle ;
Effacer soumet une liste vide. Lisez les états vérifiés/erreurs et limites matérielles.
Les références liées à une session/pause expirent ; inspectez de nouveau sans inventer d’ID.

## S’arrêter sur une instruction {#instructions}

Lisez le Désassemblage en pause. Le clic droit sur une adresse admissible permet
d’ajouter un point d’arrêt ou de copier l’adresse. L’onglet accepte aussi la vraie
référence du service et un décalage en octets. Utiliser l’instruction de la frame
choisie copie sa référence inspectée.

Nouvelle liste de remplacement repart de la configuration complète connue.
Examinez références, décalages, activation, conditions et compteurs, puis appliquez
les points d’arrêt. Saisir ou fermer ne soumet rien. Effacer envoie explicitement
un ensemble vide. Reprise, invalidation, changement de configuration ou application
réussie abandonnent le brouillon. Ces contrôles modifient le programme débogué et
ne font pas partie de l’annulation du texte. Lisez [l’inspection](doc:debug-inspection#changes)
et [les contrats publics](doc:api-debug#operations) avant toute automatisation.

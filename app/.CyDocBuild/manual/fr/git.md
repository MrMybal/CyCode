---
id: git
title: Examiner les changements Git
description: Sélectionnez un dépôt, examinez les différences, préparez et validez les changements.
section: Travailler dans CyCode
order: 230
parent: version-control
sourceRevision: 2
---

## Examiner et valider {#changes}

1. Sélectionnez le dépôt voulu dans Contrôle de versions, surtout si l’espace en contient plusieurs.
2. Examinez les différences du fichier de travail ou de sa version préparée dans la vue de comparaison partagée.
3. Préparez un fichier entier, ou sélectionnez les blocs de texte admissibles pour une préparation partielle.
4. Examinez l’index, saisissez le message et créez volontairement le commit.

Le commit respecte les hooks et réglages de signature Git. Le texte non enregistré de l’éditeur et l’état du disque ou de l’index sont différents. Enregistrez avant une opération exigeant des fichiers réconciliés.

## Branches et conflits {#branches}

Les actions de branche, de dépôt distant, de récupération, d’envoi, de réserve, de fusion et de cherry-pick ou revert sélectionné utilisent des formulaires de revue. Pour un conflit, examinez les versions disponibles et la résolution d’index proposée avant de l’appliquer. Une résolution d’index ne remplace pas forcément les fichiers de travail et n’enlève pas nécessairement leurs marqueurs de conflit. L’application séparée aux fichiers possède son aperçu et sa sauvegarde.

Les actions de worktree et de sous-module nécessitent également le bon dépôt et un aperçu courant. Les opérations d’historique Git ne deviennent pas des transactions Undo de l’éditeur ; examinez tout résultat incertain ou en échec avant de recommencer.

---
id: git-conflicts
title: Résoudre les conflits et rejouer des commits
description: Examinez les résolutions de fusion et les plans de rebase ou cherry-pick.
section: Travailler dans CyCode
parent: git
order: 315
sourceRevision: 1
---

## Résoudre un conflit {#resolve}

1. Enregistrez les documents concernés et actualisez le dépôt Git sélectionné.
2. Choisissez un fichier en conflit et examinez les versions de base, locale et distante ainsi que les blocs de conflit.
3. Préparez la résolution de texte souhaitée. Examinez le contenu résultant et l’état de l’index concerné.
4. Appliquez tant que l’aperçu est courant. Actualisez l’état et examinez les conflits restants avant de terminer la fusion.

Les conflits binaires, d’encodage ou de structure complexe non pris en charge restent signalés ; utilisez un outil Git externe dans ces cas. Un état de source ou d’index périmé exige un nouvel aperçu. Ne supposez pas que l’annulation de l’éditeur restaure des opérations de dépôt.

## Fusion, rebase et cherry-pick {#replay}

Consultez d’abord l’état de l’opération en cours. Les actions de rejeu exposent un plan à examiner avant exécution ; utilisez l’action de continuation, de saut ou d’abandon proposée pour l’opération et son état réels. Les branches, changements d’index et hooks suivent le comportement normal de Git.

Pour une fusion, examinez le plan de finalisation avant de créer le commit de fusion ou de quitter. Ces actions peuvent modifier le disque et l’index suivis et créer des commits. Si l’outil signale un résultat incomplet, examinez l’état du dépôt avant de réessayer. Les [contrats d’opération](doc:api-vcs#operations) exposent les appels distincts d’état, de plan et d’application pour l’automatisation.

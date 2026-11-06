---
id: other-providers
title: SVN, Mercurial et Diversion
description: Choisissez le fournisseur adapté à votre dépôt existant.
section: Travailler dans CyCode
parent: providers
order: 313
sourceRevision: 1
---

## Subversion {#svn}

Installez SVN, préparez une copie de travail et activez `cycode.svn`. Associez son dossier dans Contrôle de versions ; `CYCODE_SVN` permet de choisir un client par chemin absolu. L’état, les différences, l’historique et les annotations utilisent les vues communes. Examinez les actions explicites d’ajout, de restauration, de commit et de mise à jour avant exécution. Choisir le fournisseur ne récupère pas de dépôt. L’authentification SVN et la configuration du serveur restent dans le client externe.

## Mercurial {#hg}

Activez `cycode.mercurial`, installez `hg` et associez le dépôt local exact contenant `.hg`. Utilisez `CYCODE_HG` pour imposer un exécutable par chemin absolu. Add suit un fichier ; l’arrêt du suivi conserve le fichier de travail. Commit inclut les changements suivis et n’ajoute pas automatiquement les fichiers inconnus. Revert exige `REVERT` et ne crée pas de sauvegarde `.orig`.

Update exige une copie de travail suivie propre et une seule révision résolue. Pull utilise un alias configuré sans actualiser les fichiers ; Push utilise un alias configuré sans forçage ni autorisation de nouvelle branche. L’alias par défaut est `default`. Les URL arbitraires et expressions revset ne sont pas acceptées dans ces formulaires.

## Diversion {#diversion}

Installez et authentifiez `dv`, puis créez ou clonez le dépôt avec ce client. Activez `cycode.diversion` et associez le dossier exact contenant `.diversion`. Un chemin absolu dans `CYCODE_DV` choisit le client. L’agent Diversion synchronise les changements ; il n’existe ici ni index Git ni commandes Push/Pull séparées.

Vérifiez la disponibilité de la synchronisation avant une écriture. Commit exige un message ; l’action courante valide tous les changements et l’agent publie le résultat. Le changement de branche et Update exigent une copie de travail propre et entièrement synchronisée. Revert concerne un fichier après saisie de `REVERT`. La disponibilité des verrous dépend du service et de l’offre installés. Une synchronisation incomplète bloque les modifications.

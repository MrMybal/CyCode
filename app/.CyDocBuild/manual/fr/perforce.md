---
id: perforce
title: Utiliser Perforce (P4)
description: Associez un client P4 existant et examinez les actions sur fichiers.
section: Travailler dans CyCode
parent: providers
order: 312
sourceRevision: 1
---

## Configurer un client {#setup}

Installez la CLI P4 et configurez le serveur, le client, la vue, le ticket et la confiance avec les outils P4 habituels. Un chemin absolu dans `CYCODE_P4` permet de choisir l’exécutable. Activez `cycode.perforce` dans Extensions. Dans Contrôle de versions, choisissez Perforce (P4) et associez le dossier du projet à l’intérieur de la racine du client configuré. Ouvrir ou associer un projet ne crée pas de client, ne connecte pas de compte et ne synchronise rien.

## Travail quotidien {#files}

L’état affiche les fichiers du projet ouverts dans P4, avec l’action, la changelist, le type et les conflits non résolus. Ce n’est pas une recherche automatique des modifications hors ligne non ouvertes. Sélectionnez un fichier pour les différences, l’historique ou l’origine des lignes. Checkout utilise `edit` ; Add et Reopen acceptent la changelist par défaut ou un numéro positif.

Commit soumet un seul fichier résolu en edit/add/delete de la changelist par défaut, avec une description explicite. Utilisez un client P4 externe pour les changelists numérotées complètes, les shelves, les streams, les déplacements et les intégrations. Revert exige `REVERT` et peut abandonner le contenu du disque ; l’annulation du document ne le restaure pas.

Sync se limite au projet et refuse les fichiers ouverts ou les changements détectés par la prévisualisation de réconciliation en lecture seule. Configurez P4IGNORE pour les fichiers générés appropriés et les métadonnées `.cycode`. Le verrouillage agit sur un fichier choisi sans forçage administrateur. Les associations de client ambiguës ou non prises en charge sont signalées.

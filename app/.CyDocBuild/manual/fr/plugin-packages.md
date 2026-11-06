---
id: plugin-packages
title: Installer et mettre à jour une extension
description: Examinez un paquet signé et gérez la confiance des éditeurs.
section: Travailler dans CyCode
parent: plugins
order: 311
sourceRevision: 1
---

## Examiner et installer {#install}

1. Ouvrez Extensions > Paquets de plugins et choisissez un dossier de paquet local.
2. Lancez l’inspection. Lisez la version, la plateforme, la compatibilité, les autorisations et l’empreinte de l’éditeur. L’inspection ne charge pas la bibliothèque native.
3. Vérifiez l’empreinte par un moyen indépendant. Si nécessaire, accordez explicitement votre confiance à l’éditeur, puis relancez l’inspection.
4. Choisissez séparément l’action d’installation ou de mise à jour du paquet.
5. Démarrez un nouveau processus de l’application pour utiliser cette sélection. Les hôtes existants conservent leur version de bibliothèque.

Le gestionnaire accepte des paquets locaux signés par des éditeurs de confiance ; il ne télécharge pas de catalogue de marché d’extensions. Une signature identifie l’éditeur et le contenu, sans créer de bac à sable système. L’activation par projet dans Extensions reste distincte de la sélection des paquets pour l’installation.

## Revenir à une version et supprimer {#versions}

Une mise à jour doit augmenter la version numérique et conserver la clé de l’éditeur. Pour changer d’éditeur, retirez le paquet et examinez le remplaçant depuis le début. La version sélectionnée et quatre versions précédentes sont conservées. Le choix d’une version précédente vérifie sa compatibilité et son contenu avant la bascule.

La suppression contrôle les dépendances requises. Le nettoyage concerne seulement les versions vérifiées inutilisées, hors sélection et historique ; une version retenue par un autre hôte reste utilisée. Révoquer un éditeur agit sur les découvertes futures et ne décharge pas le code déjà en cours. Les identifiants des extensions fournies ne peuvent pas être remplacés par des paquets gérés.

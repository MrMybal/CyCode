---
id: plugins
title: Gérer les extensions
description: Activez les plugins de confiance et gérez leurs options et paquets.
section: Travailler dans CyCode
order: 250
sourceRevision: 3
---

## Activer une extension {#extensions}

Ouvrez le gestionnaire d’extensions pour consulter les plugins, commandes, dépendances et options installés. Activez ou désactivez un plugin pour l’espace de travail, puis examinez les menus et panneaux ajoutés. Une bibliothèque native désactivée peut rester chargée jusqu’à l’arrêt de l’application.

Les extensions natives exécutent du code de confiance dans l’application ; ce ne sont pas des composants isolés. Les commandes de projet ou de fournisseur peuvent aussi lancer des outils externes selon leurs permissions déclarées. Examinez l’origine et les capacités avant l’activation.

Le gestionnaire de paquets propose des actions explicites de consultation, installation, suppression, retour arrière et nettoyage, avec revue des chemins et décisions de signature ou de confiance. Ce n’est pas une place de marché publique générale et il ne rend pas sûr un code natif arbitraire.

## Catalogues d’extraits {#snippet-catalogues}

Les plugins ABI2 actifs peuvent fournir des catalogues d’extraits statiques. Ouvrez
Insérer un extrait ou Encadrer avec un extrait pour voir les portées correspondantes
et l’ID du plugin propriétaire. La demande n’appelle pas de fournisseur et aucune
insertion n’est automatique. Désactiver un plugin ou une dépendance requise retire
son catalogue et invalide les propositions en attente. Le modèle de confiance des
bibliothèques natives reste inchangé. Voir [utiliser les extraits](doc:refactoring#plugin-snippets)
et [le contrat public](doc:plugin-api#snippet-catalogues).

## Écrire une extension {#development}

Utilisez l’ABI C publique et les descripteurs de contribution JSON pour les commandes, menus, panneaux, options, syntaxes, fournisseurs de projet ou de versions et services de langage ou de débogage pris en charge. Consultez le [développement de plugins](doc:plugin-api#contract) pour la propriété et les points d’entrée.

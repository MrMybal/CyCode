---
id: providers
title: Fournisseurs externes de contrôle de versions
description: Configurez Perforce, SVN, Mercurial ou Diversion par plugin.
section: Travailler dans CyCode
order: 240
parent: version-control
sourceRevision: 2
---

## Configurer un fournisseur {#external}

Installez ou activez le plugin de confiance, installez son client habituel et configurez son espace de travail et son authentification hors de l’IDE lorsque nécessaire. Associez le dossier au fournisseur, actualisez son contexte, puis utilisez les actions annoncées dans ses menus.

Perforce utilise le client P4 avec sa configuration d’espace et de serveur. SVN, Mercurial et Diversion utilisent leurs clients installés respectifs. Les parcours de changelist, de shelf, de stream et de résolution dépendent des actions du plugin ; sélectionner un fournisseur ne promet pas toutes les fonctions de son client autonome.

Les fournisseurs peuvent contribuer des menus et formulaires différents. Un système personnalisé peut employer le même [contrat de plugin](doc:plugin-api#contract) versionné sans remplacer le service Git intégré.

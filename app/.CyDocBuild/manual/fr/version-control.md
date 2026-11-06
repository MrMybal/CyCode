---
id: version-control
title: Choisir le contrôle de versions
description: Utilisez Git par défaut ou associez un dossier à un autre fournisseur.
section: Travailler dans CyCode
order: 220
sourceRevision: 2
---

## Associations des projets {#providers}

Git est intégré. Des plugins peuvent fournir Perforce, SVN, Mercurial, Diversion ou un autre système compatible. Associez un dossier de projet à son fournisseur ; l’association de dossier la plus précise détermine les menus et actions applicables.

Le fournisseur installé doit annoncer la capacité demandée. Ses menus et formulaires varient ; choisir un fournisseur n’installe pas son client ou son serveur externe. Un espace de travail peut contenir plusieurs racines associées différemment.

Pour Git intégré, utilisez le [parcours Git](doc:git#changes). Pour les outils externes, consultez la [configuration des fournisseurs](doc:providers#external). Les opérations SDK typées `vcs_*` propres à Git restent spécifiques à Git ; les fournisseurs externes passent par `vcs_provider_action`.

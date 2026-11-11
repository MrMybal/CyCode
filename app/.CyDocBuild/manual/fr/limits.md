---
id: limits
title: Capacités et limites
description: Comprenez les dépendances externes et les limites de l’IDE.
section: Bien démarrer
order: 300
parent: overview
sourceRevision: 3
---

## Périmètre {#limits}

CyCode fournit l’édition native, les outils de projet, les connexions de services de langage, les parcours Git et fournisseurs, le débogage et les extensions. Il ne promet pas toutes les fonctions de Rider, Visual Studio ou VS Code. Les serveurs et adaptateurs déterminent les actions sémantiques et de débogage disponibles.

Le profil ordinaire des tampons d’édition/LSP traite des textes jusqu’à 4 Mio. Le profil UTF-8 paginé natif et la vue virtuelle partagée de CyUI sont des composants distincts ; leur présence ne prouve pas une intégration graphique complète ni un overlay LSP vivant des modifications paginées. L’enregistrement d’un document paginé publie son contenu natif entier, jamais le seul fragment visible. La compilation de shaders n’a pas d’aperçu GPU. Les tâches web n’intègrent pas de débogueur de navigateur. L’édition d’assets Blueprint, l’intégration complète d’Unreal Editor et l’isolation sans restriction des plugins natifs ne sont pas fournies. L’explorateur structuré de tests utilise CTest plutôt que tous les frameworks.

La disponibilité du service se distingue de l’initialisation des serveurs, des diagnostics du fichier courant et de la couverture sémantique du projet entier. Les checkpoints restaurés contiennent des observations historiques, inconnues pour la passe courante jusqu’à une nouvelle analyse. Le même bridge natif fournit l’état intégré et celui de son endpoint ; cette identité ne prouve pas un index complet. Voir [la progression de l’analyse](doc:analysis-progress#persistence) et [l’intégration native](doc:native-core#ownership).

## Langues {#translations}

Ce manuel est disponible en anglais et en français. Les identifiants techniques d’API, les mots-clés des langages et les noms de fichiers conservent leur orthographe d’origine. Changez la langue en haut pour garder la page courante.

## Lecteur {#reader}

Le manuel complet s’ouvre localement sans serveur ni connexion. Ctrl+K permet d’y chercher. Les médias s’agrandissent dans le lecteur et Échap les réduit. Le stockage du navigateur détermine la conservation des favoris, notes et positions. Les liens externes de licences ou de dépôts nécessitent une connexion.

---
id: native-core
title: Séparation du Core natif et de l’IDE
description: Réutiliser le moteur natif indépendamment de l’interface graphique.
section: Pour commencer
parent: overview
order: 3
sourceRevision: 1
---

## Propriété des services {#ownership}

CyCode possède désormais le moteur C++20 et son CLI/service. CyCodeIDE possède
l’interface SDL/OpenGL/CyUI. CyCodeReader et l’éditeur graphique partagé consomment
les implémentations natives Paths, TextFile et Snippet de CyCode (API source 1).
La session de documents et de recherche de Reader n’a pas encore été transférée
vers Engine. Les guides IDE existants restent utiles pour l’interface extraite ;
les téléchargements 0.2.3 décrivent l’application avant cette séparation des dépôts.

## Compiler sans interface {#build}

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

Le Core ne demande aucun checkout Reader, SDL, ImGui, CyUI, OpenGL ou Tree-sitter.
Un hôte natif ajoute les sources et lie `CyCode::Core`. Un petit éditeur peut
importer seulement `common/` et lier `CyCode::Files` / `CyCode::Snippets`. Chaque
Engine possède son projet. Un build ordinaire ne lance ni application ni tests natifs.

## Service persistant {#contract}

`cycode_cli --disable-plugins <projet> --stdio` émet une ligne JSON de disponibilité,
puis accepte des requêtes avec `cycode-core/1`, un id, une méthode et des paramètres
objets. `service.status` expose le cycle de vie et l’identité du contexte.
`service.shutdown` ou EOF ferme proprement l’hôte via la destruction existante
d’Engine, sans enregistrer les documents. Les requêtes réutilisent le même Engine
et ses moteurs de langage. Les erreurs ont un code et un message ; les échecs
historiques d’Engine utilisent prudemment OperationFailed.

Le mode authentifié `--serve <descripteur>` reste compatible avec
`cycode-native/1`. Son parent contrôle l’arrêt par stdin/EOF. Les commandes stdio
ne sont pas de nouvelles opérations SDK/MCP. `language.status` conserve les états,
capacités, époques et diagnostics liés aux révisions des moteurs. Un moteur
initialisé ne prouve pas que l’index sémantique couvre tout le projet.

## Vérification et mesures {#validation}

La compilation de production Windows couvre Core/CLI, l’IDE extrait et le CLI
Reader. Les exécutions natives locales restent suspendues pendant le diagnostic
des suppressions antivirus. Les contrôles de données et de syntaxe du compilateur
ne prouvent pas le fonctionnement après extraction. Linux/macOS nécessitent de
nouveaux builds et des vérifications d’exécution.

Le workflow explicite `app/scripts/benchmark_semantic.py --ci`, réservé aux runners,
mesure les vraies requêtes natives/clangd sur un corpus synthétique déterministe :
transport JSON, démarrage, diagnostics liés aux versions, navigation, références,
complétion, préparation de renommage, éditions non enregistrées et mémoire délimitée.
`--prepare-only` crée les données sources sans exécution native. Aucun temps n’a
encore été relevé pour cette extraction. Un résultat synthétique ne démontrera
ni les performances sur de grands projets ni une équivalence avec un autre IDE.

## Étapes restantes {#limits}

Les références sur de vrais projets CMake/MSBuild/Unreal, l’invalidation du contexte,
la synchronisation incrémentale, les transformations avancées et la réutilisation
plus large d’Engine dans Reader restent à traiter. L’identité de l’installateur et
des mises à jour IDE doit être migrée avant publication. Ce build de développement
désactive l’ancien updater pour éviter de proposer l’installateur de l’application
précédente. Les transactions, révisions, contrôles du disque et sauvegardes explicites
existants restent les références.

---
id: native-core
title: Core natif et intégration des applications
description: Documents natifs communs, analyse des projets et services persistants sans interface.
section: Pour commencer
parent: overview
order: 3
sourceRevision: 5
---

## Propriété des services {#ownership}

CyCode fournit le moteur C++20 et son CLI/service. CyCodeIDE possède l’interface
SDL/OpenGL/CyUI. CyCodeReader utilise la session de texte autonome du Core :
documents, encodages, recherche, plugins de texte et transport natif. Son éditeur
graphique reste un adaptateur de présentation partagé. Les deux profils natifs
utilisent le même BufferState et les mêmes primitives de fichiers protégées ;
leurs règles de chemins restent explicites. Le profil texte de Reader ne lance
pas les serveurs de langage et n’ajoute pas implicitement l’analyse des projets.

Les guides et médias IDE restent utiles pour l’application extraite. Les paquets
0.2.3 publiés précèdent cette séparation ; les sources de développement ne
constituent pas un nouvel installateur ni une nouvelle version. Les ABI C des
plugins restent compatibles dans leur profil d’origine ; les binaires des
plugins de projet et de texte ne sont pas interchangeables.

## Compiler sans interface {#build}

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

La configuration et les dépendances du Core ne demandent ni Reader, ni SDL,
ImGui, CyUI, OpenGL ou Tree-sitter. CyCode::Core fournit le moteur de projet.
Un hôte léger sélectionne Documents, TextSession, TextBridge, Process, Transport,
Protocol, Files et Snippets via CYCODE_COMPONENTS ou cmake/CyCodeComponents.cmake.
Les dépendances sont résolues automatiquement ; seul JSON épinglé est nécessaire
au profil texte autonome. L’API source 1 décrit l’intégration des sources,
sans promettre une ABI binaire C++ stable.

```sh
python app/scripts/fetch_dependencies.py --components TextSession TextBridge Process
cmake -S . -B runtime/text -DCYCODE_COMPONENTS="TextSession;TextBridge;Process" -DBUILD_TESTING=OFF
cmake --build runtime/text --config Release
```

Chaque Engine possède un projet. Conservez-le entre les requêtes et attendez
les appelants avant sa destruction. Un build ordinaire ne lance ni application
ni tests natifs.

## Service persistant {#contract}

`cycode_cli --disable-plugins <projet> --stdio` reçoit des objets cycode-core/1
avec un id, une méthode et des paramètres objets. La réponse conserve l’id et
retourne result ou une erreur structurée avec code/message et des détails
facultatifs. Les conflits de révision/disque, modifications en lecture seule et
échecs de moteurs migrés conservent le code de leur propriétaire. Les échecs
historiques restent OperationFailed ; aucun code n’est déduit du texte d’erreur.
Les requêtes sont bornées à 8 Mio, les réponses à 20 Mio et les ids texte à 256 octets.

service.shutdown ou EOF ferme proprement l’hôte stdio sans enregistrer les
documents. Les descripteurs locaux cycode-native/1 et cycodereader-native/1
restent compatibles et annoncent coreProtocol. Le parent contrôle l’arrêt du
socket par stdin/EOF. Le transport attend ses appelants, borne les connexions
et messages et préserve le descripteur d’une instance plus récente. CyToolsCore
possède toujours le MCP, les jobs, identités, permissions et l’ordonnanceur ;
ces contrôles d’hôte ne sont pas de nouvelles opérations SDK/MCP.

service.status décrit le cycle de vie et le contexte. language.status.servers
décrit chaque moteur : configuration, lancement observé, initialisation,
capacités, époque, révisions envoyées et diagnostics liés aux versions. Un
moteur initialisé ou une notification de texte envoyée ne prouve pas une analyse
complète. La couverture de l’index sémantique global reste inconnue ; les caches
de chemins et la recherche littérale sont distincts de l’index de clangd.

## Vérification et mesures {#validation}

Le Core 84e4c78 a passé la suite native de 63 cas sur Windows x64, Ubuntu x64 et
macOS arm64 (MSBuild est ignoré sur POSIX). Le cas Core POSIX inclut 80 contrôles
concurrents de propriété des processus enfants. Le contrôle corrigé du groupe a
aussi passé le replay Git macOS qui échouait, puis trois répétitions supplémentaires.
Reader 671c588 a passé 17 cas de services/vues sur ces systèmes : plugins natifs,
encodages, éditions actives, IPC protégé, extraits, comparaison et historique Git.
Ces vues sans fenêtre OS ne valident pas le GPU, l’IME ni le bureau physique.

Le benchmark sémantique réservé aux runners a terminé un corpus synthétique
déterministe et le véritable projet CMake du Core : 121 unités de compilation,
230 fichiers sources et en-têtes. Sur un runner Linux, les médianes à chaud ont
été de 8,88 ms pour la définition, 8,79 ms pour les références, 303,82 ms pour la
complétion et 18,55 ms pour l’aperçu de renommage. Les premiers diagnostics liés
à une version ont pris 2,91 secondes ; la somme RSS instantanée CLI/moteur était
de 1 234 755 584 octets. Cinq échantillons constituent une référence, pas une
promesse universelle. La RSS peut compter deux fois des pages partagées et ne
mesure ni le pic ni la mémoire privée. Les réponses ne prouvent pas la couverture
complète des références. Aucune comparaison CLion/IntelliJ n’a été réalisée.
Le guide d’architecture conserve les rapports bruts et les liens CI exacts.

Une seconde exécution après consolidation, à la révision d0b1791, a aussi réussi
sur le projet réel : 121 unités de compilation, 227 fichiers, médianes à chaud
de 9,31 ms pour la définition, 8,74 ms pour les références, 269,26 ms pour la
complétion et 18,52 ms pour l’aperçu de renommage. Les premiers diagnostics
actuels ont pris 2,26 s ; la médiane modification non enregistrée / diagnostics /
définition était de 550,76 ms. Le corpus ayant changé, ces observations ne
constituent pas une comparaison d’optimisation. Le guide d’architecture conserve
les rapports bruts et les révisions exactes des workflows.

`app/scripts/benchmark_semantic.py --ci` s’exécute uniquement sur un runner
explicitement autorisé. `--prepare-only` crée des données sans exécution native.
Les lancements natifs locaux restent suspendus pendant le diagnostic antivirus ;
les builds et contrôles de données/syntaxe sont distingués des essais d’exécution.

Le chemin de réponse natif réveille maintenant son propriétaire à la réception
d’un message du moteur. Les callbacks sémantiques restent sur ce thread ; le
transport est rejoint avant destruction et le prédicat de réveil est protégé.
Sur le corpus synthétique identique, deux processus de runner ont observé une
médiane de définition vers 5,08 ms au lieu de 8,3 ms, et un aperçu de renommage
vers 10–11 ms au lieu de 19 ms. Le projet réel du Core a aussi terminé, mais
la complétion et le délai modification/analyse étaient plus lents sur cette
exécution. Cela ne prouve pas une accélération générale ni une comparaison
contrôlée sur une même machine. Le guide conserve les rapports bruts et leurs
révisions exactes.

## Étapes restantes {#limits}

Les mesures sur de grands projets Unreal/MSBuild, la couverture complète de
l’invalidation du contexte et des dépendances, la synchronisation incrémentale,
les mesures concurrentes/annulables et les transformations avancées par commandes
restent à poursuivre. Les éditions sémantiques existantes conservent les aperçus,
contrôles de révision/disque, sauvegardes explicites et annulations. Les actions
non prises en charge restent indisponibles. La présentation IDE et l’identité
de son installateur/updater exigent leur propre validation avant publication ;
l’ancien updater reste désactivé dans cette version de développement.

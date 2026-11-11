---
id: native-core
title: Core natif et intégration des applications
description: Documents natifs communs, analyse des projets et services persistants sans interface.
section: Pour commencer
parent: overview
order: 3
sourceRevision: 18
---

## État actuel de la livraison {#current-status}

La distribution autonome 0.1.2 comprend 252 opérations publiques, historique
transactionnel/reçus de commit, récupération paginée et composant PagedText.
Compilation native de production et intégrité des paquets sont contrôlées sans
lancer Core ni l’installateur. Les anciennes observations compilateur/exécution
conservent leur provenance ; les nouveaux essais réels d’historique/installation
restent à faire. Une installation ou un processus 0.1.1 reste ancien avant mise
à niveau et reconnexion. Les suspensions antivirus du récit daté concernent les
anciens essais. GitHub Actions reste suspendu ; binaires et Setup sont créés localement.


L’historique transactionnel natif commun et les reçus atomiques sont accessibles
aux intégrations/clients SDK. Voir [propriétaire Undo/Redo et gardes](doc:large-projects#history).
Les bibliothèques de production compilent sous Windows ; les nouvelles régressions
d’historique à l’exécution et les interactions graphiques restent à vérifier.

## Composant de stockage paginé facultatif {#paged-text}

`CyCode::PagedText` se compile indépendamment avec Files, JSON et le digest de
Monocypher. Il fournit morceaux immuables, lignes/colonnes UTF-16 parcimonieuses
et publication progressive sans Engine, serveurs de langage ou dépendances
graphiques. Incluez `cycode/paged/LargeText.h`. Le moteur de projet utilise cette
même implémentation ; les adaptateurs Reader/IDE peuvent la sélectionner.
Cette liaison ne fournit pas à elle seule un éditeur graphique virtuel.
Consultez [le guide d’intégration](reference-app-docs-coreextraction).

Le fournisseur graphique commun est désormais disponible dans CyUI révision
`0ba1ba663b4b2bf46d066a32829bc91ea2743cba` : `CyUI::Code` fournit le contrat
lignes/pages et `CyUI::CodeEditor` fournit `PagedCodeEditor`. Reader/IDE doivent
encore raccorder leurs adaptateurs, révisions et historiques Undo/enregistrements.
Le Core natif ne reçoit aucune dépendance graphique ni overlay LSP pour les
modifications paginées.

## Propriété des services {#ownership}

Les vues intégrées lisent le service déjà possédé par leur bridge avec
l’accesseur const `cycode::Bridge::serviceStatus()` (`cycode/Bridge.h`,
`CyCode::Core`). Le profil texte utilise
`cycode::text::SessionBridge::serviceStatus()` (`cycode/text/SessionBridge.h`,
`CyCode::TextBridge`). L’endpoint et l’accesseur natif partagent le même calcul
d’état : contexte et durée restent ceux du service réel, et le profil texte
n’annonce aucune analyse de projet. Gardez le bridge vivant pendant les appels.
Aucun second Engine, appel réseau, serveur de langage ou opération SDK n’est créé.

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
MCP `service_status` expose l’inspection du cycle de vie ; l’arrêt natif reste
un contrôle de l’hôte propriétaire. Le [guide IA](doc:core-agents#connect)
décrit la préparation et les jobs.

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

## Grands projets et poursuite de l’énumération {#large-projects}

La découverte continue désormais dans un flux de fond possédé, avec publication
de lots triés immuables. indexing/complete distinguent la progression de la fin
d’un instantané ; les limites des pages ne tronquent pas le projet entier.
L’IDE suit la progression et construit l’arbre hors du thread graphique.
L’arrêt et l’actualisation terminent le processus possédé. Les vraies erreurs
restent explicites ; la mémoire de l’index de chemins dépend de sa taille.

Un test Windows du Core de production, explicitement autorisé, a utilisé le
moteur Unreal 5.5.4 et 37 679 commandes UBT réelles. Les 177 797 chemins selon
les règles d’exclusion correspondent exactement à une énumération indépendante.
Diagnostics Core/CoreUObject, navigation, complétion, aperçu de renommage local
et Apply/Undo ont été vérifiés sans sauvegarder les sources. Un défaut de
publication des diagnostics après Undo rapide a été reproduit puis corrigé par
le point de contrôle de clangd. Le guide source conserve les mesures et limites.
L’export UBT a été préparé explicitement : la préparation automatique d’un moteur
seul et la complétude de l’index sémantique ne sont pas établies. Ce test ciblé
ne réactive pas les lancements locaux d’IDE ou d’exécutables de test natifs.

Le Core de production passe maintenant le renommage réel
`FDateTime::ParseHttpDate` sur trois fichiers dès la première demande, avec
Apply/Undo et refus d’une portée modifiée. Les positions résistent à une édition
valide d’en-tête puis à sa restauration. Dix navigations répétées dans World.cpp
mesurent environ 0,52 ms de médiane avec le cache. Il s’agit d’un passage Windows
avec caches en cours de chauffe, sans comparaison contrôlée aux mesures antérieures.

La vérification MCP précédente couvrait 236 opérations métier et 21 contrôles SDK ; le contrat actuel ajoute `analysis_results`. Les
permissions de lecture, la progression, l’annulation jusqu’à l’arrêt natif, les
codes d’erreur, buffers Unicode partagés, jobs privés et arrêt normal du Core ont
été vérifiés. Le [guide IA](doc:core-agents#connect) précise les contrats et limites,
dont les lots reprenables, les observations paginées et la complétude sémantique globale inconnue.

Le plafond de 128 fichiers est supprimé : 160 vraies sources compilées de
Runtime/Core ont passé quatre lots, avec diagnostics actuels sans erreur,
observations paginées et seulement trois documents utilisateur conservés ouverts.
L’inventaire retrouve 37 557 unités Unreal uniques ; cet échantillon réduit ne
démontre pas la sémantique exhaustive du moteur.

Le parcours MCP à 237 opérations a ensuite validé découverte, permissions,
progression, annulation, reprise et pages typées, puis a été interrompu entre
Apply et Undo. Le propriétaire a fourni une alerte Kaspersky PDM correspondante
sur le CLI de production. Les exécutions natives et reconstructions suivies de
relances sont suspendues ; la validation MCP finale n’est pas déclarée réussie.
Les sources disque sont intactes. Les contrôles de source, données et documentation
continuent, sans contournement de protection ni redémarrage automatique.

**Livraison actuelle :** les noms temporaires indépendants corrigent le
motif de sauvegarde/publication observé. Le propriétaire a autorisé la
compilation de production corrigée. Recherches et navigation MCP CyCodeIDE
et douze cycles de sauvegarde/récupération passent avec Kaspersky actif,
sans nouvelle alerte observée. Les paragraphes d’enquête ci-dessous sont
historiques : incidents, suspensions et validations interrompues. Ils ne
décrivent pas la procédure actuelle. Aucun verdict de faux positif de
l’éditeur n’est revendiqué. Voir [l’installation](core-installation#distribution).

## Enquête antivirus Windows {#antivirus}

Le propriétaire a signalé une alerte PDM sur le CLI de production. Le comportement
exact détecté est inconnu ; deux événements Windows avp récupérés identifient
désormais son SHA-256 d’origine, malgré l’absence du fichier à son emplacement
de compilation. Les exécutions natives et
reconstructions suivies de relances restent suspendues. Les sources Windows
limitent désormais l’héritage de handles des enfants et refusent le lancement
d’un serveur sans responsabilité vérifiée de son job et reprise réussie du thread.
La syntaxe MSVC est validée ; le comportement natif et le verdict antivirus n’ont
pas été retestés.

`app/scripts/antivirus_evidence.py` prépare un rapport JSON local et une demande
d’analyse. Il calcule seulement l’empreinte des fichiers explicitement choisis,
sans lancer, copier ou recréer l’exécutable, inclure les contenus de rapports
privés ni envoyer de données. Un original ou une empreinte absents restent
explicitement indisponibles, sans substitution par une autre compilation.

Aucun certificat payant n’est nécessaire pour cette enquête. La
[procédure officielle de réanalyse](https://support.kaspersky.com/common/error/other/1870)
et le [programme gratuit pour développeurs](https://www.kaspersky.com/partners/allowlist-program)
permettent une étude par l’éditeur, sous conditions d’éligibilité et d’examen.
Aucun dossier n’a été envoyé, aucune protection modifiée et aucun faux positif
confirmé. Le rapport détaillé de l’événement reste nécessaire pour identifier
l’opération détectée.

Le jeton natif utilise désormais l’aléa sécurisé du système et un fichier privé
avant écriture : ACL protégée utilisateur/LocalSystem sur Windows, mode 0600 sur
POSIX. La publication atomique évite la reprise des anciennes ACL et l’intervalle
avant chmod. Le protocole et les responsabilités du SDK restent identiques ; la
validation native reste à effectuer. L’outil importe les exports XML avp choisis
explicitement, en excluant les commandes privées et les empreintes contradictoires.
Aucune quarantaine ni configuration de protection n’a été consultée ou modifiée.

## Étapes restantes {#limits}

Les mesures sur de grands projets Unreal/MSBuild, la couverture complète de
l’invalidation du contexte et des dépendances, la synchronisation incrémentale,
les mesures concurrentes/annulables et les transformations avancées par commandes
restent à poursuivre. Les éditions sémantiques existantes conservent les aperçus,
contrôles de révision/disque, sauvegardes explicites et annulations. Les actions
non prises en charge restent indisponibles. La présentation IDE et l’identité
de son installateur/updater exigent leur propre validation avant publication ;
l’ancien updater reste désactivé dans cette version de développement.

Le propriétaire a ensuite autorisé l'installation de production et sa validation
MCP. Le runtime installé a exécuté recherches, évaluation MSBuild de 62 unités,
diagnostics sans erreur sur deux fichiers observés, navigation et rédaction
protégée du guide CyCodeIDE. Kaspersky a de nouveau supprimé le CLI installé à
19 h 20 (Europe/Paris). Son MD5 correspond au build. L'acceptation complète reste
interrompue, le MCP est désactivé et la publication binaire suspendue. Aucune
relance native automatique n'a suivi. Voir [l'installation](core-installation#distribution).


La bibliothèque native de mise à jour possède une identité consommateur immuable, distincte de la version Core. Voir [les profils de mise à jour](doc:updates#products).

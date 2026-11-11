---
id: analysis-progress
title: Progression persistante de l’analyse
description: Reprendre le parcours natif, distinguer les preuves historiques et mesurer les grands projets.
section: Pour commencer
parent: native-core
order: 265
sourceRevision: 2
---


## Périmètre et preuves {#scope}

`analysis.prepare` parcourt les vraies commandes de compilation, avec un seul
tampon temporaire natif à la fois. Les documents déjà ouverts restent propriétaires
de leur contenu. Inventaire, fichiers visités, diagnostics actuels et complétude
de l’index sont des observations différentes. Le même propriétaire natif sert les
clients et le véritable SDK/MCP CyToolsCore.

## Enregistrer et reprendre le parcours {#persistence}

Activez explicitement les métadonnées natives dans `.cycode/analysis-progress` :

```json
{"scope":"compilation-database","persist":true,"batchSize":64}
```

Conservez l’identifiant `checkpoint` émis. Le processus courant poursuit un lot
avec `continuation`. Après fermeture, `analysis_checkpoints` retrouve les
identifiants et `analysis_prepare` reprend un instantané :

```json
{"checkpoint":"analysis-0123456789abcdef0123456789abcdef","batchSize":64}
```

Ce nom illustre le format ; utilisez l’identifiant réellement retourné. Un lot de
taille zéro inventorie ou restaure les métadonnées sans analyser de source.
Le parcours n’est pas limité à 128 fichiers. La réponse initiale contient au plus
128 lignes ; `analysis_results` fournit les suivantes, par pages de 256 au maximum.

La reprise conserve l’ordre du parcours, pas une preuve du compilateur. Les lignes
restaurées deviennent `diagnostics: unknown`, avec `evidence: historical-checkpoint`.
Leur état antérieur reste dans `previousDiagnostics`. Les anciennes révisions sont
des métadonnées historiques, jamais des autorisations actuelles de modification :
relisez d’abord le document vivant. `historicalObserved` et `currentPassObserved`
séparent les observations. Une restauration dans le même processus, après une
modification de document, applique également cette invalidation explicite.

La base de compilation est protégée par BLAKE2b-256 ; configuration, chemin et
métadonnées doivent aussi correspondre. Sinon, un nouvel inventaire est requis.
Un verrou système réserve l’écriture. Les chemins restent relatifs au projet,
les redirections sont refusées. Le format 2 publie des pages immuables de 256
entrées au plus (16 Mio par page) et un manifeste atomique de 32 Mio. L’ensemble
peut dépasser 32 Mio ; seules les pages nouvelles/modifiées sont écrites. Le
format 1 reste lisible.
Les instantanés ne stockent aucun texte source. Leur publication atomique utilise
`CyCode::Files`. Une annulation conserve le préfixe terminé. La persistance après
fermeture normale ne prouve ni la récupération après coupure électrique ni la
persistance de l’index du compilateur. Les identifiants ne sont ni des secrets
d’accès ni un ordonnanceur SDK supplémentaire.

## Erreurs isolées de sources {#failures}

Une source illisible, non UTF-8 ou trop volumineuse produit une ligne inconnue
avec son erreur, puis les autres unités continuent. Un fichier non ouvert porte
la révision zéro, inutilisable pour modifier un document. Annulation, conflit de
révision du document/espace de travail et panne de serveur restent des échecs
distincts ; aucun ne devient un diagnostic propre. Les erreurs du compilateur
restent visibles sans interrompre tout le parcours en lecture seule. Le renommage
conserve ses conditions plus strictes sur tous les candidats.

## Mesurer le travail réel {#measurement}

Chaque source traitée fournit `elapsedMilliseconds`. Pendant le travail,
`analysis_status` publie `currentPath` et `currentFileMilliseconds`, sans relancer
le compilateur. Les agrégats distinguent diagnostics propres, erreurs, inconnus et
fichiers restants. Ces nombres ne prouvent pas une couverture exhaustive des
macros Unreal, Blueprints, configurations ou de l’index d’arrière-plan.

Le script volontaire `app/scripts/benchmark_unreal_campaign.py` utilise la vraie
base UBT et le MCP officiel. Il conserve lots, temps par fichier, mémoire observée
de l’arbre natif et vérification des empreintes des sources sélectionnées et de la
base. Une limite de temps ou d’unités laisse une progression en attente, pas une
analyse prétendument terminée. Gardez les mêmes arguments clangd lors d’une reprise.

```text
python app/scripts/benchmark_unreal_campaign.py --run-native --core . --executable <Core> --clangd <clangd> --workspace <racine-UE> --database <vraie-base-UBT-compile_commands.json> --max-seconds 600 --max-units 0 --output <nouveaux-resultats>
python app/scripts/benchmark_unreal_campaign.py --run-native --core . --executable <Core> --clangd <clangd> --workspace <racine-UE> --database <vraie-base-UBT-compile_commands.json> --checkpoint <identifiant-emis> --output <nouveaux-resultats>
```

## Validation {#validation}

`verify_analysis_persistence.py --run-native` utilise le vrai MSBuild, clangd et
MCP sur plus de 128 sources : redémarrage, annulation, gardes de contexte/format et
réservation de l’écriture. Aucun exécutable natif de test généré ni IDE graphique
n’est lancé. `verify_inspection_edges.py` vérifie les bornes des vrais résultats
clang-tidy et les commandes absentes. Ces scripts sont des vérifications
Windows/MSBuild explicites ; les contrôles ordinaires de sources ne les lancent pas.


Le cache de navigation distingue désormais les révisions uniques des documents
et les ouvertures temporaires de l’analyse en lecture seule. Le parcours conserve
les instantanés inchangés ; une véritable édition et les contrôles du contexte,
du backend et du système de fichiers les invalident toujours. Le test
`verify_navigation_analysis.py --run-native` utilise de vraies requêtes compilateur.

Sur Unreal, 37 557 unités sont inventoriées. Un premier processus en observe 104,
puis une reprise ajoute 64 observations propres. Les 104 précédentes deviennent
explicitement historiques et inconnues ; 37 389 unités restent à visiter.
Le renommage DateTime/HTTP sur trois fichiers passe toujours Apply/Undo sans
sauvegarder les sources. L’ensemble du moteur Unreal n’est donc pas encore validé.

Sur le petit projet MSBuild de navigation, les 16 requêtes SDK/MCP concurrentes
utilisent le cache après correction, contre 3 auparavant ; la médiane passe
d’environ 72 à 37 ms. Ces mesures locales ne démontrent pas une supériorité globale
face à Rider/CLion. Huit transformations réellement proposées par clangd passent
aussi aperçu/Apply/Undo, dont les déplacements de corps de fonction sur deux
fichiers. Les actions absentes du backend sont signalées séparément.

## Limites {#limits}

Les fichiers restaurés restent un historique de parcours, sans nouvelle analyse.
Relancez un inventaire pour une passe intégralement fraîche. La taille maximale des
tampons sources reste en vigueur : les fichiers refusés sont signalés. La découverte
des instantanés est bornée et rapporte les entrées invalides ou tronquées sans
effacer de données. Linux/macOS, coupure électrique, courses malveillantes du système
de fichiers et validation graphique complète restent à vérifier. GitHub Actions
reste désactivé. Aucune publication ni mise à jour installée n’est automatique.

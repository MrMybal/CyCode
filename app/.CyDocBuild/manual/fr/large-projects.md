---
id: large-projects
title: Grands projets et texte paginé
description: Données natives progressives, documents paginés pour les agents et transactions communes.
section: Pour commencer
parent: native-core
order: 266
sourceRevision: 1
---

## Stockage et propriétaire {#storage}

Le même Engine natif possède les tampons ordinaires et le nouveau profil paginé
UTF-8. Un fichier ne peut pas être ouvert dans les deux profils. CyToolsCore garde
les identités, tâches et permissions SDK/MCP ; aucun modèle documentaire Python
n’est ajouté. Les contenus immuables privés de `.cycode/working-data` appartiennent
aux documents et aperçus, pas aux fichiers temporaires des tâches SDK. Ils peuvent
contenir des sources : excluez `.cycode` des publications et du contrôle de versions.
La fermeture normale retire seulement les fichiers inchangés appartenant au
processus. Les contenus modifiés/inconnus sont conservés ; ni reprise après crash
ni nettoyage automatique des données interrompues ne sont implémentés.

Le profil classique texte/GUI/LSP conserve sa politique de compatibilité de 4 Mio.
Pour les gros fichiers, l’agent choisit explicitement le stockage paginé : import
et validation par blocs de 64 Kio, puis références à des morceaux immuables.
Ce profil n’a pas de plafond choisi de 4 Mio pour le fichier entier. Il nécessite
de l’espace disque pour sa copie de référence et les éditions. UTF-8 est vérifié
progressivement ; données binaires/NUL et encodages invalides sont refusés.
Les octets BOM et CRLF sont conservés sans normalisation implicite.

## Lecture et édition par les agents {#agents}

1. `documents_read_page` ouvre une référence de travail et retourne au plus
   262 144 octets. La taille de page va de 4 à 262 144. Conservez l’identité,
   la révision et `nextOffset`. Le fichier source n’est pas réécrit.
2. Pour la suite, passez `offset: nextOffset` et `expectedRevision`. Les points
   Unicode UTF-8 ne sont jamais coupés. Les positions sont des OCTETS, pas des
   coordonnées UTF-16 ou des numéros de ligne.
3. `documents_prepare_byte_edit` prépare une modification de la révision vivante :
   `offset` en octets, `length` supprimée et texte inséré borné par requête.
4. Examinez `changes_preview` par pages, avec `beforeOffset` et `afterOffset`
   indépendants, puis leurs `nextBeforeOffset`/`nextAfterOffset`. Les petits
   aperçus classiques restent compatibles ; les gros aperçus intégraux sont
   explicitement refusés avec indication de pagination, jamais tronqués en secret.
5. Les commandes communes `changes_apply` et `changes_undo` modifient uniquement
   le tampon natif. Relisez ensuite sa révision actuelle.
6. `documents_save_paged` sauvegarde explicitement cette révision par le writer
   atomique commun, en vérifiant empreinte de référence, morceaux et lecture seule.
   `documents_close_paged` ferme ; un tampon modifié exige `discard: true`.

L’import, la préparation et la sauvegarde prolongés libèrent le verrou Engine ;
une réservation du document refuse les opérations concurrentes incompatibles.
Ces opérations et la publication sont actuellement non annulables, comme déclaré
dans le SDK. Les révisions et aperçus appartiennent au Core courant. Ce profil
n’a pas encore de LSP, API par lignes, adaptateur GUI, Enregistrer sous ou reprise
des éditions après crash. Une édition paginée ne prouve pas l’analyse compilateur
d’un très gros fichier.

La recherche globale existante `search_text` inclut les modifications paginées
non sauvegardées : instantané privé écrit progressivement, suppression des
anciennes correspondances disque, ripgrep commun annulable puis vérification de
la révision vivante. Elle nécessite de l’espace temporaire et conserve les
budgets de résultats, temps et sortie. Les octets ne deviennent pas des positions LSP.

Les requêtes compilateur nécessitent de sauvegarder ou fermer les éditions
paginées, car elles n’ont pas d’overlay LSP. La recherche textuelle globale inclut
ces éditions. `analysis_inspect` accepte une source C/C++ paginée déjà ouverte et
propre avec sa révision et sa vraie commande de compilation ; les références
utilisent des empreintes progressives plutôt qu’une copie intégrale du texte.
Les tampons paginés modifiés/occupés refusent les inspections disque. Il s’agit
de clang-tidy/Clang Static Analyzer, pas de navigation clangd sur le tampon paginé.

## Bases de compilation et progression {#analysis}

Inventaire et inspections consomment une commande de compilation à la fois.
Les empreintes BLAKE2b-256 utilisent des blocs de 64 Kio : ces chemins n’imposent
plus 64 Mio à la base entière. Le JSON est strict, accepte le BOM UTF-8 initial,
vérifie l’annulation et borne chaque commande individuelle à 8 Mio de travail.
Les compilateurs et serveurs de langage gardent leurs propres contraintes.

Le format 2 publie un manifeste atomique et des pages immuables contenant au plus
256 chemins/observations. Un lot réécrit uniquement les pages nouvelles ou modifiées.
L’ancien état reste valide avant publication du manifeste. L’ensemble peut dépasser
les 32 Mio de l’ancien fichier unique. Chaque page est bornée à 16 Mio, le manifeste
à 32 Mio. Le format 1 reste lisible et est migré lors de la sauvegarde. Les anciennes
observations restent historiques et inconnues ; empreintes, contexte et bail
exclusif sont conservés. Les pages non référencées sont retenues sans suppression
automatique. Les lignes immuables sont partagées entre copies de session ; index
de chemins et observations occupent toujours une mémoire proportionnelle à leur
nombre. Ce jalon ne persiste pas les AST de clangd.

## Transactions volumineuses {#transactions}

Les aperçus ordinaires et références de renommage stockent leurs contenus privés
sur disque, au lieu de garder une copie complète supplémentaire avant/après/disque
par fichier. Le plafond global de 16 Mio est retiré. Les éditions restent celles
du compilateur, sans remplacement textuel ni aperçu global incomplet.

Apply vérifie toutes les révisions, références disque et données immuables, puis
prépare les chaînes ordinaires avant la première mutation. Les tampons classiques
échangent leurs chaînes ; les documents paginés échangent leurs instantanés de
morceaux. Undo conserve les mêmes contrôles. Les notifications suivent la validation
complète. Les tampons classiques et résultats préparés consomment encore de la RAM ;
une grosse validation peut conserver le verrou Engine. Espace disque, trames,
backends et allocations restent des contraintes réelles. Aucune garantie sans
limite matérielle, contre toute course de fichiers ou panne électrique n’est faite.

## Reproduire et interpréter les preuves {#validation}

```text
python app/scripts/verify_large_project_capacity.py --run-native --core . --executable <Core> --clangd <clangd> --clang-tidy <clang-tidy> --output <nouveau-dossier>
```

Le test Windows explicite utilise le vrai SDK/MCP, les commandes MSBuild évaluées,
clangd et clang-tidy. Il crée uniquement des données sources, jamais un exécutable
de test ou la GUI. psutil est facultatif ; sans lui la mesure mémoire est déclarée
indisponible. Les cas couvrent un document UTF-8 de 96 Mio, une base réelle de
70 Mio complétée d’espaces JSON valides, plus de 32 Mio de métadonnées historiques
inconnues injectées comme données de test, et un renommage compilateur sur neuf
fichiers dépassant 16 Mio avant/après. Les métadonnées injectées ne constituent
jamais une preuve compilateur. Consultez le rapport pour les résultats effectifs.

Linux/macOS et la présentation graphique sont reportés. Les Actions GitHub restent
désactivées ; aucune installation, release ou publication n’est implicite.

Les tests Windows de production passent : texte paginé de 96 Mio avec BOM/CRLF/
Unicode, recherche littérale/regex non sauvegardée, refus lecture seule/conflits
et Apply/Save/Undo ; inspection réelle d’un C++ paginé propre de 5 Mio ; inventaire
et inspection d’une base de 70 Mio ; 35 Mio de métadonnées inconnues injectées avec
refus de corruption/imbrication excessive ; et renommage réel sur neuf fichiers
avec 20 Mio avant/après. La hausse de mémoire résidente du Core observée pour le
texte de 96 Mio reste sous 1 Mio sans serveur de langage actif. Ce scénario ne
mesure pas la mémoire globale d’un projet. Le rapport conserve chaque empreinte
binaire et le périmètre exact de ses vérifications.

---
id: large-projects
title: Grands projets et texte paginé
description: Données natives progressives, documents paginés pour les agents et transactions communes.
section: Pour commencer
parent: native-core
order: 266
sourceRevision: 5
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
n’a pas encore d’overlay LSP ni d’adaptateur graphique. `documents_read_lines`
lit désormais par ligne et colonne UTF-16, avec un index natif d’un repère toutes
les 1024 lignes. Le BOM initial est exclu des coordonnées ; CRLF, CR et LF sont
reconnus et les demi-paires de substitution refusées. Une longue ligne peut
occuper plusieurs pages : suivez `nextLine` et `nextCharacter` avec la révision.
Les curseurs en octets restent distincts.
`documents_save_paged_as` sauvegarde progressivement dans un nouveau fichier
du projet et réassocie l’identité du document avec une nouvelle révision.
Le fichier d’origine est préservé, les destinations existantes/ouvertes refusées
et les anciens plans liés au chemin deviennent périmés. Cette réassociation
de ressource n’a pas d’annulation d’éditeur. Une édition paginée ne prouve pas l’analyse compilateur
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

## Récupération paginée explicite et persistante {#recovery}

`documents_checkpoint_paged` persiste explicitement un instantané complet dans
`.cycode/working-data/core-<id>`, par écriture progressive et manifeste privé
atomique. Le fichier source n’est pas sauvegardé. Ces checkpoints sont conservés
après arrêt normal et crash du processus ; ils nécessitent de l’espace disque.
Il ne s’agit pas d’une sauvegarde automatique après chaque frappe.

Après redémarrage, `documents_paged_checkpoints` découvre au plus 256 entrées
dans 4096 dossiers. Cette liste vérifie métadonnées et tailles, pas les empreintes
des contenus. `documents_preview_paged_checkpoint` vérifie l’empreinte complète,
fournit des pages UTF-8 en octets et indique `diskMatchesBaseline`.
Les révisions historiques ne donnent aucun droit d’édition courant. Rouvrez
l’original avec `documents_read_page`, puis préparez
`documents_restore_paged_checkpoint` avec sa révision vivante. Prévisualisation,
Apply et Undo réutilisent le propriétaire transactionnel commun. La sauvegarde,
ou Enregistrer sous, reste explicite ; Undo restaure l’instantané vivant précédent.

La restauration refuse une référence disque d’origine modifiée ; l’aperçu reste
consultable pour réconciliation. Contenus corrompus, redirections et métadonnées
trop imbriquées sont refusés. Aucune suppression silencieuse ou collecte
automatique n’est introduite. Un scénario Windows arrête brutalement son seul
Core de production après publication, puis vérifie reprise, Undo, Enregistrer
sous, conflits et corruption. Cela valide un checkpoint explicitement publié
après crash du processus, pas une coupure électrique ni les frappes ultérieures.

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

## Undo/Redo natif commun et reçus de commit {#history}

L’Engine possède un seul historique en mémoire pour les transactions ordinaires
et paginées de `changes.apply`. Les vues gardent identifiants émis et intentions,
sans second LargeText, copie du texte ni pile Undo. Cet historique n’inclut pas
les remplacements directs de document, ressources, paramètres, VCS ni les éditions
d’un autre processus Engine.

1. Consultez `changes_history_status` : les têtes `undo` et `redo` indiquent
   l’identifiant original, le titre et les chemins affectés. Gardez sa `revision`.
2. Récupérez les métadonnées vivantes d’identité/révision de **tous** ces documents
   sur le même Engine. Appelez `changes_history_prepare` avec `id`, `direction`
   (`undo` ou `redo`), `expectedHistoryRevision` et un tableau `documents` donnant
   `path`, `expectedDocumentId` et `expectedRevision` pour chaque fichier.
3. Examinez le nouveau plan par pages bornées `changes_preview`. Il restaure des
   instantanés natifs immuables : une liste textuelle `edits` vide est normale.
   Appliquez son nouvel ID via `changes_apply`. Redo conserve les confirmations
   nécessaires avec les identifiants du nouveau plan. Préparer ne modifie rien.

Avant le premier échange de tampon, le Core revérifie la tête d’historique,
identités/révisions vivantes, instantanés ou contenus exacts, références disque
et contenus cibles. Un plan périmé refuse la transaction entière. Une nouvelle
édition appliquée supprime la branche Redo. L’ancien `changes_undo` conserve ses
gardes strictes ; une restauration native causale permet plusieurs annulations.

Au plus **256 transactions originales** restent retenues au-delà du cache normal
de 8 aperçus/20 minutes. `retentionLimit` et `dropped` exposent cette borne et les
entrées retirées par rétention, fermeture ou changement de chemin. Fermer/rouvrir
crée une nouvelle identité inaccessible à l’ancien historique. Celui-ci ne survit
pas au redémarrage : utilisez la récupération explicite décrite plus haut.
Les contenus privés demandent de l’espace disque ; la préparation peut garder
le verrou Engine. Préparation d’historique et commit ne sont pas annulables.

Apply/Undo renvoient `commit` avec transaction réellement appliquée, action,
révision d’historique et `{path,id,revision,dirty,bytes,storage}` pour chaque
document. Le reçu est préparé avant mutation sous le même verrou. Le client
natif acquitte directement cette révision : un agent peut déjà avoir modifié le
document après ce commit. Ne remplacez pas le reçu par une relecture ultérieure.
Une notification après commit échouée donne `notificationsComplete: false` :
l’édition a réussi, un rafraîchissement distinct peut être nécessaire. Pour
l’appel natif direct, une exception de préparation signifie aucun commit.
Une perte de transport/sérialisation RPC reste ambiguë : interrogez `changes_status`
avec l’ID émis, examinez état et dernier reçu puis rapprochez l’état avant une
nouvelle mutation. Un ID inconnu/évincé n’autorise pas une répétition aveugle.
Aucune action n’enregistre implicitement la source.

Des régressions ordinaires/paginées sont ajoutées à la suite native existante :
Undo/Redo successifs, reçus, plans périmés, conflits disque, éviction des aperçus,
rétention et fermeture/réouverture. Elles nécessitent un lancement natif explicite ;
contrôles sources/schémas et compilation des bibliothèques ne valident pas ces
scénarios à l’exécution.

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

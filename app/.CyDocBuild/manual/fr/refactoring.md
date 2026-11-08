---
id: refactoring
title: Complétion et refactorisation
description: Examinez la complétion, le renommage et les actions de code avant d’enregistrer.
section: Travailler dans CyCode
order: 60
parent: editing
sourceRevision: 10
---

## Compléter le code {#completion}

Utilisez Ctrl+Espace pour demander une complétion au service de langage actif. L’acceptation d’un élément peut insérer ses imports associés dans une seule transaction de l’éditeur. Parcourez les champs d’extrait avec Tab / Maj+Tab ; Échap termine l’extrait. Ctrl+Espace quitte l’extrait courant pour demander une autre complétion.

## Extraits personnalisés du projet {#snippet-catalogue}

Choisissez **Édition > Modifier les extraits du projet...** pour ouvrir `.cycode/snippets.code-snippets`, à la racine de l’espace de travail. Au premier accès, cette action explicite crée le fichier avec un objet JSON vide dans son texte non enregistré ; les fichiers et textes existants sont conservés. Modifiez les définitions en JSON UTF-8 puis enregistrez. Choisissez **Édition > Insérer un extrait...**, ou recherchez cette action dans la palette de commandes. Le profil Rider utilise Ctrl+J ; Visual Studio utilise Ctrl+K, Ctrl+X. Le profil VS Code propose l’action sans raccourci par défaut. Recherchez par nom, préfixe ou description, choisissez avec les flèches/Entrée et fermez avec Échap. Cette liste explicite fonctionne sans serveur de langage, y compris pour Verse.

```json
{
  "Classe": {
    "prefix": ["class", "cls"],
    "scope": "cpp",
    "description": "Classe nommée selon le fichier",
    "body": ["class ${1:$TM_FILENAME_BASE}", "{", "\t$0", "};"]
  },
  "Message Verse": {
    "prefix": "msg",
    "scope": "verse",
    "body": "Print(\"${1:Bonjour}\")$0"
  }
}
```

Les noms identifient les définitions. `prefix` est une chaîne ou un tableau ; `body` est du texte ou un tableau joint par des sauts de ligne. `scope`, facultatif, énumère les langages séparés par des virgules ; omettez-le pour tous les fichiers. Utilisez `cpp`, `csharp`, `rust`, `java`, `javascript`, `typescript`, `html`, `css`, `python`, `hlsl` ou `glsl` ; JSX/TSX utilisent `javascriptreact`/`typescriptreact`. Les autres fichiers utilisent leur extension en minuscules, comme `verse` ou `log` ; le texte et les fichiers sans extension utilisent `plaintext`, Markdown utilise `markdown`. Enregistrez les changements puis redemandez la liste. Les commentaires, propriétés inconnues et modèles de fichiers sont refusés.

L’insertion exige un fichier enregistré de l’espace de travail, modifiable, avec un seul curseur ; l’encadrement d’une sélection est décrit ci-dessous. Seul un préfixe de mot correspondant avant le curseur est remplacé, en respectant la casse ; sinon le texte est inséré au curseur. Le texte suivant est conservé. Les champs numérotés, variables, indentation et Undo réutilisent la complétion habituelle. Un changement de document, de curseur ou du catalogue enregistré exige une nouvelle demande. Le catalogue est limité à 256 Kio, 256 définitions, 16 préfixes par définition et 512 propositions, avec des corps de 8 Kio. Les erreurs et limites sont affichées dans la liste. Il n’est pas fusionné avec les propositions automatiques du serveur. Le stockage personnel et les catalogues des plugins actifs sont décrits ci-dessous.

## Encadrer du code sélectionné {#snippet-selection}

Sélectionnez du code puis choisissez **Édition > Encadrer avec un extrait...**
(Rider Ctrl+Alt+J). Les profils VS/VS Code proposent la commande sans raccourci par
défaut. Insérer un extrait utilise aussi une sélection existante. Enregistrez par exemple :

```json
{
  "Parenthèses": { "prefix": "paren", "body": "(${1:$TM_SELECTED_TEXT})$0" },
  "Région": { "prefix": "region", "body": ["#region ${1:nom}", "$TM_SELECTED_TEXT", "#endregion$0"] }
}
```

TM_SELECTED_TEXT contient la sélection initiale, insérée littéralement : dollars,
barres obliques inverses, accolades et code Unicode ne créent pas de champs
d’extrait. Le premier exemple crée un champ modifiable contenant la sélection.
Une transformation explicite peut la modifier. Ces définitions insèrent du texte ;
limitez leur portée aux langages adaptés.

Une sélection peut couvrir plusieurs lignes, dans les deux sens, jusqu’à 8 Kio
de source UTF-8. Le curseur de la demande doit être à une extrémité. Le corps doit
effectivement utiliser la variable : une mention dans un repli inutilisé, du texte
échappé ou un choix littéral ne suffit pas. Les corps incompatibles sont grisés
avec une raison puis refusés pendant la préparation. Le remplacement doit égaler
exactement la plage émise. Déplacer l’ancre, le curseur ou modifier le document
invalide la liste et l’acceptation en attente, même si le curseur reste au même endroit.

L’encadrement conserve la disposition littérale du corps et de la sélection, sans
réindentation automatique ; les nouvelles lignes du corps ne reçoivent pas
l’indentation courante. Formatez séparément si nécessaire. Il utilise la même
transaction de complétion/Undo ; Échap ferme sans modifier, et accepter n’enregistre
pas. Les complétions serveur ne capturent pas le texte sélectionné.
## Extraits personnels {#user-snippets}

Choisissez **Édition > Modifier les extraits personnels...**, aussi dans la palette
d’actions sans raccourci par défaut. L’éditeur JSON commun fournit Undo/Redo et la
recherche du brouillon. Enregistrer ou Ctrl+S valide, enregistre explicitement puis
ferme ; insérer un extrait n’enregistre pas le document cible. Recharger et fermer/
quitter demandent avant d’abandonner un brouillon modifié. Les erreurs conservent
le brouillon. Une opération de stockage en attente bloque la fermeture ; le changement
d’espace de travail attend la fermeture de cet éditeur. Pas d’enregistrement automatique
ni d’Undo persistant du fichier après fermeture.

Le bureau et la CLI installée utilisent data/settings/snippets/user.code-snippets,
partagé entre les projets de cette installation portable. Lire un fichier absent
propose un JSON vide sans le créer. Utilisez le format du projet ci-dessus, au plus
256 Kio et 256 définitions. Un JSON UTF-8 lisible mais mal formé peut être réparé ;
un stockage illisible, UTF-8 invalide, binaire, trop grand ou redirigé est conservé.
Corrigez-le extérieurement puis rechargez. Une sauvegarde depuis une autre instance
produit un conflit de révision ; rechargez explicitement plutôt que forcer l’écriture.

Ordre de fusion : projet, personnel, puis plugins actifs. Les entrées personnelles
affichent [user] et conservent leur provenance ; les doublons restent distincts. Les
limites communes de 512 propositions et de données s’appliquent. Les variables
décrivent le document et l’espace cibles. Les demandes/préparations natives revérifient
le contenu enregistré ; l’interface utilise des générations de lecture locales,
pas une lecture par image ni une surveillance des autres instances. Les plans SDK
concrets déjà retournés gardent leurs contrôles habituels. Le stockage de compte OS,
la synchronisation cloud, les imports externes et la fusion serveur restent absents.

Les clients natifs/SDK/ABI2 utilisent snippets_user_read/save et le même propriétaire.
Écrire exige settings.write, distinct de workspace.write. L’intégration active un
dossier absolu fixe ; Engine par défaut et les sessions graphiques de test/smoke ne
lisent ni ne créent ce stockage. Ces préférences ne sont pas des fichiers temporaires
de tâche.

## Créer un extrait depuis le code sélectionné {#capture-snippet}

Sélectionnez une plage dans un document enregistré et modifiable de l’espace de
travail, puis **Édition > Créer un extrait personnel depuis la sélection...**,
aussi dans la palette d’actions sans raccourci par défaut. Le texte courant, avec
ses modifications non enregistrées, est copié dans le même éditeur JSON personnel.
La source et le catalogue enregistré restent inchangés. Modifiez le nom généré
Selection snippet, le préfixe snippet, la portée du langage source et le corps,
puis enregistrez explicitement avec Enregistrer/Ctrl+S. Les valeurs des définitions
existantes sont conservées ; le brouillon complet est mis en forme avec deux
espaces d’indentation. Les modifications JSON suivantes ont Undo/Redo ; Abandonner,
Recharger/Fermer conservent les décisions habituelles de brouillon non enregistré.

Dollars, antislashs, accolades fermantes, sauts de ligne et Unicode sont copiés
littéralement. Le corps se termine par $0, sans champ numéroté. La sélection est
limitée à 8 Kio ; le corps échappé et le curseur final doivent aussi tenir dans
8 Kio. Un catalogue plein/invalide ou désactivé refuse la capture ; aucun fichier
ni verrou n’est créé. Après une erreur, Recharger permet d’ouvrir le catalogue
enregistré pour le réparer. Une sauvegarde par une autre instance provoque le
conflit habituel lors de votre enregistrement explicite. Cette copie capturée ne
suit pas les modifications ultérieures de la source.

L’opération SDK commune en lecture seule est [snippets_user_draft](doc:operation-snippets-user-draft-snippets-user-draft).

## Extraits fournis par les plugins {#plugin-snippets}

Les plugins ABI2 actifs peuvent fournir des catalogues intégrés à la même liste.
Les définitions du projet passent d’abord, puis les définitions personnelles et les catalogues des plugins par ordre et ID.
Les préfixes identiques restent distincts ; les propositions affichent l’ID du
plugin propriétaire. La recherche couvre noms, préfixes, descriptions et titres/ID
des catalogues. Désactiver un plugin ou une dépendance requise invalide les demandes
et acceptations graphiques en attente. Une nouvelle demande lit le registre actif
sans exécuter de callback fournisseur.

Le registre contient au plus 32 catalogues / 1 Mio, en comptant les enregistrements
inactifs. Chaque catalogue conserve la syntaxe et les limites ci-dessus ; la liste
fusionnée comporte au plus 512 propositions, avec le budget habituel de complétion.
Le stockage personnel est fixé par l’hôte ; il n’y a pas de dossier de compte OS,
d’import de fichier externe ni de fusion
automatique avec les propositions serveur. Les plans SDK concrets déjà retournés
gardent leurs contrôles habituels de source/disque/application.

Le fournisseur Unreal actif propose ue_log/uelog, ue_property, ue_function et
ue_editor pour la portée cpp. Le dernier encadre une sélection avec WITH_EDITOR.
Ces modèles apparaissent aussi dans les autres espaces C++ quand le plugin est
actif ; ils n’interrogent pas la réflexion, n’ajoutent pas d’includes et ne compilent
pas le code. Vérifiez la classe englobante et les prérequis. Consultez les
[extraits Unreal](doc:unreal#code-snippets) et
[l’enregistrement par plugin](doc:plugin-api#snippet-catalogues).

## Variables de contexte des extraits {#snippet-variables}

Les extraits fournis par le serveur peuvent utiliser le nom du fichier (`TM_FILENAME`, `TM_FILENAME_BASE`), les chemins du document et de l’espace de travail, la ligne et le mot courants, le numéro de ligne initial et l’indice du curseur unique. `$TM_FILENAME_BASE` insère le nom sans sa dernière extension ; `${TM_CURRENT_WORD:identifier}` fournit une valeur de repli si le mot est vide. Ce repli peut contenir un champ modifiable, comme `${CUSTOM_NAME:${1:identifier}}`. Les valeurs sont insérées littéralement ; les champs numérotés conservent leur navigation par Tab et leur édition liée.

Le contexte vient de la demande de complétion initiale, avant que les imports supplémentaires déplacent les lignes. L’acceptation de la complétion et des imports reste une transaction Undo ; enregistrez séparément. TM_SELECTED_TEXT contient la sélection locale explicite ci-dessus, sinon une valeur vide. Le presse-papiers, les variables d’environnement, de date ou aléatoires et les variables simples inconnues sans repli sont indisponibles. Un extrait dépassant la syntaxe ou les tailles prises en charge est écarté des propositions ou refusé pendant la préparation.

## Transformer les variables des extraits {#snippet-transforms}

Un serveur de langage peut fournir `${VARIABLE/motif/format/options}`. Par exemple, dans `Actor.cpp`, `${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}` insère `ACTOR`. Pour initialiser un champ modifiable avec cette valeur, utilisez `${1:${TM_FILENAME/(.*)\.cpp$/${1:/upcase}/}}`. Le calcul est effectué une fois ; les modifications suivantes conservent les champs liés et la transaction Undo habituels.

Les formats acceptent la correspondance complète (`$0`), les captures numérotées (`$1`, `${1}`), le texte conditionnel (`${1:+alors}`, `${1:-sinon}`, `${1:sinon}`, `${1:?alors:sinon}`) et les conversions `upcase`, `downcase`, `capitalize`, `camelcase`, `pascalcase`, `snakecase`, `kebabcase`. Une capture vide sélectionne la branche sinon ; `:-` et `:sinon` conservent une capture non vide. Échappez un `/` littéral dans le motif avec `\/`. Les options sont `g`, `i`, `m`, `s`, `u`, chacune une seule fois.

La liste des propositions affiche le libellé du serveur pour les extraits transformés. Les regex sont calculées pendant la préparation native, hors du fil d’affichage. Une expression mal formée ou une limite dépassée refuse la préparation sans modifier le document. PCRE2 utilise toujours UTF-8 et les classes Unicode ; ce n’est pas un moteur ECMAScript. La casse utilise des conversions Unicode simples, indépendantes de la langue. Les conversions par mots normalisent la casse et reconnaissent les frontières de mots et d’acronymes. Sans correspondance, la valeur initiale est conservée ; les branches sinon s’appliquent uniquement aux correspondances réelles.

Les limites comprennent des motifs de 256 octets, 16 captures, huit transformations et 64 tentatives de correspondance par extrait, des entrées/sorties de 8 Kio et un délai coopératif de 50 ms avec des limites supplémentaires de calcul et de mémoire. C’est un sous-ensemble borné des extraits de style VS Code. Les transformations de champs interactifs (`${1/motif/format/options}`) et les autres familles de variables restent indisponibles. Les catalogues personnels sont pris en charge comme indiqué plus haut.

## Renommer et appliquer des actions {#rename}

1. Placez le curseur sur le symbole et choisissez Renommer, ou ouvrez les actions contextuelles de la plage courante.
2. Sélectionnez une proposition réelle du serveur. Les actions désactivées indiquent le contexte ou la capacité manquante.
3. Examinez les changements Avant/Après et les confirmations dans la vue de refactorisation.
4. Appliquez les changements aux textes en mémoire, puis enregistrez lorsque vous le souhaitez.

L’IDE refuse les modifications périmées, les chevauchements et les conflits sur disque. Une modification du code après l’aperçu peut demander une nouvelle requête. Le serveur détermine les refactorisations disponibles ; la coloration seule ne permet pas de renommer correctement les symboles.

---
id: workspace-files
title: Gérer les fichiers et la corbeille du projet
description: Créer, déplacer et restaurer les fichiers physiques en distinguant les modifications de texte.
section: Travailler dans CyCode
parent: editing
order: 41
sourceRevision: 1
---

## Parcourir les fichiers physiques {#files}

Ouvrez **Affichage > Fichiers** pour parcourir un dossier physique, y compris ses
dossiers vides et fichiers ignorés ou cachés. Projet et la recherche de noms
utilisent l’index source filtré par les exclusions. Un dossier peut donc être
visible dans Fichiers sans apparaître dans Projet. Actualisez la vue concernée
lorsque son observation est ancienne.

Sélectionnez un fichier ou dossier avant Renommer/Déplacer ou Supprimer. F2 et
Suppr agissent sur la ressource uniquement lorsque Projet ou Fichiers a le focus ;
ils ne renomment pas un symbole. Ctrl/Cmd+F active le filtre de Fichiers ou de la
corbeille. Échap annule la boîte de dialogue.

## Examiner une modification {#preview}

1. Enregistrez ou réconciliez les documents concernés.
2. Choisissez Nouveau fichier/dossier, Renommer/Déplacer ou Supprimer et examinez l’aperçu.
3. Utilisez des chemins relatifs au projet ; les dossiers parents de destination doivent exister.
4. Appliquez seulement l’aperçu encore courant. Une destination occupée est refusée.

Un déplacement conserve les identités des documents ouverts et leur annulation
de texte. Il ne réécrit pas les inclusions, solutions, noms de réflexion Unreal,
ressources ou points d’arrêt. L’entrée active de solution/projet ne peut pas être
déplacée/supprimée ainsi. Métadonnées protégées, chemins redirigés et liens
symboliques sont exclus. Les déplacements entre volumes et renommages limités
à la casse sur un système insensible à la casse ne sont pas proposés.

```diagram
{"type":"flow","title":"Examiner une opération sur les fichiers","nodes":[{"id":"choose","label":"Choisir une ressource","x":0,"y":0},{"id":"preview","label":"Examiner l’aperçu","x":230,"y":0},{"id":"apply","label":"Appliquer le plan courant","x":460,"y":0},{"id":"trash","label":"Conserver les données","x":690,"y":0}],"edges":[{"from":"choose","to":"preview"},{"from":"preview","to":"apply"},{"from":"apply","to":"trash","label":"Suppression"}]}
```

## Restaurer ou annuler {#trash}

Supprimer déplace les données dans `.cycode/trash`. **Corbeille du projet** permet
de restaurer une entrée après redémarrage. Examinez sa destination d’origine ou
choisissez-en une autre. Les destinations occupées et contenus modifiés/corrompus
sont refusés. Aucune suppression définitive, vidange ou expiration automatique
n’est proposée.

L’annulation d’opération sur les fichiers est distincte de celle du texte. Ses
plans sont locaux au processus et expirent ; la corbeille leur survit. Annuler
une création déplace son résultat dans la corbeille. Un texte ouvert supprimé
reste modifiable avec un indicateur de fichier absent ; Enregistrer peut recréer
son chemin. La corbeille conservée ne peut pas écraser ce nouveau fichier.
Enregistrez ou fermez un buffer modifié avant de tenter une restauration.

Une opération est limitée à 4 096 entrées, 64 niveaux, 256 Mio et 128 documents
ouverts concernés. Le navigateur physique peut aussi signaler une liste tronquée.
Cette limite ne signifie pas que les autres fichiers ont disparu.

## Modifications externes et index {#monitoring}

Les documents ouverts sans modification rechargent les changements détectés sur
disque ; les buffers modifiés ou en conflit gardent leur texte. Enregistrer refuse
un état disque incompatible. Examinez le changement puis rechargez/abandonnez
explicitement si nécessaire. La surveillance est distincte des points de reprise
de texte de [Récupération](doc:recovery#recover).

L’index respecte les exclusions et contient au plus 200 000 chemins. Une
actualisation explicite le reconstruit ; les exclusions Git globales peuvent
l’exiger. Les clients SDK de [workspace_files](doc:operation-workspace-files-workspace-files)
doivent comparer indexRevision et generation pour assembler des pages.
Les opérations physiques partagent les services aperçu/application/annulation de
[l’API des ressources](doc:api-resources#operations).

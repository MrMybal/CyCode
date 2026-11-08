---
id: update-storage
title: Téléchargements, stockage et historique des mises à jour
description: Reprendre un téléchargement, examiner les données conservées et interpréter les reçus.
section: Travailler dans CyCode
parent: updates
order: 291
sourceRevision: 1
---

## Reprendre un téléchargement {#downloads}

Mises à jour sépare vérification, téléchargement et installation. Un transfert
compatible interrompu peut proposer Reprendre ou un nouveau téléchargement.
La ressource enregistrée et la plage HTTP acceptée doivent correspondre ; le
SHA-256 complet est toujours vérifié avant de proposer l’installation. Un fichier
partiel conservé n’est pas une mise à jour installable. Progression et débit
estimés concernent la phase en cours.

## Examiner le stockage {#storage}

Choisissez explicitement **Examiner le stockage des mises à jour**. Sélectionnez
un ancien téléchargement admissible puis son retrait. Examinez version, dossier,
fichiers et tailles, puis confirmez avoir lu l’action avant de retirer le contenu.
Ce retrait déplace les fichiers suivis vers un stockage conservé : il ne libère
pas d’espace disque. Versions actuelles/récentes, sauvegardes de récupération,
fichiers inconnus et anciens caches non pris en charge restent protégés.

Examinez de nouveau puis préparez une restauration de cache pour remettre les
fichiers. Une destination occupée ou un contenu modifié bloque la restauration.
Gardez le reçu après un résultat partiel/annulé et examinez-le de nouveau.
Il n’y a ni suppression définitive ni expiration automatique. Ces actions concernent
les installations portables Windows x64 gérées ; les dossiers de développement sont refusés.

## Lire les résultats historiques {#history}

**Examiner l’historique détaillé** lit les métadonnées conservées sans installer,
restaurer ou revérifier les fichiers. Filtrez texte/catégorie et parcourez les pages.
Sélectionnez une ligne pour les nombres préparés/terminés, les étapes et le résultat
rapporté. Les anciens enregistrements sans date indiquent Date indisponible ;
une date enregistrée ne prouve pas une chronologie fiable.

L’historique décrit une ancienne tentative, pas les empreintes actuelles de
l’installation ni l’admissibilité à une restauration. Attente, échec, échec de
redémarrage et annulation demandent un examen distinct. Le redémarrage peut échouer
après une restauration réussie. Pour une installation bloquée, consultez
[la récupération](doc:update-recovery#recover).

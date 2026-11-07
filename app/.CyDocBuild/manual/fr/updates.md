---
id: updates
title: Mettre à jour l’application
description: Consultez les versions, téléchargez une mise à jour et choisissez son installation.
section: Bien démarrer
order: 290
parent: overview
sourceRevision: 3
---

## Vérifier, télécharger et installer {#updates}

Ouvrez les mises à jour de l’application depuis le menu Affichage. La vérification au démarrage est réglable dans les options. Une nouvelle version stable GitHub propose de reporter ou d’ignorer cette version ; téléchargement et installation restent distincts.

1. Recherchez une version et lisez ses notes.
2. Téléchargez ou reprenez la mise à jour compatible. L’application vérifie son SHA-256 avant d’autoriser l’installation.
3. Choisissez l’installation et le redémarrage lorsque vous êtes prêt. Enregistrez ou abandonnez les documents dans les demandes de fermeture habituelles ; Annuler interrompt l’installation.
4. Fermez les autres instances utilisant cette installation. L’assistant attend la libération de sa propriété plutôt que de tuer ces processus.

Un téléchargement interrompu peut proposer une reprise ou un téléchargement neuf. Conservez les sauvegardes de récupération tant que l’installation n’est pas réglée. Le retrait de stockage déplace les données conservées et ne libère pas immédiatement leur espace. L’historique détaillé est indicatif ; il ne prouve pas que les fichiers installés correspondent toujours à un ancien résultat.

La vérification anonyme ne peut pas lire une version GitHub privée. Sans paquet compatible accessible, utilisez manuellement la page de version et l’archive fournie. Une erreur de connexion n’est pas présentée comme une application à jour.


## Installation Windows {#windows-installation}

Utilisez l’installateur Windows par utilisateur ou le ZIP portable complet. Les deux incluent le runtime MSVC local et le manuel hors ligne. Les données restent dans data/ de l’installation accessible en écriture et sont conservées à la désinstallation. Depuis 0.2.3 les copies installées utilisent Setup vérifié et les copies portables utilisent le même ZIP pour leurs mises à jour, sans archive de mise à jour séparée. Les versions jusqu’à 0.2.2 nécessitent une première installation manuelle. La vérification au démarrage est active par défaut ; les préférences existantes restent appliquées.

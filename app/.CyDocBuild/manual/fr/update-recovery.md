---
id: update-recovery
title: Récupérer une mise à jour interrompue
description: Suivez les instructions conservées pour une installation portable.
section: Travailler dans CyCode
parent: updates
order: 317
sourceRevision: 1
---

## Trouver la récupération {#recover}

Une interruption du remplacement peut laisser `data/updates/pending.json` et bloquer le démarrage normal. Conservez ce marqueur et les fichiers de la transaction. Ouvrez son `RECOVERY.txt` : il identifie la copie préparée de `CyCodeUpdater.exe` à utiliser. L’IDE complet peut être indisponible tant que ses fichiers gérés sont incomplets.

Lancez explicitement cet assistant préparé. Il contrôle son identité et la transaction conservée, puis présente les versions, les fichiers concernés, les conflits et l’estimation de stockage. Fermez les autres instances utilisant cette installation. Lisez la question de restauration ; Non est le choix par défaut. Des conflits ou une capacité insuffisante ou inconnue peuvent bloquer la confirmation.

## Restaurer et redémarrer {#restore}

Pendant la préparation et la restauration, suivez les phases et compteurs de fichiers. Une demande d’arrêt est coopérative ; attendez le résultat annoncé. Conservez les fichiers de récupération après un résultat interrompu ou incertain. Ne supprimez pas manuellement des fichiers gérés pour forcer la progression.

Une restauration terminée propose séparément le redémarrage, avec Non sélectionné. Un échec de redémarrage peut survenir après une restauration réussie ; lisez ces deux résultats séparément. Des fichiers inconnus ou modifiés localement peuvent demander un examen manuel. Cette récupération concerne les transactions portables Windows de format 2 prises en charge, pas les anciens installateurs arbitraires ni toute installation endommagée.

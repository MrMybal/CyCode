---
id: mcp
title: Connecter un client MCP
description: Connectez-vous à l’IDE actif avec une identité de lecture ou d’édition.
section: Travailler dans CyCode
order: 260
parent: plugins
sourceRevision: 2
---

## Se connecter {#connection}

CyCode utilise le véritable compagnon CyToolsCore. Un SDK et un runtime privés compatibles doivent être installés localement ; ils ne sont pas inclus dans l’archive publique de l’application. Ouvrez le panneau MCP et copiez dans votre client la configuration de connexion de cette instance active de l’IDE.

La configuration par défaut est en lecture seule. Sélectionnez volontairement l’accès en édition pour modifier les documents ou le contrôle de versions, exécuter des processus ou appeler des plugins. Chaque instance possède son propre contexte de connexion. Ne partagez pas les fichiers privés d’identités ou de points de connexion.

## Tâches et documents {#operations}

L’adaptateur MCP officiel expose `op_<operationId>`, `cy_get_job`, `cy_events` et `cy_cancel_job`. La soumission d’une tâche n’est pas son résultat final : attendez Completed, Failed ou Cancelled. Pour un résultat de processus, examinez aussi `exitCode`, `timedOut` et `truncated`.

Les lectures et écritures utilisent les mêmes documents natifs que l’éditeur. Relisez une révision courante avant de remplacer le texte, puis enregistrez explicitement. Les sources de l’espace de travail sont des références persistantes, pas des artefacts temporaires de tâches. La [référence publique des opérations](doc:api-links#contract) décrit les paramètres et résultats.

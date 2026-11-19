---
id: mcp
title: Connecter un client MCP
description: Connectez-vous à l’IDE actif avec une identité de lecture ou d’édition.
section: Travailler dans CyCode
order: 260
parent: plugins
sourceRevision: 8
---



## Se connecter {#connection}

Le Core 0.1.5 utilise CyMCPSDK 0.7.0 natif. Le Manager configure une connexion
liée à un projet et en lecture seule. Le lancement manuel utilise
`--sdk --workspace <projet> --connection-state <etat-prive> --disable-plugins mcp`.
Aucun serveur MCP Python ni tchat intégré n’est requis. `--native-descriptor FICHIER`
réutilise un Engine ouvert ; `--bridge-config FICHIER --access read|edit mcp`
rejoint son service HTTP privé. Ne publiez pas les jetons de connexion.

## Opérations et résultats {#operations}

Les cinq outils MCP découvrent et appellent les 257 opérations canoniques.
`cy_tools_search` recherche une capacité, `cy_tools_describe` charge son contrat,
puis `cy_tools_call` reçoit par exemple
`{"name":"documents_read","arguments":{"path":"Source/main.cpp"}}`.
Les gros résultats sont récupérables intégralement par ressources privées et
morceaux. Les droits de l’hôte et outils installés gouvernent les actions.
Relisez les révisions, prévisualisez/appliquez les transactions et enregistrez explicitement.

Les mutations et tâches explicites gardent leurs résultats au redémarrage.
Une action interrompue n’est jamais rejouée automatiquement. Les profils
historiques `code`/`full` conservent `op_<id>`, `cy_get_job`, `cy_cancel_job`
et leurs enveloppes de jobs.

## Core sans interface {#headless}

Consultez [l’intégration native](native-automation#connect), [les workflows IA](core-agents#connect)
et [les contrats publics](api-links#contract) pour les schémas, droits, index du
projet, prérequis sémantiques, persistance et réglages indépendants.

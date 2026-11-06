---
id: api-links
title: Référence publique SDK et MCP
description: Retrouvez les signatures, schémas et règles d’édition partagées.
section: Travailler dans CyCode
order: 270
parent: plugins
sourceRevision: 2
---

## Contrat des opérations {#contract}

Les sous-pages de services suivantes recensent les opérations réellement déclarées par CyCode. Chaque opération précise son identifiant, ses paramètres et les schémas d’entrée et de sortie. Ces identifiants ne sont pas des noms de fonctions C++ directement appelables. Utilisez CyToolsCore ou l’adaptateur MCP ; les plugins natifs utilisent les méthodes annoncées par l’hôte.

Les paramètres facultatifs portent `?`. `expectedRevision` doit provenir d’une lecture courante du document ; les identifiants d’aperçu, de session ou de plan doivent provenir de la requête native correspondante. Ne fabriquez pas ces valeurs et ne les réutilisez pas après un changement de contexte source.

Par exemple, choisissez l’outil MCP `op_documents_read` avec ces arguments pour un fichier existant dans votre espace de travail :

```json
{"parameters":{"path":"Source/main.cpp"}}
```

Lisez la révision réellement retournée avant un appel ultérieur à `op_documents_replace`. L’[opération de document](doc:operation-documents-read-documents-read#signature) présente le schéma complet du résultat. Les tableaux et schémas décrivent l’entrée de l’opération ; les outils MCP officiels placent cette entrée dans `parameters`, comme ci-dessus. Il s’agit des arguments de l’outil, pas d’une enveloppe de transport complète.

## Examiner, appliquer et enregistrer {#transactions}

1. Consultez l’état courant du document, du dépôt ou du processus.
2. Demandez un aperçu ou un plan si l’opération le prévoit.
3. Appliquez avec l’identité et les protections retournées, après les confirmations requises.
4. Examinez le résultat réel. Enregistrez explicitement les documents modifiés en mémoire.

```diagram
{"type":"flow","title":"Transaction de document","nodes":[{"id":"read","label":"Lecture et révision","x":0,"y":0},{"id":"preview","label":"Examen de l’aperçu","x":220,"y":0},{"id":"apply","label":"Application en mémoire","x":440,"y":0},{"id":"save","label":"Enregistrement sur disque","x":660,"y":0}],"edges":[{"from":"read","to":"preview"},{"from":"preview","to":"apply"},{"from":"apply","to":"save"}]}
```

`changes_undo` concerne les transactions d’éditeur natives prises en charge ; les commits Git, les processus et les écritures de mémoire du débogueur sont d’autres opérations. Une réussite du transport ou de la tâche ne garantit pas un code de sortie de processus nul. Consultez la [connexion MCP](doc:mcp#operations) et l’[ABI des plugins](doc:plugin-api#contract).

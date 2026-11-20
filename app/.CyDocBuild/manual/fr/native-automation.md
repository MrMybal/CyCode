---
id: native-automation
title: CLI et MCP natifs
description: CyMCPSDK, recherche indexée, tâches durables et ressources privées.
section: API et extensions
parent: core-agents
order: 264
sourceRevision: 3
---

## Connexion native {#connect}

Les sources actuelles utilisent **CyMCPSDK 0.7.2**, révision
`e562c731a146d599e3404718ea8414bb5b96b66d`. Seuls Core, Platform, Http et MCP sont
compilés. AI, Local et ChatUI sont désactivés. Le Core reste sans interface,
sans tchat et sans serveur MCP Python. La distribution autonome 0.1.6 inclut les quatre profils d’accès et choisit Free par défaut.

```text
cycode_cli --sdk --workspace <projet> --connection-state <dossier-prive> --disable-plugins mcp
cycode_cli --sdk --workspace <projet> --data-dir <etat> describe-operation search_text
cycode_cli --sdk --workspace <projet> --data-dir <etat> invoke search_text --params-file query.json
```

`query.json` : `{"query":"answer","engine":"index","waitMs":20000}`. Cette opération
utilise l’index persistant du Core, pas un parcours du projet par le ripgrep du SDK.
`--connection-state` isole les tâches de chaque connexion et partage l’index du
projet ; `CYCODE_INDEX_DIR` explicite reste prioritaire. Les anciennes commandes
natives et Python restent compatibles. Les nouvelles configurations du Manager
lancent directement le binaire natif.

## Découverte et droits {#operations}

Le profil compact annonce cinq outils : `cy_tools_search`, `cy_tools_describe`,
`cy_tools_call`, `cy_resources_read`, `cy_resources_grep`. Conservez les schémas
chargés et appelez une opération réelle ainsi :

```json
{"name":"documents_read","arguments":{"path":"Source/main.cpp"}}
```

`direct-full` expose les noms canoniques. `code`/`full` conservent les anciens
`op_<id>` et contrôles de tâches. Free est le défaut ; les profils contrôlés
appliquent les droits du lanceur à l’édition, VCS, processus, plugins et paramètres.
Relisez les révisions, prévisualisez/appliquez les transactions et enregistrez
uniquement sur demande. `--native-descriptor FICHIER` agit sur les documents non
enregistrés et l’Undo/Redo de l’Engine existant. `serve --service-descriptor FICHIER`
publie des connexions HTTP privées de lecture/édition ;
`--bridge-config FICHIER --access read|edit mcp` s’y connecte. Protégé et Jeton seul exigent un jeton ; Free demande uniquement l’adresse.

## Profils d’accès local {#access-profiles}

Les sources choisissent **Free** lorsqu’aucun accès n’est sélectionné. L’adresse locale `/mcp` suffit au client HTTP pour appeler toutes les opérations enregistrées sans autre confirmation. Arguments, révisions, chemins du projet et racines d’import/export configurées restent validés.

| Profil | Jeton HTTP | Politique des opérations |
|---|---|---|
| Free (par défaut) | Aucun | Toutes les opérations enregistrées |
| Local contrôlé | Aucun | Droits de l’hôte |
| Jeton seul | Requis | Toutes les opérations enregistrées |
| Protégé | Requis | Droits de l’hôte |

Choisir `--local-access-profile free|local-controlled|token-only|protected` ou `CYCODE_MCP_ACCESS_PROFILE`. Le serveur écoute sur `127.0.0.1`. Les profils explicites et anciens droits lecture/édition sont conservés ; les configurations sans choix migrent vers Free. Le profil du serveur est enregistré dans son `local-access.json` privé et repris au redémarrage. Les arguments et variables explicites sont prioritaires. Le jeton concerne HTTP ; stdio utilise déjà l’identité locale de l’hôte. Setup et le ZIP portable 0.1.6 incluent cette mise à jour ; 0.1.5 a introduit l’intégration native initiale.

## Résultats et optimisations {#results}

Les mutations possèdent une tâche durable. Découvrez `core_job_submit/get/list/cancel`
par la passerelle. Les tâches en attente/en cours interrompues deviennent échouées
au redémarrage et ne sont jamais rejouées. L’import unique par le script Python
standard conserve anciennes bases SQLite, identités et chemins d’artefacts ;
le paquet installé lance cette conversion si nécessaire. Arrêtez l’ancien propriétaire.

Les gros résultats disposent de références privées lisibles intégralement par
morceaux. Une expiration ne rejoue jamais une action : récupérez sa tâche durable
ou relancez explicitement une lecture. Des options indépendantes règlent découverte,
seuil d’inclusion, quotas mémoire/disque, stockage temporaire, durées des résultats
et du cache, notifications et mesures JSONL sans contenu sensible. Le grep des
ressources ne parcourt pas les dossiers du projet. Les événements natifs mettent
à jour la ressource optionnelle `cycode://workspace/changes` ; les anciens clients
utilisent les durées de cache et le rafraîchissement explicite.

Le schéma projeté du SDK omet les contraintes pattern/dependency/contains ; le
Core vérifie toujours le contrat canonique complet. Lisez
`cycode://contracts/<id>` ou `core_contract`. Les détails d’erreur natifs restent
dans le message MCP encodé en JSON et dans la tâche durable. Voir
[les contrats complets](api-links#contract).

## Compilation et distribution {#build}

Compilez les sources propres et épinglées avec
`python app/scripts/build.py --core-only --no-install --cymcp-source <source>`.
`python app/scripts/prepare_manager_runtime.py` prépare uniquement CPython et sa
bibliothèque standard pour le Manager et la compatibilité, sans roue SDK ni
site-packages. Le Core/MCP n’a pas besoin de Python. Les notices MIT du SDK et
des composants liés sont distribuées. Windows x64 est validé ; les exécutions
Linux/macOS restent à faire.

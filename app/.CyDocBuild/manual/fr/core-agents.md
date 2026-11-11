---
id: core-agents
title: Utiliser le Core depuis les IA
description: Core persistant, contrats MCP complets et modifications protégées.
section: API et extensions
parent: mcp
order: 261
sourceRevision: 3
---

## Se connecter {#connect}

Installez la distribution autonome et le skill IA avec [le guide d’installation](core-installation#distribution).

CyCode Core expose le même Engine natif persistant aux applications et aux IA.
Les 238 opérations métier deviennent des outils `op_<operationId>` grâce au
véritable adaptateur MCP de CyToolsCore. Le SDK ajoute ses contrôles de tâches,
de découverte et d’identités. Une opération déclarée ne prouve pas l’installation
d’un moteur de langage, compilateur, débogueur, fournisseur VCS ou plugin.

Depuis les sources avec l’environnement SDK privé déjà préparé :

```sh
python -B -X utf8 app/tool.py --core-workspace <dossier-projet> --core-executable <cycode_cli-fiable> --disable-native-plugins --data-dir <dossier-jobs-prive> mcp
```

Utilisez l’interpréteur isolé préparé par `app/scripts/setup_runtime.py` ; la
distribution Windows du bureau utilise son interpréteur embarqué. Cette commande
lance un Core de production sans interface, le conserve entre les requêtes et
l’arrête proprement à la fermeture MCP. Elle ne télécharge aucun binaire.
`--core-executable` est facultatif si le CLI compilé se trouve à côté de
l’application ou dans son dossier de build runtime. `CYCODE_CORE_EXECUTABLE`
permet aussi un choix explicite par l’hôte. La racine du projet est fixée par
l’hôte de confiance, jamais par un paramètre d’opération envoyé par l’IA.

Le MCP autonome est en lecture seule par défaut. L’hôte peut fournir une liste
explicite dans `CYCODE_PERMISSIONS`, par exemple `workspace.write,process.execute`.
Les écritures VCS, plugins natifs et paramètres utilisateur possèdent leurs
propres autorisations : consultez le champ `permissions` de chaque opération.
Désactivez les plugins natifs si seuls les services intégrés sont nécessaires.

Pour un IDE actif, copiez sa configuration MCP : `--bridge-config
<descripteur-service-prive> --access read mcp` utilise son véritable service SDK
partagé. Choisissez `edit` uniquement avec autorisation. Un hôte sans interface
peut démarrer ce même service officiel avec `--core-service --service-descriptor
<fichier-prive> --data-dir <dossier-jobs-prive>`, en complément des deux options
de démarrage du Core. Gardez ce processus actif ; l’arrêt de son hôte natif
possédé termine le service. Ne publiez ni descripteur ni jeton d’identité.
Deux runtimes MCP autonomes ne partagent pas leurs jobs simplement parce qu’ils
utilisent le même projet.

### Configuration du client {#configuration}

Pour un client acceptant une configuration stdio `mcpServers`, remplacez les
chemins absolus entre chevrons dans cet exemple en lecture seule. D’autres
clients utilisent un format extérieur différent ; conservez l’exécutable et l’argv.

```json
{
  "mcpServers": {
    "cycode": {
      "command": "<private-python>",
      "args": [
        "-B",
        "-X",
        "utf8",
        "<CyCode>/app/tool.py",
        "--core-workspace",
        "<project-directory>",
        "--core-executable",
        "<trusted-cycode_cli>",
        "--disable-native-plugins",
        "--data-dir",
        "<private-job-directory>",
        "mcp"
      ],
      "env": {
        "CYCODE_PERMISSIONS": ""
      }
    }
  }
}
```

Pour plusieurs agents partageant les mêmes documents natifs et le même
ordonnanceur SDK, démarrez le service une seule fois depuis l’hôte de confiance :

```sh
python -B -X utf8 app/tool.py --core-workspace <dossier-projet> --core-executable <cycode_cli-fiable> --disable-native-plugins --core-service --service-descriptor <descripteur-sdk-prive> --data-dir <dossier-jobs-prive>
```

Chaque client lance ensuite l’interpréteur privé avec `app/tool.py --bridge-config
<descripteur-sdk-prive> --access read mcp`. Un client autorisé à modifier choisit
`--access edit`. Le SDK possède les identités ; aucun client ne reçoit les jobs
privés de l’autre. Arrêtez le service propriétaire avec Ctrl+C pour un arrêt normal.

## Découvrir les opérations {#methods}

Consultez la [référence complète des opérations](doc:api-links#contract).

Consultez `cy_describe`, `cy_list_operations`, `cy_describe_operation` et
`cy_can_run` avant de supposer une capacité ou une permission. Le catalogue
généré `app/MCPMethods.json` contient chaque nom MCP, méthode native, schéma
complet d’entrée/sortie, permission, groupe exclusif et capacité de progression
ou d’annulation, directement issus du contrat exécutable. Régénérez-le avec
`python app/scripts/generate_mcp_reference.py` ; `--check` le vérifie sans écrire.
La référence HTML bilingue regroupe ces mêmes 238 contrats par domaine. Elle
ne décrit pas les fonctions C++ internes et n’expose pas les contrôles natifs
privés d’arrêt ou d’annulation de l’hôte.

| Besoin | Opérations à découvrir |
| --- | --- |
| Contexte et préparation | `service_status`, `analysis_status`, `analysis_prepare`, `language_status` |
| Fichiers et texte | `workspace_files`, `workspace_refresh`, `workspace_include`, `search_text`, `documents_*`, `sources_*` |
| Intelligence du code | `language_definition`, `language_declaration`, `language_type_definition`, `language_references`, `language_symbols`, `language_document_symbols`, `language_complete`, `language_hover`, hiérarchies et diagnostics |
| Modifications | `language_rename`, formatage/actions de code, `changes_preview`, `changes_apply`, `changes_undo` |
| Compilation et exécution | `toolchains_*`, `build_*`, `msbuild_*`, `unreal_*`, `project_*`, `tests_*`, `terminal_*`, `debug_*` |
| Extensions et VCS | `plugins_*`, `packages_*`, `vcs_providers`, `vcs_context`, `vcs_provider_action`, opérations Git typées |
| Récupération et événements | `recovery_*`, `events_read`, `snippets_*` |

Ces groupes servent à la découverte ; les jokers ne sont pas des appels MCP.
Utilisez les noms et schémas exacts du catalogue, avec leurs prérequis et confirmations.

## Suivre les tâches {#jobs}

Un appel métier MCP place les paramètres natifs dans `parameters`. La soumission
retourne un `jobId`. Interrogez `cy_get_job({"jobId":"<id-emis>"})` jusqu’à
Completed, Failed ou Cancelled. Lisez le résultat métier dans `outputs` seulement
après Completed ; après Failed, lisez `error.code` et `error.technicalDetails`.
Pour les processus, contrôlez aussi `exitCode`, `timedOut`, `truncated` et les
champs propres à l’opération.

```json
{"tool":"op_analysis_prepare","arguments":{"parameters":{"paths":["Source/Game.cpp","Source/Game.h"]}}}
```

Ces chemins illustrent la forme d’une requête : choisissez d’abord des fichiers
existants. N’inventez jamais une révision, identité de document, époque du moteur,
id d’aperçu, curseur de continuation, id de confirmation ou id de job.

`analysis_prepare` et le renommage C++ publient leur progression dans le job
SDK. `analysis_status` est un état agrégé natif ; il ne donne pas la propriété
du job d’un autre client. Utilisez `cy_cancel_job` avec votre propre id émis.
L’annulation conserve la réservation native jusqu’à l’arrêt réel de l’opération.
Les tentatives AST lentes peuvent continuer pendant l’indexation ; annulation,
échec du moteur et conflits de révision/configuration restent distincts. Aucun
ordonnanceur ou modèle d’édition Python parallèle n’est ajouté.

Le SDK possède les fichiers temporaires importés ou générés pour ses tâches.
Les sources du projet sont des références natives persistantes : ne les
importez pas avec `cy_import_file`, ne les passez pas comme entrées jetables et
ne les supprimez pas avec `cy_finish_task`. Exportez les artefacts temporaires
souhaités avant de terminer leur tâche. Fermer une tâche MCP n’enregistre ni
n’abandonne les buffers natifs partagés de l’éditeur.

## Préparer et naviguer {#analysis}

Commencez par `service_status`, puis `language_status.servers`. Un service
prêt ne prouve pas une indexation complète. `analysis_prepare` attend les réponses
AST/symboles des vrais compilateurs. Les chemins explicites n’ont plus de plafond
fixe de fichiers ; les limites d’octets du transport et de chaque fichier restent
applicables. Un seul buffer temporaire appartenant à l’analyse est conservé à la
fois. Les documents existants et modifiés de l’éditeur restent ouverts. La
préparation C++ explicite rouvre le brouillon LSP pour obtenir des diagnostics
actuels, sans changer le texte, l’identité ou la révision du buffer natif.

Pour inventorier un grand projet Unreal avec sa base de compilation active :

```json
{"scope":"compilation-database","batchSize":0}
```

Cette demande renvoie les nombres de commandes et d’unités de traduction uniques,
une session et une continuation, sans analyser de source. `pathPrefix` permet de
choisir un dossier existant du projet. Continuez par lots (64 par défaut) :

```json
{"continuation":"<session-émise>","batchSize":64}
```

Répétez tant que `state` vaut `pending`. `completed`, `total` et
`coverage.remaining` décrivent la portée sélectionnée. `diagnosticClean`,
`diagnosticErrors` et `diagnosticUnknown` distinguent des diagnostics du
compilateur liés à la version exacte et une preuve absente. Terminer ce parcours
ne démontre pas la sémantique exhaustive d’Unreal : configurations inactives,
assets, noms créés par macros et limites du compilateur restent hors de cette
affirmation. `wholeProjectIndexCompleteness` reste `unknown`.

Le résultat contient les 128 premières observations. `analysis_results` reçoit
`session`, `offset` et `limit` (256 maximum) ; suivez `nextOffset` tant que
`hasMore`. Les pages contiennent des métadonnées historiques, pas les sources ni
une garantie que la session est encore actuelle. Une modification de document,
du projet observé, du moteur ou de sa base de compilation invalide la reprise
avec `RevisionConflict`. Une annulation conserve les observations terminées :
une nouvelle demande explicite peut reprendre cette session dans le même Core.
Ce point de reprise est en mémoire, remplacé par une nouvelle préparation et
perdu à l’arrêt du Core. Un renommage global annulé recommence son parcours.

La préparation n’active pas de moteur, ne compile pas le projet, n’enregistre
pas les sources et ne certifie pas la complétude de l’index global.

Activez un moteur installé avec `language_configure` et son argv de confiance.
Unreal exige les vraies commandes du compilateur UBT et les en-têtes générés.
`unreal_prepare` est l’adaptateur de projet ; une racine contenant uniquement
les sources du moteur, sans `.uproject`, demande encore une base UBT exportée
séparément et fournie à clangd. La coloration lexicale ou un cache de noms de
fichiers n’est pas de l’analyse sémantique.

`workspace_files` est paginé : conservez `generation` et `indexRevision`, suivez
`nextOffset`, puis recommencez si l’un des deux change. `complete`, `indexing`,
`indexError` et `truncated` décrivent cette liste, pas la complétude de clangd.
La navigation utilise des coordonnées UTF-16 `line`/`character` à partir de zéro
et une `expectedRevision` fraîche de `documents_read`. Conservez les identités
et époques fournies par l’opération. `sources_*` gère les cibles SDK en lecture seule.

Définition, déclaration et définition de type peuvent réutiliser un cache natif
borné. Les révisions et le contexte des documents, l’époque du moteur, les
changements observés du projet, le contexte de la base de compilation et les
empreintes disque des cibles protègent cette réutilisation. `refresh:true` la
contourne ; `cacheHit` signale un résultat effectivement réutilisé. Références
et implémentations ne sont pas mises en cache, car leurs résultats peuvent
augmenter pendant l’indexation. Ce cache ne prouve pas la complétude de l’index.


Le lancement C++ intégré active les en-têtes en mémoire de clangd par défaut
(clangd 14+ ; vérification locale avec 18.1.8). L’option explicite
`--use-dirty-headers=false` permet de désactiver ce comportement. Pour une
définition/déclaration vers une cible C++ déjà ouverte, le Core peut réconcilier
une position ancienne de l’index avec l’AST actuel du fichier cible, grâce à
l’identité USR exacte du compilateur. `astReconciled:true` signale une correction.
Une correspondance ambiguë est comptée comme invalide, sans supposition. Ce
traitement couvre aussi la restauration du texte initial d’un en-tête modifié.
Aucun analyseur parallèle ni remplacement textuel de symbole n’est utilisé.

## Modifier avec une transaction {#refactor}

1. Lisez le document courant et sa révision exacte. Soumettez `language_rename`
   sur le symbole résolu par le compilateur, avec le nouveau nom souhaité.
2. Examinez tous les fichiers, `coverage`, annotations et `changes_preview`.
   Le C++ parcourt tous les candidats textuels avec des buffers temporaires.
   L’identité USR du compilateur confirme le symbole dans chaque fichier utile ;
   seules les modifications produites par le compilateur pour ce fichier sont
   réunies. Un paramètre local confirmé évite le parcours global.
3. Le plafond de 128 candidats et celui de 100 fichiers par transaction sont
   supprimés. Le lancement clangd intégré fournit `--rename-file-limit=0`, sauf
   option explicite contraire. Une identité ambiguë, des diagnostics actuels
   absents ou contenant des erreurs et une réponse non prise en charge refusent
   tout aperçu. Le budget mémoire de transaction reste à 16 Mio ; un dépassement
   ne produit jamais un plan partiel. Collage de tokens, noms sérialisés
   Blueprint/réflexion, branches de plateformes/configurations inactives,
   projets externes et références non signalées restent des limites.
4. Appliquez le plan émis avec `changes_apply` et tous les ids requis dans
   `confirmedAnnotations`. Révisions, bases disque et portée globale préparée
   sont revérifiées avant la première modification. Une portée différente
   rejette tout le plan. Examinez le résultat avant l’action suivante.
5. `changes_undo` restaure la transaction entière si ses buffers et bases disque
   correspondent encore. Apply et Undo n’enregistrent aucun fichier. Utilisez
   volontairement `documents_save` avec la révision courante lorsque c’est autorisé.

Le cas réel Unreal `FDateTime::ParseHttpDate` couvre implémentation, déclaration
d’en-tête et appel HTTP dès la première demande, sans ouverture manuelle des
deux derniers fichiers. Ce cas ne prouve pas une analyse exhaustive des
37 557 unités de traduction Unreal.

## Erreurs et limites des capacités {#errors}

`RevisionConflict` impose une nouvelle lecture et un nouveau plan ; ne rejouez
pas aveuglément une modification périmée. `Cancelled` est une annulation finale
du propriétaire. `DeadlineExceeded` décrit une tentative limitée du moteur ;
la préparation sémantique peut continuer avec progression. `BackendUnavailable`
demande de configurer ou réparer le moteur. `CapacityExceeded` demande une portée
prise en charge ou une évolution de capacité, jamais de traiter un résultat
partiel comme complet. Les codes/détails natifs traversent le SDK. InvalidOutput
est une erreur de contrat du pont, pas un succès natif à ignorer.

Vérifiez capacités annoncées et outils installés pour LSP/DAP, Unreal, compilation,
plugins natifs et fournisseurs autres que Git. Un descripteur ne rend pas
automatiquement une fonction exécutable. Ces changements sont vérifiés localement
sous Windows avec le Core/clangd de production et Unreal réel. L’exécution
Linux/macOS, les nouveaux installateurs et l’acceptation graphique demandent
des contrôles distincts. GitHub Actions reste suspendu par le propriétaire.

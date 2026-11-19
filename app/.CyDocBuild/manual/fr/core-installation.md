---
id: core-installation
title: Installer le Core autonome
description: Paquets locaux, skill IA et connexion MCP par projet.
section: API et extensions
parent: mcp
order: 262
sourceRevision: 10
---



## Distribution {#distribution}

CyCodeCore 0.1.5 est un service natif sans interface avec **CyMCPSDK 0.7.0**.
La version du paquet est indépendante du contrat historique 0.2.3 affiché par
`cycode_cli --version`. Le Core et son MCP ne nécessitent ni fenêtre IDE, ni
serveur Python, ni modèle GPU, ni tchat intégré.

Setup et le ZIP portable contiennent le CLI natif, le Manager CyUI, les DLL MSVC,
ripgrep, le skill IA, les schémas exacts et le lecteur CyDoc hors ligne EN/FR.
Seule la bibliothèque standard Python est incluse pour le Manager et les anciens
scripts. Les nouvelles connexions MCP lancent directement le CLI ; la passerelle
compacte atteint les 257 opérations. Résultats et anciennes identités sont conservés
par la migration documentée ; une action interrompue n’est jamais rejouée automatiquement.

Compilateurs, serveurs de langage, adaptateurs de débogage et clients VCS restent
externes. Le Manager propose l’installation privée de clangd à côté des versions
du Core, sans modifier PATH ni LLVM. Les paquets Windows x64 sont des préversions
non signées. L’exécution et le packaging Linux/macOS restent à valider.

### Anciennes distributions {#history}

La version 0.1.1 utilise des noms temporaires indépendants pour les sauvegardes
atomiques. Le CLI de production passe les recherches MCP et la navigation clangd
sur CyCodeIDE, ainsi que les cycles de sauvegarde/récupération et les contrôles de
révision, avec Kaspersky actif. Ces observations Windows ciblées ne constituent
ni une certification antivirus ni une validation exhaustive d’Unreal. Les paquets
ne sont pas signés.

La version 0.1.2 distribue le catalogue actuel de 252 opérations, Undo/Redo natif
commun et reçus de commit, texte/récupération paginés et bibliothèque de mise à
jour configurable par consommateur. Elle provient de sa sourceRevision exacte ;
Setup/ZIP contiennent le même Python/MCP vérifié, ripgrep, licences et manuel local.
Compilation de production et intégrité des paquets sont contrôlées sans lancer
Core, son hôte MCP ni Setup. Aucun nouvel essai réel d’historique/installation
n’est revendiqué ; les observations ciblées précédentes gardent leur provenance.
Une installation ou un processus 0.1.1 reste inchangé avant mise à niveau/reconnexion.

La version 0.1.3 valide chaque message reçu du serveur de langage, partage
un index de lignes et assemble les éditions de texte en un seul passage. Les
opérations Git partagent leurs options de sécurité : status désactive le programme
fsmonitor du dépôt, et diff/show désactivent la conversion externe du texte. Le
catalogue de 252 opérations et le contrat du moteur sont conservés. Compilation
et contrôles des paquets/données ne valent pas validation native ou d'installation.

[Télécharger Setup ou le ZIP Windows](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.5).
Setup propose le raccourci CyCodeCore Manager. La version 0.1.4 ajoute les profils
IA et la sélection vérifiée de nouveaux moteurs sans écraser les sessions actives.

## Connecter un projet {#connect}

Ouvrir **CyCodeCore Manager > Agents et skills**. Choisir les applications, la
portée personnelle/projet, le skill et/ou MCP, puis **Voir les modifications** et
**Appliquer**. Un skill personnel seul ne demande aucun projet ; le MCP reste lié
à un projet existant. La lecture seule est proposée par défaut. L’édition et les
processus de compilation sont explicites ; VCS/plugins/paramètres restent séparés
et les plugins natifs sont désactivés.

Le [guide du gestionnaire](doc:core-manager#agents) détaille les profils et formats.
Les autres paramètres, skills modifiés et connexions non gérées sont conservés.
Les reçus et sauvegardes restent dans les données locales privées de l’utilisateur.
Recharger et approuver la connexion dans le client : une configuration enregistrée
ne prouve pas que le serveur, le skill ou une requête compilateur fonctionne.

Le script Codex historique reste utilisable depuis le moteur sélectionné :

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <dossier-projet> --access edit --enable
```

Il conserve `.codex/config.toml`, ses blocs gérés, les protections du skill et
`agent-state`. Sans `--enable`, la connexion reste désactivée. Les options
`--skill-only`, `--status`, `--python`, `--skill-home`, `--config` et `--state-dir`
restent disponibles. L’enregistrement ne lance pas le Core et n’installe pas de compilateur.

Deux hôtes autonomes ne partagent pas leurs buffers non enregistrés. Utiliser le
service SDK privé de l’IDE pour ses documents en mémoire ; voir [Agents](doc:core-agents#connect).

## Mettre à jour une installation active {#versioned-installation}

Setup installe dans `versions/<version>-<génération>`, vérifie l’inventaire complet
puis sélectionne atomiquement `current.json`. Un ancien dossier de version choisi
comme destination est ramené à sa racine. Les anciens moteurs, Python/MCP et le
gestionnaire restent intacts ; aucun processus n’est arrêté de force. Réinstaller
la même version crée aussi une nouvelle génération. Une activation échouée
conserve la sélection précédente. Voir [les mises à jour](doc:core-manager#updates).

Le gestionnaire peut actualiser les connexions inchangées dont il possède le reçu,
y compris les anciens reçus Codex. Permissions, état désactivé et skills personnels
modifiés sont conservés. Les conflits de configuration sont distingués du succès
de l’installation. Chaque lancement de MCP possède ses propres données SDK.

Enregistrer ou créer un checkpoint des modifications avant de recharger le client
MCP. L’ancien processus conserve son binaire et ses buffers tant qu’il fonctionne.
Sources, moteur sélectionné, configuration et processus actif sont des états distincts.

## Gestionnaire et vérification des mises à jour {#updates}

CyCodeCore Manager remplace CyCodeUpdater. Il réunit agents/skills et mises à jour,
vérifie GitHub une fois à l’ouverture et sur demande, affiche les notes et propose
le Setup vérifié. La tâche de vérification quotidienne au démarrage de Windows
reste décochée par défaut. Aucun service de surveillance permanent n’est installé.

Seules les versions `core-v` complètes avec Setup et empreintes sont retenues.
Taille, SHA-256 et digest GitHub sont contrôlés. Le gestionnaire attend l’assistant
visible puis vérifie la sélection réelle de la version demandée avant d’annoncer
un succès. Il peut actualiser les connexions gérées puis rouvrir sa nouvelle version.

Pour un dépôt privé, l’authentification Git non interactive existante ou les variables
CYCODE_GITHUB_TOKEN / GH_TOKEN / GITHUB_TOKEN sont utilisées. Aucun identifiant
n’est distribué ou enregistré. Une erreur d’accès/réseau n’est jamais présentée
comme une version à jour. Le canal preview ne certifie pas la complétude du Core.

Le bouton de langue permet EN/FR. `--check` produit du JSON, `--list` lit les
connexions et `--background` propose les nouveautés au plus quotidiennement.
Dans le ZIP, ouvrir `CyCodeManager.cmd`. L’ancien point d’entrée
`apps/CyCodeUpdater/main.py` délègue à la vue Mises à jour du gestionnaire.

## Construire les paquets localement {#package}

Compilez le Core séparément avec les tests désactivés. Le packaging utilise un
binaire de production existant et accepté, sans l'exécuter :

```text
python app/scripts/package_core.py --revision <commit-source-propre-exact> --build <dossier-build> --executable <dossier-build>/app/Release/cycode_cli.exe --manager-bundle <runtime-verifie> --output <nouveau-dossier-sortie> --installer
```

Le runtime suit `app/manager-runtime.lock.json` et son vérificateur commun. Inno Setup
6.7+ est nécessaire pour `--installer`. Aucun exécutable graphique ni ancien
updater n'est nécessaire. Le ZIP et Setup ont une identité CyCodeCore distincte
des anciennes versions de l'application. `SHA256SUMS.txt` accompagne les fichiers.

La préparation sans moteur doit être explicite :

```text
python app/scripts/package_core.py --revision <commit-source-exact> --manager-bundle <runtime-verifie> --output <nouveau-dossier-sortie> --agents-only
```

Le manifeste indique `state: agents-only` ; `--installer` est refusé dans ce mode.
Un fichier absent ou modifié provoque un échec sans reconstruction, substitution
ni téléchargement. Seuls les paquets complets et acceptés peuvent être publiés
comme version exécutable. Les fichiers de préparation peuvent accompagner un
brouillon clairement identifié pendant le blocage. Ne distribuez jamais caches
de projets, identifiants, travaux privés ou paramètres locaux.

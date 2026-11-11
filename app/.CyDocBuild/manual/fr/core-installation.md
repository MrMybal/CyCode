---
id: core-installation
title: Installer le Core autonome
description: Paquets locaux, skill IA et connexion MCP par projet.
section: API et extensions
parent: mcp
order: 262
sourceRevision: 4
---

## Distribution {#distribution}

CyCodeCore fournit le moteur natif sans interface et le pont MCP officiel de
CyToolsCore. Aucun IDE, CyUI/SDL/OpenGL, modèle GPU ou Python système n’est requis.
La distribution autonome est en version candidate locale 0.1.2 ; le contrat du moteur conserve
sa version historique 0.2.3. Il s’agit d’une préversion de développement.

Le paquet Windows x64 contient `cycode_cli.exe`, les DLL MSVC, ripgrep vérifié,
CPython/CyToolsCore/MCP isolés, les licences, les méthodes et le skill
`cycode-core`. Compilateurs, clangd, outils de build et autres serveurs de langage
restent des prérequis externes. L’installateur par utilisateur utilise la même
charge utile et permet de connecter un projet. Aucun workflow GitHub ne construit
ces paquets.

La version 0.1.1 utilise des noms temporaires indépendants pour les sauvegardes
atomiques. Le CLI de production passe les recherches MCP et la navigation clangd
sur CyCodeIDE, ainsi que les cycles de sauvegarde/récupération et les contrôles de
révision, avec Kaspersky actif. Ces observations Windows ciblées ne constituent
ni une certification antivirus ni une validation exhaustive d’Unreal. Les paquets
ne sont pas signés.

[Télécharger Setup ou le ZIP Windows](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1).
Setup propose CyCodeUpdater comme composant séparé et optionnel.

## Connecter un projet {#connect}

Depuis la racine d'une distribution **complète** :

```text
tools/python/python.exe -B app/scripts/install_core_agents.py --workspace <dossier-projet> --access edit --enable
```

Le script copie le skill dans le répertoire de skills Codex de l'utilisateur et
enregistre `cycode_core` dans `.codex/config.toml` du projet. Faites confiance à ce
projet dans le client puis rechargez sa configuration MCP. Les autres clients
peuvent reprendre la commande stdio décrite dans [le guide IA](core-agents#configuration).

Sans `--enable`, le serveur reste désactivé : cela permet de préparer un paquet
sans moteur. `--skill-only` installe uniquement les instructions. `--status
--workspace <dossier-projet>` consulte l'état sans démarrer de programme.
`--python` et `--skill-home` permettent de choisir explicitement un runtime
compatible et le dossier des skills. `--config <configuration-client.toml>` permet de choisir explicitement la configuration utilisateur pour ce projet fixe. Le runtime redistribuable et Setup ciblent
actuellement Windows x64 ; les paquets Linux/macOS demandent une validation distincte.

L'accès lecture est le défaut. `--access edit` autorise `workspace.write` et
`process.execute`, avec les écritures VCS/plugins/paramètres séparées. Les plugins
natifs sont désactivés pour cet hôte. Le script ne lance pas le Core, n'installe
pas de compilateur et ne modifie pas l'antivirus. Une fois activé, le client MCP
peut démarrer le service. Une configuration enregistrée ne prouve pas sa connexion.

Les autres réglages du client sont conservés. Seul le bloc identifié est mis à
jour ; les modifications locales du bloc ou du skill sont préservées pour examen.
Sauvegardes, reçus et travaux restent dans `agent-state` de l'installation, hors
du code source : ne publiez ni ces données ni les descripteurs d'accès privés.
La désinstallation conserve les skills personnels, réglages de projet et travaux.
Désactivez/supprimez la connexion du projet avant de retirer l'installation.

Un hôte autonome possède ses propres buffers. Pour accéder aux documents non
enregistrés de l'IDE, utilisez son service SDK partagé privé, décrit dans
[le guide de connexion](core-agents#connect), plutôt que deux moteurs indépendants.

## Mettre à jour une installation active {#versioned-installation}

Le Setup reste le paquet d’installation habituel. Pour une mise à jour de
développement pendant qu’un Core fonctionne, extrayez le paquet portable vérifié
dans un nouveau dossier versionné en conservant l’installation active. Configurez
son `--home` et son Python privé, avec
`--state-dir <ancienne-installation>/agent-state` pour réutiliser le reçu de
propriété de la connexion. Le nouveau dossier reçoit son propre reçu et son
répertoire de jobs indépendant ; le runtime actif garde ses données. Les
enregistrements suivants peuvent utiliser le nouveau dossier sans --state-dir. Les réglages sans rapport et les skills personnalisés
sont préservés ; le bloc MCP géré doit toujours correspondre à son empreinte.
L’enregistrement n’arrête aucun moteur en cours d’utilisation.

Rechargez la connexion MCP du client pour charger le nouveau catalogue. Le
processus précédent garde son ancien binaire jusqu’à sa déconnexion. Vérifiez
ensuite le catalogue réel et une requête compilateur. Sources, paquet installé,
catalogue du client et processus actif sont des versions distinctes.

## Application de mise à jour optionnelle {#updates}

CyCodeUpdater possède son propre raccourci dans le menu Démarrer. À son ouverture,
il consulte les versions GitHub du Core, affiche les notes et propose le
téléchargement vérifié puis l’installation. Une confirmation est nécessaire ;
fermez les clients Core/MCP avant de continuer. La vérification ne lance pas le
Core et ne remplace pas silencieusement ses fichiers.

Une option distincte, décochée par défaut, permet une vérification à l’ouverture
de Windows. Elle vérifie au plus une fois par jour, propose l’application en cas
de nouvelle version puis quitte. Aucun service permanent n’est installé.

Le programme accepte `--language fr`, `--check` pour une réponse JSON et
`--background` pour la notification quotidienne. Dans le ZIP portable, lancez
`tools/python/pythonw.exe -B apps/CyCodeUpdater/main.py`. Le remplacement manuel
du ZIP reste possible ; la proposition automatique utilise le Setup Windows.

Seules les versions `core-v` disposant du Setup correspondant et de
`SHA256SUMS.txt` sont retenues. Les versions IDE, brouillons et livraisons
incomplètes sont ignorés. Taille, SHA-256 et empreinte GitHub, lorsqu’elle est
fournie, sont vérifiées. Pour un dépôt privé, l’outil utilise l’authentification
GitHub Git existante, sans interaction, ou CYCODE_GITHUB_TOKEN / GH_TOKEN /
GITHUB_TOKEN. Aucun identifiant n’est distribué ou enregistré. Une erreur réseau
ou d’accès conserve la version installée. Le canal preview inclut les préversions
du Core, sans garantir leur complétude.

## Construire les paquets localement {#package}

Compilez le Core séparément avec les tests désactivés. Le packaging utilise un
binaire de production existant et accepté, sans l'exécuter :

```text
python app/scripts/package_core.py --revision <commit-source-propre-exact> --build <dossier-build> --executable <dossier-build>/app/Release/cycode_cli.exe --mcp-bundle <runtime-verifie> --output <nouveau-dossier-sortie> --installer
```

Le runtime suit `app/mcp-runtime.lock.json` et son vérificateur commun. Inno Setup
6.7+ est nécessaire pour `--installer`. Aucun exécutable graphique ni ancien
updater n'est nécessaire. Le ZIP et Setup ont une identité CyCodeCore distincte
des anciennes versions de l'application. `SHA256SUMS.txt` accompagne les fichiers.

La préparation sans moteur doit être explicite :

```text
python app/scripts/package_core.py --revision <commit-source-exact> --mcp-bundle <runtime-verifie> --output <nouveau-dossier-sortie> --agents-only
```

Le manifeste indique `state: agents-only` ; `--installer` est refusé dans ce mode.
Un fichier absent ou modifié provoque un échec sans reconstruction, substitution
ni téléchargement. Seuls les paquets complets et acceptés peuvent être publiés
comme version exécutable. Les fichiers de préparation peuvent accompagner un
brouillon clairement identifié pendant le blocage. Ne distribuez jamais caches
de projets, identifiants, travaux privés ou paramètres locaux.

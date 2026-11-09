---
id: core-installation
title: Installer le Core autonome
description: Paquets locaux, skill IA et connexion MCP par projet.
section: API et extensions
parent: mcp
order: 262
sourceRevision: 2
---

## Distribution {#distribution}

CyCodeCore fournit le moteur natif sans interface et le véritable pont MCP de
CyToolsCore. Il ne nécessite ni IDE, ni CyUI/SDL/OpenGL, ni modèle GPU, ni Python
système. Sa distribution autonome commence à 0.1.0 ; la version historique du
contrat moteur reste 0.2.3. Cette première livraison est une préversion.

Le paquet Windows x64 complet contient `cycode_cli.exe`, les DLL MSVC, ripgrep
vérifié, CPython/CyToolsCore/MCP isolés, les licences, le catalogue des méthodes
et le skill `cycode-core`. Les compilateurs, clangd, outils de build et autres
serveurs de langage restent des prérequis externes. Un installateur par utilisateur
reprend cette charge utile et peut connecter un projet choisi. Aucun workflow
GitHub ne construit ces paquets.

**Limite actuelle :** le propriétaire a explicitement autorisé une compilation
et une installation de production après le premier incident antivirus. Le Core
installé a réalisé les recherches de fichiers/texte, l'évaluation MSBuild, la
navigation et les références du compilateur, puis a rédigé le guide d'intégration
de l'IDE par MCP. Le module Surveillance du système de Kaspersky a ensuite supprimé
ce CLI le 9 novembre 2026 à 19 h 20 (Europe/Paris). Son MD5 correspond au build.
La connexion MCP est désactivée, aucun Core ne reste actif et aucune reconstruction,
restauration ou relance automatique ne suit cette seconde détection. Un faux
positif n'est pas établi. La validation MCP installée complète **n'est pas réussie**
et la publication binaire est suspendue. Seul un paquet explicitement nommé
`agents-only.zip` convient au brouillon : runtime MCP et instructions, **sans moteur
natif**. Il ne peut pas activer le serveur ni être présenté comme un installateur
fonctionnel. Voir [l'enquête](native-core#antivirus).

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

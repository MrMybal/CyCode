# CyCode — premier socle de développement

Lancer **CyCode.exe**. Le programme ouvre une interface native C++ utilisant CyUI.
CyCode est sous [GNU GPL v3 uniquement](LICENSE) (`GPL-3.0-only`).
Copyright (c) 2026 Cyberalien ; voir [COPYRIGHT](COPYRIGHT).
Les dépendances conservent leurs propres licences.

Version **0.2.0** : [notes de version](CHANGELOG.md) et
[téléchargements GitHub](https://github.com/MrMybal/CyCode/releases).
Extraire le ZIP complet dans un dossier accessible en écriture, puis lancer CyCode.exe.
Le runtime Visual C++ 2022 x64 et un pilote OpenGL 3.3 sont nécessaires.
Le paquet inclut la recherche ripgrep ; les serveurs de langage, compilateurs,
débogueurs et le runtime MCP privé nécessitent leur installation séparée.

La bibliothèque [CyUI](https://github.com/MrMybal/CyUI), encore privée, est sous
[licence MIT](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE).
Une [copie locale de sa licence](app/licenses/CyUI.txt) est fournie.

- **Fichier > Ouvrir une solution** : ouvrir directement un `.sln` ou `.uproject` ; un dossier reste possible.
- **Ctrl+Alt+F** : retrouver un fichier ; **double Maj** : rechercher partout.
- Profil **Rider** : **Ctrl+N** cherche un type, **Alt+Inser** crée un fichier,
  **Ctrl+Maj+A** cherche une action. Les profils Visual Studio et VS Code sont sélectionnables.
- **Ctrl+S** : enregistrer ; **Ctrl+F** : rechercher/remplacer dans le document.
- **F12** : aller à la définition C/C++ ; **Maj+F12** : rechercher les références.
- Menu **Navigation** et panneau **Références** : afficher et ouvrir les emplacements trouvés.
- **Ctrl+Maj+F** : recherche dans le projet avec ripgrep.
- Panneau **Gestion de versions** : différences, préparation, commits, branches,
  historique, remotes, fetch/pull/push, stashes et conflits de texte à trois versions.
- Profil Rider : **Ctrl+K** ouvre les commits ; **Ctrl+Maj+K** ouvre la synchronisation Git.
- **Alt+F12** : terminal interactif PowerShell/cmd, plusieurs sessions et fenêtres détachables.
- Menu **Exécuter** : lancer, déboguer et configurer un exécutable ou une assembly .NET.
  Les débogueurs C++ et C# proposent points d'arrêt, conditions, pile, variables,
  espions et pas à pas. Les logpoints dépendent du moteur ; NetCoreDbg ne les propose pas.
- Profil Rider : **Maj+F9** débogue, **Maj+F10** exécute, **F8** passe au pas suivant,
  **F7** entre dans une fonction, **Ctrl+F2** arrête, **Ctrl+F8** bascule un point d'arrêt.
- Panneau **Compilation** : lancer la configuration ou la compilation et consulter la sortie copiable.
- **Compilation > Reglages CMake** : détecter les toolchains, choisir le compilateur,
  le générateur, la configuration et la cible, puis configurer et compiler.
- Panneau **MSBuild** : compiler directement une solution Visual Studio ou un projet
  C++/.NET, choisir configuration/plateforme et projet, nettoyer/recompiler/restaurer.
  **Préparer l'analyse MSBuild** transmet les includes et définitions C++ à clangd.
  Les erreurs de compilation avec position apparaissent dans **Problèmes**.
- Panneau **MCP** : copier la configuration de connexion à l'instance ouverte. Les assistants utilisent CyToolsCore.
- **Options** : langue EN/FR, Graphite et cinq thèmes CyUI, taille du texte et raccourcis personnalisables.

Clangd fournit la complétion C/C++, les diagnostics, les définitions et les références.
Roslyn fournit ces services pour C#, avec les références entre projets .NET, la
recherche de types/membres, le renommage avec aperçu et le formatage. Son serveur
et son SDK .NET d'analyse sont installés dans le runtime privé du projet ; les
installations globales ne sont pas modifiées. Le débogage C#/.NET utilise NetCoreDbg,
avec sélection automatique du moteur et du runtime à partir du programme compilé.
Les DLL exécutables figurent parmi les artefacts évalués du panneau MSBuild ;
elles peuvent alimenter le profil du débogueur. Le parcours console .NET 8 avec
symboles PDB portables est testé. La sortie apparaît dans la console de débogage ;
pour saisir du texte au clavier dans le programme, utiliser **Exécuter sans débogage**.
Le MCP partage ce service et les modifications non enregistrées avec l'interface.
Les en-têtes des SDK/Unreal enregistrés s'ouvrent en lecture seule.
La coloration C++/Unreal distingue directives bleues, chemins et chaînes orangés,
types violets, fonctions turquoise, macros et métadonnées de réflexion ; les
commentaires restent verts. Les mots-clés C++ comme `static`, `bool` et `const`
partagent la même couleur. La palette est adaptée aux thèmes sombres et clairs.
Clangd et Roslyn complètent cette coloration avec les symboles réels : alias,
fonctions, variables, paramètres, champs et constantes d'énumération. Clangd atténue
aussi les branches de préprocesseur inactives. Les anciennes couleurs d'analyse
sont écartées dès qu'un document change ; la coloration immédiate reste disponible.
Le renommage C++ multi-fichier dispose d'un aperçu, d'une application aux buffers
et d'une annulation.
Une configuration CMake réussie transmet ses includes
et macros au service de langage. Ninja fournit sa base compile_commands.json ;
avec Visual Studio, CyCode construit une base d'analyse depuis les métadonnées CMake.
Le profil local est conservé dans .cycode/build.json. Pour changer de compilateur
ou de générateur, choisir un nouveau dossier de compilation.
Git et CMake sont des outils externes détectés. Ripgrep, CodeLLDB et NetCoreDbg sont installés
dans runtime/tools par le script de build, avec versions et empreintes verrouillées.
Le dossier app/examples/hello contient un petit projet CMake pour essayer la compilation.

Cette version est un jalon utilisable, pas une parité fonctionnelle avec Rider.
Le terminal et les débogueurs C++/.NET sont exécutés et testés sur Windows.
Les autres frameworks .NET, Rust/Java/web, les aperçus shaders/web, les refactorisations plus larges,
les workflows VCS restants et la validation Linux/macOS restent à réaliser.
L'éditeur accepte actuellement les fichiers UTF-8 jusqu'à 4 Mio.

Les données et préférences sont dans data/, les dépendances et builds dans runtime/.
Ne pas déplacer runtime/python après installation : son environnement est créé pour
ce chemin. La distribution actuelle est un build de développement Windows ; elle
n'est pas encore un installeur autonome testé sur une machine vierge.

Instructions développeur : [app/README.md](app/README.md).
État précis et preuves : [app/Docs/Validation.md](app/Docs/Validation.md).
Toolchains et limites CMake : [app/Docs/Build.md](app/Docs/Build.md).
Exécution et limites du débogueur : [app/Docs/Execution.md](app/Docs/Execution.md).
Git et conflits : [app/Docs/VersionControl.md](app/Docs/VersionControl.md).
Solutions et projets Visual Studio : [app/Docs/MSBuild.md](app/Docs/MSBuild.md).
Services C# et limites : [app/Docs/CSharp.md](app/Docs/CSharp.md).

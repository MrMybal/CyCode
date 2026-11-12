<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://github.com/MrMybal/CyCode/tree/main">Sources du Core natif</a><br>
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview">Documentation · manuel HTML</a><br>
      <a href="https://github.com/MrMybal/CyCode">GitHub</a><br><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCode Core" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Licence du projet</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>Paquet Core publié</sub><br><a href="https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.3"><strong>0.1.3</strong></a><br><strong>PRÉVERSION DE DÉVELOPPEMENT</strong></p>
      <p><a href="https://github.com/MrMybal/CyCode/releases/download/core-v0.1.3/CyCodeCore-0.1.3-windows-x64-setup.exe"><strong>Installer CyCodeCore</strong></a><br><sub>Windows x64</sub><br><a href="https://github.com/MrMybal/CyCode/releases/download/core-v0.1.3/CyCodeCore-0.1.3-windows-x64.zip">ZIP portable</a></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCode Core</h1>

Un moteur de code natif C++20 pour les **agents IA, outils de développement et
éditeurs**. Les mêmes services de documents, de projets et de langage sont
accessibles par bibliothèque C++, service en ligne de commande persistant ou
pont SDK/MCP officiel CyToolsCore.

> **PRÉVERSION DE DÉVELOPPEMENT · Paquet Core 0.1.3**
>
> L’installateur et le ZIP Windows publiés concernent le Core autonome.
> La version 0.1.3 inclut le catalogue actuel de 252 opérations et les transactions natives communes.
> Consulter les [notes de version et limites connues](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.3).

## Un Core, des applications distinctes

| Projet | Rôle |
| --- | --- |
| **CyCode Core · ce dépôt** | Documents natifs, services de projets, analyse par les outils du compilateur, modifications protégées et automatisation sans interface. |
| [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) | IDE de bureau : fenêtres, panneaux, commandes et parcours d’édition graphique. |
| [CyCodeReader](https://github.com/MrMybal/CyCodeReader) | Éditeur autonome de texte/code utilisant le profil texte natif léger. |

**Sources actuelles :** `main` contient le Core natif autonome. Le développement
de l’interface de bureau appartient à CyCodeIDE, et celui de l’éditeur de texte
autonome à CyCodeReader. Les anciens téléchargements CyCode 0.2.x correspondent
à l’application de bureau précédente. Les sources ajoutent `CyCode::PagedText`,
lecture protégée par lignes/UTF-16, Enregistrer sous exclusif et récupération
paginée explicite après crash. Le catalogue canonique expose 252 opérations
publiques. Voir [les grands textes et projets](app/Docs/LargeProjects.md),
[l’analyse persistante](app/Docs/AnalysisProgress.md) et
[l’intégration native](app/Docs/CoreExtraction.md). Le paquet Windows 0.1.3 provient de ces sources natives maintenues.
Installer et reconnecter un client restent distincts de la mise à jour des sources.

Le Core fonctionne sans CyUI, SDL, ImGui, OpenGL ni fenêtre d’IDE. Les applications
choisissent les services nécessaires ; le profil texte autonome ne démarre pas
les services de projet ou de langage.

## Ce que permet le Core

- **Explorer le code** : rechercher des fichiers et du texte, puis interroger
  définitions, références, symboles et diagnostics via un serveur de langage configuré.
- **Travailler sur de vrais projets** : utiliser les adaptateurs CMake,
  MSBuild/solutions Visual Studio et Unreal avec la véritable chaîne d’outils et
  le contexte de compilation du projet.
- **Examiner les changements avant application** : préparer des modifications
  protégées par révision, prévisualiser les transactions multifichiers,
  appliquer/annuler puis enregistrer explicitement. La récupération et les
  contrôles de conflit disque préservent la gestion des documents actifs.
- **Automatiser les services de développement** : exposer compilation, débogage
  DAP, contrôle de versions et plugins natifs à une application ou à un agent
  autorisé. Les capacités dépendent des outils installés et du fournisseur choisi.
- **Intégrer les composants utiles** : `CyCode::Core` pour les projets,
  `CyCode::TextSession` / `TextBridge` pour le texte autonome, ou les composants
  natifs `Files`, `Snippets`, `PagedText`, `Process`, `Transport` et `Protocol`.

Compilateurs, clangd/autres serveurs de langage, adaptateurs de débogage et clients
de contrôle de versions restent des prérequis externes. Les plugins natifs
exécutent du code de confiance. La couverture sémantique dépend des véritables
commandes de compilation et des en-têtes générés ; la couverture exhaustive
d’Unreal et l’équivalence avec Rider/CLion ne sont pas établies. Le travail sur
les grands projets est documenté séparément ; la campagne complète Unreal reste inachevée.

---

## Installation

1. [Télécharger l’installateur CyCodeCore pour Windows x64](https://github.com/MrMybal/CyCode/releases/download/core-v0.1.3/CyCodeCore-0.1.3-windows-x64-setup.exe) et installer
   pour l’utilisateur actuel. Aucun compte administrateur ni Python système n’est nécessaire.
2. Sélectionner **CyCodeUpdater** si souhaité. Cette application vérifie les
   versions du Core à son ouverture et propose le téléchargement vérifié de
   l’installateur, puis son installation avec confirmation. La vérification à
   l’ouverture de Windows est une option séparée, décochée par défaut.
3. Pour une installation portable, [extraire le ZIP complet](https://github.com/MrMybal/CyCode/releases/download/core-v0.1.3/CyCodeCore-0.1.3-windows-x64.zip).
   Conserver le runtime, `app/`, les outils, notices et documentation à côté du moteur.

Le paquet complet inclut le CLI natif, le runtime MSVC local, l’environnement
isolé CPython/CyToolsCore/MCP, ripgrep, le skill `cycode-core` pour les agents et
le lecteur CyDoc hors ligne anglais/français. Les paquets ne sont pas signés.
La distribution publiée cible Windows x64 ; le packaging Linux/macOS et les
derniers changements demandent une validation séparée. Voir le
[guide d’installation et de mise à jour](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreInstallation.md).

### Connecter un agent IA

L’installateur peut enregistrer un projet existant. Pour le faire manuellement,
lancer cette commande depuis le dossier de l’installation complète en remplaçant
le chemin d’exemple par celui du projet :

```powershell
.\tools\python\python.exe -B app/scripts/install_core_agents.py --workspace "C:/Projects/MyProject" --access edit --enable
```

Cette commande installe le skill et configure la connexion MCP Codex du projet.
`--access edit` autorise les modifications de sources et l’exécution de processus ;
les écritures du contrôle de versions, des plugins et des paramètres restent
séparées. Recharger la connexion MCP du client après l’enregistrement. Les autres
clients MCP peuvent utiliser la commande stdio documentée. Le
[guide des agents](https://github.com/MrMybal/CyCode/blob/main/app/Docs/Agents.md) décrit la découverte, les jobs,
permissions et la connexion au service partagé des documents actifs de l’IDE.

## Sources, compilation et intégration

<details>
<summary>Compiler le Core natif sans dépendances graphiques</summary>

Prérequis : Git, Python 3.11+, CMake 3.24+ et un compilateur C++20. Les projets
Windows demandent les outils Visual Studio/MSBuild correspondants. Les
dépendances des sources sont épinglées ; les chaînes d’outils de langage ont
leurs propres prérequis.

```sh
git clone --branch main https://github.com/MrMybal/CyCode.git
cd CyCode
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

Utiliser le tag `core-v0.1.3` pour reproduire les sources de la préversion publiée.
Les builds ordinaires ne lancent ni tests natifs ni application. GitHub Actions
reste désactivé. Le [guide d’intégration native](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreExtraction.md)
décrit les composants, l’intégration et le service persistant stdio/loopback.
L’API source ne constitue pas une ABI binaire C++ stable.

</details>

## Documentation

Le [manuel HTML français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview)
conserve le lecteur CyDoc, la recherche, les images et le choix de langue. Le
manuel des applications hébergé contient des éléments de l’ancien IDE ; utiliser
les guides actuels d’[installation du Core](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreInstallation.md),
d’[intégration native](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreExtraction.md) et le
[catalogue des méthodes](https://github.com/MrMybal/CyCode/blob/main/app/MCPMethods.json) pour le moteur publié.
Le lecteur du paquet complet s’ouvre aussi localement par `file://`, sans serveur.

---

## Licence

**Les sources originales, la documentation et les exemples de CyCode utilisent
la [GNU GPL v3 uniquement](LICENSE) (`GPL-3.0-only`).** Voir [COPYRIGHT](COPYRIGHT).
Les dépendances conservent leurs propres conditions.

### Core natif et SDK/runtime optionnels

| Bibliothèque · sources | Licence · texte | Utilisation / périmètre |
| --- | --- | --- |
| [nlohmann/json](https://github.com/nlohmann/json) | [MIT](https://github.com/MrMybal/CyCode/blob/main/app/licenses/nlohmann-json.txt) | Modèles et protocoles JSON natifs. |
| [TinyXML2](https://github.com/leethomason/tinyxml2) | [zlib](https://github.com/MrMybal/CyCode/blob/main/app/licenses/TinyXML2.txt) | Projets et données de fournisseurs XML. |
| [Monocypher](https://monocypher.org/) | [BSD-2-Clause OR CC0-1.0](https://github.com/MrMybal/CyCode/blob/main/app/licenses/Monocypher.txt) | Primitives cryptographiques et vérification des paquets de plugins natifs. |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | [BSD-3-Clause WITH PCRE2-exception](https://github.com/MrMybal/CyCode/blob/main/app/licenses/PCRE2.txt) | Transformations de snippets par expressions régulières dans le profil projet. |
| [utf8proc](https://github.com/JuliaStrings/utf8proc) | [MIT + conditions des données Unicode](https://github.com/MrMybal/CyCode/blob/main/app/licenses/utf8proc.txt) | Conversion de casse Unicode des snippets. |
| [libvterm](https://www.leonerd.org.uk/code/libvterm/) | [MIT](https://github.com/MrMybal/CyCode/blob/main/app/licenses/libvterm.txt) | Modèle de terminal dans le profil projet. |
| CyToolsCore SDK · distribution privée | [MIT](https://github.com/MrMybal/CyCode/blob/main/app/licenses/CyToolsCore.txt) | Runtime SDK/MCP, jobs, identités et permissions ; fourni dans le paquet Windows complet. |
| [MCP Python SDK](https://github.com/modelcontextprotocol/python-sdk) | [MIT](https://github.com/MrMybal/CyCode/blob/main/app/licenses/MCP-Python.txt) | Transport MCP dans le runtime Python privé. |

Le paquet Windows complet conserve aussi les notices PSF et des composants de
CPython ainsi que les [conditions MIT OR Unlicense de ripgrep](https://github.com/BurntSushi/ripgrep/blob/15.2.0/COPYING).
Les serveurs de langage et adaptateurs optionnels ont leurs propres licences.
Le lecteur [CyDoc](https://github.com/MrMybal/CyDoc) embarqué utilise MIT ; ses
outils de génération et notices des médias sont indexés dans les
[notices tierces](https://github.com/MrMybal/CyCode/blob/main/app/THIRD_PARTY_NOTICES.md).

[CyUI](https://github.com/MrMybal/CyUI) conserve sa [licence MIT](app/licenses/CyUI.txt).
Les dépendances graphiques comme CyUI, Dear ImGui et SDL concernent les
applications de bureau et l’arborescence historique ; elles ne sont pas
nécessaires à la compilation du Core natif. Le tableau est un index des
composants et ne remplace pas les notices complètes.

## Références

- [Version publiée du Core et empreintes](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.3) · [Toutes les versions](https://github.com/MrMybal/CyCode/releases).
- [Sources du Core natif](https://github.com/MrMybal/CyCode/tree/main) · [Suivi des problèmes](https://github.com/MrMybal/CyCode/issues).
- [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) · [CyCodeReader](https://github.com/MrMybal/CyCodeReader).
- [Manuel français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview) · [English manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview).

<details>
<summary><strong>AI Usage</strong></summary>

- [Commencer ici : instructions du projet Core](https://github.com/MrMybal/CyCode/blob/main/app/AGENTS.md) — responsabilités et conventions du dépôt.
- [Guide des agents](https://github.com/MrMybal/CyCode/blob/main/app/Docs/Agents.md) — démarrage persistant, contexte du compilateur, jobs MCP et changements protégés.
- [Installer et mettre à jour](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreInstallation.md) — paquets, skills et enregistrement des projets.
- [Intégrer la bibliothèque native](https://github.com/MrMybal/CyCode/blob/main/app/Docs/CoreExtraction.md) — composants, API source et périmètre des validations.
- [Schémas des opérations](https://github.com/MrMybal/CyCode/blob/main/app/MCPMethods.json) — opérations prises en charge et leurs véritables contrats.

</details>

<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://github.com/MrMybal/CyCode">GitHub</a><br>
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview">Documentation · manuel des applications</a><br>
      <a href="app/Docs/CoreExtraction.md">Guide d’intégration native</a><br><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCode Core" width="128"><br>
      <strong>CyCode Core</strong>
    </td>
    <td width="300" align="right" valign="top">
      <p><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p>API source native <strong>1</strong><br>
      <strong>Version de développement</strong></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCode Core</h1>

Le moteur C++20 natif commun à CyCodeIDE, CyCodeReader et aux futurs outils de
code. Le Core possède les documents, adaptateurs de projets, analyses de langage,
diagnostics, plugins natifs et transactions protégées. Le CLI/service utilise le
même moteur sans initialiser d’interface graphique.

CyCodeIDE possède les fenêtres, panneaux et la présentation CyUI. Reader utilise
le profil texte léger du Core pour les documents autonomes et conserve l’éditeur
graphique partagé. Les [versions 0.2.3 historiques](https://github.com/MrMybal/CyCode/releases)
précèdent cette séparation ; elles ne sont pas une nouvelle distribution du Core
ou de CyCodeIDE.

## Version Windows autonome

[CyCodeCore 0.1.1 : installateur et ZIP portable](https://github.com/MrMybal/CyCode/releases/tag/core-v0.1.1)
fournit le CLI natif, le runtime privé Python/CyToolsCore MCP, ripgrep et le skill
pour les agents. L’installateur propose CyCodeUpdater comme application séparée,
avec vérification optionnelle à l’ouverture de Windows et installation confirmée
des nouvelles versions GitHub du Core. Les compilateurs et serveurs de langage
restent des prérequis externes. Voir le
[guide d’installation et de mise à jour](app/Docs/CoreInstallation.md).

## Compiler sans interface

```sh
python app/scripts/fetch_dependencies.py --core-only
cmake -S . -B runtime/core -DBUILD_TESTING=OFF
cmake --build runtime/core --config Release --target cycode_cli
```

CMake 3.24+ et C++20 sont nécessaires. Les dépendances et la configuration
natives ne demandent ni Reader, ni CyUI, SDL, ImGui, OpenGL ou Tree-sitter. Un build
ordinaire ne lance ni application ni tests natifs. Les révisions des sources et
archives tierces sont épinglées.

## Choisir le profil natif

| Profil | Cibles et responsabilités |
| --- | --- |
| Projet | CyCode::Core · Engine, CMake/MSBuild/Unreal, LSP, DAP, VCS, transactions |
| Texte autonome | CyCode::Documents / TextSession / TextBridge |
| Socle natif | CyCode::Files / Snippets / Process / Transport / Protocol |

Une application de texte sélectionne CYCODE_COMPONENTS et récupère seulement
JSON avec `fetch_dependencies.py --components TextSession TextBridge Process`.
Le résolveur commun réutilise les cibles et refuse de mélanger plusieurs copies
du Core. Les profils projet et texte partagent les buffers, encodages et
primitives de fichiers, avec leurs règles de chemins, révisions et transactions.
Le profil texte de Reader ne lance pas les services de projet/langage. L’API
source 1 n’est pas une promesse d’ABI binaire C++ stable.

## Service natif persistant

```sh
cycode_cli --disable-plugins <project> --stdio
```

```json
{"protocol":"cycode-core/1","id":1,"method":"service.status","params":{}}
```

Le même Engine et ses moteurs restent actifs entre les requêtes. Les réponses
conservent l’id et structurent les erreurs. service.status distingue le cycle de
vie/contexte de la disponibilité des moteurs et diagnostics liés aux révisions
dans language.status. L’arrêt/EOF n’enregistre pas les buffers modifiés. Les
protocoles de descripteurs locaux restent compatibles. CyToolsCore conserve la
gestion des jobs SDK/MCP, identités et permissions.

## Agents IA et MCP

Paquets autonomes et skill IA réutilisable : [installation du Core](app/Docs/CoreInstallation.md).

Le [guide des agents en français](app/.CyDocBuild/manual/fr/core-agents.md) décrit le démarrage sans interface,
la préparation sémantique, les jobs et les transactions protégées. Le
[catalogue MCP](app/MCPMethods.json) contient les 239 opérations natives avec
leurs schémas complets. CyToolsCore possède le MCP et les permissions.

## Vérifications et limites

Le Core 84e4c78 a passé la suite native de 63 cas sur Windows x64, Ubuntu x64 et
macOS arm64 (MSBuild est ignoré sur POSIX), dont les contrôles concurrents de
propriété des processus. Le replay Git corrigé sur macOS a aussi passé trois
répétitions supplémentaires. Reader 671c588 a passé 17 cas de services/vues et son
profil headless séparé sur ces systèmes. Le guide donne les liens CI et les limites
exactes. Le propriétaire a autorisé séparément les tests du Core/clangd de
production sur UE 5.5.4 : renommage initial sur trois fichiers, Apply/Undo,
invalidation du cache et parcours MCP passent. Voir [les résultats délimités](app/Docs/Benchmarks/unreal55-fixes-20261109.json).
Les lancements graphiques/exécutables de test restent suspendus ; GitHub Actions reste désactivé.

La référence sémantique mesure les vraies requêtes clangd sur le projet CMake du
Core (121 unités de compilation). Le [guide d’architecture](app/Docs/CoreExtraction.md)
conserve les mesures brutes, l’identité du corpus, le périmètre mémoire, les liens
CI et les limites. Une compilation, un cache de chemins ou une réponse de
références non vide ne prouvent pas la couverture sémantique complète. Les
vérifications exhaustives Unreal et sur de vrais projets MSBuild, l’invalidation plus large, la
synchronisation incrémentale et les transformations avancées restent à poursuivre.
Aucune équivalence CLion/IntelliJ n’est annoncée.

## Applications et documentation

- [CyCodeIDE](https://github.com/MrMybal/CyCodeIDE) — hôte graphique ; l’accès au dépôt peut être nécessaire.
- [CyCodeReader](https://github.com/MrMybal/CyCodeReader) — éditeur autonome de texte/code.
- [CyUI](https://github.com/MrMybal/CyUI) — modules de présentation graphique, hors du Core.
- [CyDoc](https://github.com/MrMybal/CyDoc) — le manuel HTML bilingue reste canonique et local/hors ligne ; il est déployé par l’IDE.

Consultez [LICENSE](LICENSE), [COPYRIGHT](COPYRIGHT) et les
[notices des dépendances](app/THIRD_PARTY_NOTICES.md). Les plugins natifs de projet
et de texte conservent leurs ABI C ; ils exécutent du code de confiance et ne sont
pas interchangeables.

[Qualité du Core et validations reproductibles](app/Docs/CoreQuality.md) : écritures communes, inspections C++ natives et transformations du compilateur protégées.

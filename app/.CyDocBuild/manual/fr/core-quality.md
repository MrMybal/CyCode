---
id: core-quality
title: Qualité et validation du Core
description: Écritures communes, inspections du compilateur, transformations C++ et mesures réelles.
section: Pour commencer
parent: native-core
order: 264
sourceRevision: 2
---

## Publication atomique commune {#atomic}

CyCode::Files fournit désormais un seul mécanisme natif de fichiers temporaires,
écriture, vidage et publication. Documents, métadonnées CMake/MSBuild et descripteurs
privés l’utilisent. La création du temporaire est exclusive et son nom est indépendant
du fichier cible. Les erreurs ordinaires nettoient uniquement le temporaire possédé ;
un échec partiel de ReplaceFile conserve les octets de remplacement restants.

Chaque appelant choisit sa politique : création exclusive, remplacement conservant
les métadonnées ou publication du fichier nouvellement protégé. Le transport impose
et vérifie une DACL utilisateur/LocalSystem avant écriture sous Windows et les droits
0600 du propriétaire sous POSIX. Documents et métadonnées conservent leurs contrôles
de conflit ; MSBuild garde sa reprise bornée des partages Windows transitoires.
Les métadonnées sont limitées à 32 Mio et une écriture identique est évitée. La
vérification précédant la publication n’est pas un compare-and-swap du système.
Les contrats source et ABI restent compatibles, sans dépendance graphique.

## Responsabilités MSBuild {#msbuild}

Sélection du projet, métadonnées d’artefacts et préparation de l’index disposent
de fonctions distinctes et lisibles. Les cycles de dossiers de solution sont
refusés. Inclusions, définitions, mode du langage et exceptions proviennent toujours
de l’évaluation réelle par MSBuild. Les limites et sélections explicites existantes
restent visibles ; un profil préparé ne valide pas toutes les configurations.

## Inspections C++ {#inspections}

L’opération MCP analysis_inspect et la méthode native analysis.inspect lancent
clang-tidy installé sur une unité enregistrée ayant une commande de compilation
unique dans un vrai compile_commands.json du projet. Une lecture préalable fournit
expectedRevision ; indiquez path, compilationDatabase et le profil souhaité.
Le runtime CyToolsCore officiel gère les jobs et l’autorisation process.execute.

Le profil recommended inclut Clang Static Analyzer, bugprone, performance et
portability. Les profils security, modernize, performance et portability sont
également disponibles. Aucun correctif -fix, argument arbitraire, drapeau de
compilation inventé ou enregistrement de source n’est demandé. Les entrées de
compilation/configuration du projet restent des outils exécutables de confiance.
Un outil absent produit BackendUnavailable.

Les documents modifiés ou en conflit bloquent l’analyse disque. Identité, révision,
observations du projet, contenu enregistré et base de compilation protègent le
résultat ; un changement produit le statut stale. Les coordonnées du compilateur
sont des colonnes d’octets UTF-8 commençant à 1, distinctes des positions UTF-16 de
l’éditeur. Diagnostic, sortie et état du processus restent bornés. Le propriétaire
de build existant fournit état, journal et annulation. Chaque requête couvre une
unité et ses en-têtes ; parcourez explicitement une portée pour en couvrir davantage.

## Transformations C++ protégées {#refactoring}

La préparation des actions reconnaît une liste explicite de transformations
clangd : extraction de fonction/variable, variable en ligne, expansion de macro
ou de type auto, définition inline/hors classe, suppression de using namespace,
enum class et inversion des branches if, lorsqu’elles sont réellement proposées.
Aucune commande arbitraire du serveur n’est autorisée. Avec clangd 18.1.8,
InlineVariable est absent et SwapIfBranches est masqué : ces deux actions ne sont
pas proposées. Le test dédié exige huit transformations réellement disponibles,
avec aperçu/Apply/Undo : extractions, macro, ExpandDeducedType, définitions
inline/hors classe sur deux fichiers, using namespace et enum class. Un nom
autorisé ne constitue pas à lui seul une fonctionnalité implémentée.

Le serveur calcule la modification ; ses demandes workspace/applyEdit sont
toujours refusées. Une seule modification capturée passe par l’aperçu natif puis
Apply/Undo. URI, sélection, révision, projet, backend et session sont contrôlés.
Aucune source n’est sauvegardée automatiquement et les opérations de fichiers
non prises en charge restent refusées. Après interruption d’un aperçu de commande,
redémarrez le backend avant un autre aperçu pour écarter les réponses tardives.

## Mesures reproductibles {#benchmarks}

benchmark_inspections.py nécessite --run-native, un Core existant, les sources,
la vraie base de compilation et clang-tidy. JetBrains jb et une solution sont
optionnels. Le rapport conserve échantillons, médiane/P95, empreintes des sources
et du contexte, diagnostics et sommes de mémoire résidente observées. psutil est
facultatif. Les mesures toutes les 50 ms peuvent manquer des pics brefs et compter
plusieurs fois les pages partagées ; une mesure absente n’est pas un zéro.

Le projet Windows à deux unités a été examiné par Core et InspectCode 2025.1.5,
version correspondant au Rider installé. Les médianes observées sont environ
0,10 seconde pour les deux requêtes Core et 5,47 secondes pour un nouveau processus
d’inspection de solution JetBrains. Les deux repèrent la variable volontairement
non initialisée. Suites de contrôles et cycles de processus diffèrent ; ces trois
échantillons sur un petit projet ne prouvent ni supériorité de l’IDE, ni couverture
équivalente, ni meilleure consommation globale. Les caches système ne sont pas vidés.

## Reproduire les validations {#validation}

Utilisez le runtime privé Python/CyToolsCore/MCP et un Core de production accepté.
Les scripts verify_core_quality.py, verify_core_debugger.py, verify_core_unreal.py
et verify_inspection_cancellation.py dans app/scripts exigent --run-native ainsi
que les chemins explicites des outils, du Core et d’un nouveau dossier de résultat.
Leur --help décrit les arguments ; les contrôles ordinaires de sources ne les lancent
pas. Aucun exécutable de test généré ni IDE graphique n’est lancé.

La validation Windows couvre le vrai SDK, MSBuild, Debug/Release, navigation,
références, renommage sur trois fichiers, extractions de variable/fonction et macro
avec aperçu/Apply/Undo, invalidation après édition d’en-tête, analyse statique,
révisions périmées, sauvegardes répétées, lecture seule et conflits disque. CodeLLDB
arrête le Core de production, expose pile, mémoire et désassemblage, refuse un ancien
arrêt puis termine normalement. La régression Unreal utilise la vraie base UBT,
l’inventaire, une reprise de lot et le renommage DateTime/HTTP sans sauvegarder les
sources. Les nombres et mesures exacts sont conservés dans les rapports de validation.

## Portée restante {#limits}

Ces nouvelles exécutions valident Windows x64. Linux/macOS, l’interface graphique,
la sémantique exhaustive d’Unreal, les transformations de Blueprints/ressources,
le débogage distant et une parité complète Rider/CLion restent à valider ou développer.
Un outil déclaré ou une compilation réussie ne suffit pas à les établir. La version
installée 0.1.1 reste distincte de ces sources de développement ; un nouveau paquet
accepté sera nécessaire pour mettre à jour les autres clients. GitHub Actions reste
en pause.

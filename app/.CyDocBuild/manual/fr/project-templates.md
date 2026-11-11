---
id: project-templates
title: Créer un projet à partir d’un modèle
description: Examiner les fichiers générés, outils requis et options fournis par les extensions.
section: Travailler dans CyCode
parent: projects
order: 121
sourceRevision: 1
---

## Examiner le nouveau projet {#create}

Choisissez **Fichier > Nouveau projet**, puis un modèle, un nom et un dossier
parent existant. L’aperçu montre les fichiers texte exacts, l’entrée, la première
source et les outils requis. **Créer et ouvrir** crée un nouveau dossier.
La case de nouvelle fenêtre ouvre une autre instance uniquement sur ce choix explicite.

| Modèle | Projet |
| --- | --- |
| Console C++ / C | CMake, C++20 / C17 et un test simple CTest |
| Console Rust | Cargo, Rust 2021 et un test unitaire |
| Console C# | `.sln` et `.csproj` SDK ; choisir net8.0, net9.0 ou net10.0 |
| Console Java | `javac` / `java` autonomes |
| Web JavaScript | HTML/CSS, modules ES et scripts npm |
| Jeu C++ Unreal | Extension Unreal fournie ; association moteur et règles de module/cibles |

Installez séparément les SDK compatibles. La création n’installe pas d’outils,
ne restaure pas de paquets, n’exécute pas le code généré, n’initialise pas Git
et ne choisit pas de licence. Le README produit explique la compilation et le
lancement. Le modèle web utilise les composants Node intégrés ; son script Run
sert explicitement les ressources en local sans ouvrir de navigateur.
Les exemples d’auto-test Java/.NET ne constituent pas une intégration unifiée des tests.

## Noms, destinations et nouvelles tentatives {#destinations}

Le dossier final doit être absent et son parent doit exister. Un nom portable
commence par une lettre et contient lettres, chiffres, `_` ou `-`, sur au plus
64 caractères. Un modèle exigeant un identifiant, dont Unreal, exclut les tirets.
Noms protégés, traversées, dossiers redirigés et liens sont refusés.

Examinez un nouvel aperçu si le précédent expire ou si son extension change.
Un plan inconnu ou déjà utilisé ne peut pas créer deux fois. La création publie
les fichiers vérifiés sans écraser de destination. Si elle réussit mais que les
documents modifiés empêchent de changer de projet, enregistrez/fermez-les puis
utilisez **Ouvrir le projet créé**. Ne recréez pas une copie pour réessayer l’ouverture.

Aucune annulation destructive du projet entier n’est proposée. Les destinations
SDK restent dans le projet courant ; le parent choisi dans l’interface n’élargit
pas l’accès MCP.

## Étendre le catalogue {#plugins}

Les extensions ABI2 actives peuvent fournir des modèles déclaratifs avec choix,
booléens, identifiants et valeurs typés. Le contenu utilise `${project.name}`,
`${project.identifier}` et `${option.ID}` ; ce n’est pas un moteur de scripts.
Un modèle génère de 1 à 64 fichiers UTF-8, limités à 256 Kio après expansion.
Un nouveau modèle n’implémente pas un autre adaptateur de compilation.
Voir [le contrat des extensions](doc:plugin-api#contributions) et
[les opérations de modèles](doc:api-templates#operations).

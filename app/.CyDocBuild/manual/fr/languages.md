---
id: languages
title: Coloration et services de langage
description: Distinguez les modes de coloration de la complétion et des refactorisations.
section: Travailler dans CyCode
order: 110
sourceRevision: 2
---

## Modes de coloration {#syntax}

Le catalogue CyUI partagé fournit 101 modes de syntaxe, dont C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, Python, Lua, JSON, Markdown, HLSL, GLSL et les journaux. CyCode choisit la syntaxe selon le chemin du document. La coloration des journaux distingue les lignes Error et Warning.

La coloration reste utile sans compilateur ni serveur de langage. Un mode de syntaxe ne constitue pas un analyseur complet, un index de projet ou un débogueur.

## Configurer un service {#services}

Ouvrez les commandes de serveurs de langage dans Problèmes. Consultez leur disponibilité, activez, redémarrez ou désactivez le serveur sélectionné, puis réglez ses arguments et paramètres pour le projet. Les serveurs supplémentaires installés peuvent démarrer à l’ouverture de leurs fichiers. Une simple consultation n’installe pas de chaîne de compilation.

| Langage | Service |
| --- | --- |
| C/C++ et Unreal | clangd |
| C# | csharp-ls / Roslyn |
| Rust | rust-analyzer |
| Java | Eclipse JDT LS |
| JavaScript / TypeScript | typescript-language-server |
| HTML / CSS | Serveurs de langage VS Code |
| Python / GLSL | Pyright / GLSL Analyzer |

Les capacités dépendent du serveur. HTML n’annonce pas les mêmes opérations de référence et de renommage que C++ ; GLSL ne propose pas toutes les opérations de symboles. Consultez les [outils de shaders](doc:shaders#compile) pour les compilateurs nécessaires.

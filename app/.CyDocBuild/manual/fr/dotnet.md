---
id: dotnet
title: Projets C# et .NET
description: Chargez une solution, restaurez les références et utilisez Roslyn.
section: Travailler dans CyCode
parent: languages
order: 310
sourceRevision: 1
---

## Préparer la solution {#solution}

1. Ouvrez le `.sln` ou le dossier contenant le `.csproj`.
2. Rendez disponibles le SDK demandé par `global.json`, les packs de ciblage et les dépendances du projet.
3. Utilisez les commandes MSBuild et de restauration pour la configuration choisie.
4. Ouvrez un fichier `.cs` et consultez le serveur C# dans Problèmes. Attendez la fin du chargement du projet avant les recherches de symboles.

Le serveur d’analyse est csharp-ls/Roslyn. Son SDK privé d’analyse ne remplace pas le SDK de compilation du projet. `CYCODE_CSHARP_LS` et `CYCODE_CSHARP_DOTNET` permettent de choisir des chemins absolus vers le serveur et son hôte. L’évaluation du projet utilise des imports MSBuild de confiance.

## Modifier et déboguer {#services}

La complétion, les définitions/références, la structure, les couleurs sémantiques, le renommage et le formatage utilisent le projet chargé par Roslyn. Le formatage respecte `.editorconfig`. Examinez le renommage avant son application, puis enregistrez explicitement. Redémarrez le serveur après une restauration ou un changement de SDK ou de références ; la surveillance dynamique des fichiers n’est pas implémentée.

Compilez une cible managée avec ses symboles, configurez NetCoreDbg et choisissez son exécutable et ses arguments dans Débogueur. Le [guide de débogage](doc:debugging#launch) décrit le lancement, la pile et les expressions suivies. Tous les parcours .NET de Visual Studio ne sont pas fournis.

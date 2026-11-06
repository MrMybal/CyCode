---
id: debugging
title: Exécuter et déboguer
description: Préparez le lancement, les points d’arrêt et l’inspection de la pile et des variables.
section: Travailler dans CyCode
order: 190
parent: projects
sourceRevision: 2
---

## Démarrer une session {#launch}

Choisissez un adaptateur compatible et configurez l’exécutable, le dossier de travail, les arguments et l’environnement. CodeLLDB sert au débogage natif ; NetCoreDbg à .NET ; Java utilise l’intégration JDT. Compilez le projet et conservez la correspondance entre sources, binaires et symboles.

Placez un point d’arrêt dans la marge, puis lancez le débogage ou un attachement explicite. Consultez sa vérification par l’adaptateur. Un point demandé n’est pas forcément actif dans le programme cible.

## Inspecter et poursuivre {#inspection}

Pendant une pause, sélectionnez un thread et une frame pour examiner les portées, variables et expressions surveillées. La mémoire, le désassemblage, les détails d’exception, les points d’arrêt de fonction, de donnée ou d’instruction et l’exécution inverse dépendent des capacités annoncées par l’adaptateur.

Exécuter jusqu’au curseur reprend temporairement vers un emplacement source. Définir la prochaine instruction déplace le pointeur d’exécution et peut sauter du code ; c’est une action explicite différente. Modifier une variable ou la mémoire agit sur le programme débogué et ne s’annule pas avec l’Undo du document. L’arrêt ou le détachement suit la propriété de la session sélectionnée.

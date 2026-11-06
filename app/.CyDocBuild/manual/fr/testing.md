---
id: testing
title: Exécuter les tests du projet
description: Découvrez et lancez des cas CTest ou les tâches de test du projet.
section: Travailler dans CyCode
order: 200
parent: projects
sourceRevision: 2
---

## Panneau CTest {#tests}

Configurez un dossier de compilation CMake de confiance, puis ouvrez Tests ou demandez la découverte des tests CTest. La découverte lit le modèle réel de CTest ; elle ne compile pas et n’exécute pas les cas listés. Elle peut néanmoins évaluer les définitions de tests du projet.

Filtrez par nom ou étiquette, sélectionnez les tests et lancez la sélection. Consultez chaque résultat, sa durée et sa sortie. Annuler arrête les processus appartenant à cette exécution. Un cas désactivé ou non exécuté diffère d’un succès ; les dépendances de fixtures peuvent lancer des préparations et nettoyages.

## Autres projets {#other}

Cargo, npm, Maven et Gradle peuvent lancer leurs propres tâches de test depuis les tâches de projet. Cela ne fournit pas un arbre de résultats unifié pour tous les frameworks. Le panneau Tests structuré intègre CTest ; ce n’est pas un explorateur de tests .NET ou Unreal Automation.

---
id: debug-inspection
title: Inspecter et modifier une cible arrêtée
description: Utilisez les capacités réelles de l’adaptateur pour mémoire, registres et points d’arrêt.
section: Travailler dans CyCode
parent: debugging
order: 316
sourceRevision: 1
---

## Examiner un arrêt {#inspect}

Sélectionnez le thread arrêté et une frame de la pile. Portées / Registres expose les portées annoncées par l’adaptateur ; chargez explicitement les portées coûteuses. Les registres n’existent que si l’adaptateur fournit une telle portée. Développez les variables par pages et réévaluez les expressions suivies après un nouvel arrêt.

Mémoire exige une `memoryReference` retournée par l’adaptateur et la capacité de lecture mémoire. Choisissez une plage bornée ; les octets illisibles sont signalés séparément. Le désassemblage peut utiliser la référence d’instruction de la frame sélectionnée. Les détails d’exception exigent la capacité correspondante.

## Modifications et points d’arrêt {#changes}

Les modifications de variable, d’expression et de mémoire sont des changements explicites du processus. L’édition mémoire exige un instantané lisible courant et les capacités de lecture et d’écriture. Un résultat peut être incertain si l’exécution change pendant la requête ; examinez à nouveau au lieu de répéter aveuglément une écriture.

Les commandes de points d’arrêt de fonction, d’exception, de données et d’instruction dépendent des capacités annoncées. Consultez la vérification ou l’erreur de l’adaptateur après envoi de l’ensemble demandé. Les identifiants de données et d’instructions appartiennent à la session et à l’arrêt observés ; une actualisation ou reprise peut les invalider. Certaines opérations remplacent l’ensemble complet des points d’arrêt : conservez les entrées souhaitées.

Le pas inverse et le redémarrage d’une frame ne sont proposés que par les adaptateurs compatibles. Une reprise efface les données d’arrêt. Une réponse tardive d’un arrêt précédent est rejetée ; sélectionnez le nouveau thread et la nouvelle frame, puis relisez. Les changements du débogueur ne sont pas annulables dans l’historique du document.

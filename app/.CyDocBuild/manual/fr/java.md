---
id: java
title: Java, Maven et Gradle
description: Sélectionnez les classes principales Java et configurez le débogage JDT LS.
section: Travailler dans CyCode
order: 160
parent: languages
sourceRevision: 2
---

## Compiler un projet JVM {#jvm}

Ouvrez les sources Java, `pom.xml` ou un descripteur de compilation ou de réglages Gradle. Les tâches sélectionnent javac autonome, Maven ou Gradle. Choisissez le dossier source ou le module, puis une classe principale réellement détectée dans les fichiers compilés. Maven et Gradle résolvent leurs dépendances et chemins de classes habituels lors d’une exécution explicite.

## Déboguer et recharger {#debug}

L’édition Java utilise Eclipse JDT LS. Le débogage nécessite aussi le module Microsoft java-debug installé séparément. Le lancement utilise la classe principale et le chemin de classes sélectionnés ; l’attachement JDWP utilise l’hôte et le port configurés. L’attachement ne donne pas à l’IDE la propriété d’une JVM existante.

L’application des changements Java demande un remplacement de code à chaud manuel pour une session active. Les capacités de la JVM et la correspondance entre sources et résultats déterminent ce qui peut être rechargé ; ce n’est pas une recompilation à chaud sans restriction.

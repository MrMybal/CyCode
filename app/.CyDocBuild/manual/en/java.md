---
id: java
title: Java, Maven and Gradle
description: Select real Java main classes and configure JDT LS debugging.
section: Work in CyCode
order: 160
parent: languages
revision: 2
---

## Build a JVM project {#jvm}

Open Java sources, `pom.xml` or a Gradle build/settings descriptor. Project tasks selects standalone javac, Maven or Gradle. Choose the source directory/module and an actual main class discovered from compiled output. Maven and Gradle resolve their normal dependencies and classpaths when explicitly executed.

## Debug and reload {#debug}

Java editing uses Eclipse JDT LS. Debugging also needs the separately installed Microsoft java-debug bundle. Launch uses the selected main class and runtime classpath; JDWP attach uses the configured host and port. Attaching does not make the IDE owner of an existing JVM.

Apply Java code changes requests a manual Hot Code Replace for an active session. JVM capabilities and matching sources/output determine what can be reloaded; this is not unrestricted live recompilation.

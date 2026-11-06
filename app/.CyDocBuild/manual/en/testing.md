---
id: testing
title: Run project tests
description: Discover and run selected CTest cases or project-native test tasks.
section: Work in CyCode
order: 200
parent: projects
revision: 2
---

## CTest panel {#tests}

Configure a trusted CMake build directory, then open Tests or choose Discover CTest tests. Discovery reads CTest's actual model; it does not compile or execute the listed test cases. It can still evaluate project test definitions.

Filter by name or label, select tests and run the selection. Review each result, duration and output. Cancel stops the owned test process tree. Disabled or not-run cases are different from passed tests, and fixture dependencies may run setup/cleanup cases.

## Other projects {#other}

Cargo, npm, Maven and Gradle can run their own test tasks from Project tasks. This does not imply a unified result tree for every framework. The structured Tests panel is a CTest integration, not a .NET or Unreal Automation test explorer.

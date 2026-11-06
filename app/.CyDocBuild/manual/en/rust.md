---
id: rust
title: Rust and Cargo
description: Choose Cargo packages, profiles, features and runnable targets.
section: Work in CyCode
order: 150
parent: languages
revision: 2
---

## Cargo tasks {#cargo}

Open `Cargo.toml` or its workspace folder. In Project tasks, choose the package or entire workspace, target kind, profile and features. Build uses Cargo's real metadata and compiler messages. Run selects an executable binary or example; Debug builds a target before handing it to the shared debugger.

Use installed Cargo or the application's configured private toolchain. Build scripts and procedural macros execute when you deliberately build a trusted project. Rust-analyzer is the separate language service; its initial analysis settings do not automatically run those rules.

Cargo tests use the project's toolchain. For the IDE's structured CTest panel, see [tests](doc:testing#tests).

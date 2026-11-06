---
id: update-recovery
title: Recover an interrupted update
description: Follow the retained recovery instructions for a portable installation.
section: Work in CyCode
parent: updates
order: 317
revision: 1
---

## Locate recovery {#recover}

An interrupted replacement can leave `data/updates/pending.json` and block ordinary startup. Keep that marker and the transaction's files. Open the affected transaction's `RECOVERY.txt`: it identifies the staged `CyCodeUpdater.exe` to use. The full IDE may be unavailable while its managed files are incomplete.

Run that staged helper explicitly. It checks its own identity and the retained transaction, then presents the versions, affected files, conflicts and storage estimate. Close other instances using this installation. Read the restoration question; No is the default. Conflicts or insufficient/unknown capacity can block confirmation.

## Restore and restart {#restore}

During preparation/restoration, follow the phase and file-count progress. A stop request is cooperative; wait for the reported outcome. Keep recovery files after an interrupted or uncertain result. Do not remove managed files manually to force progress.

A completed restoration offers a separate restart question with No selected. A restart failure can occur after restoration succeeded; read the two outcomes separately. Unknown or locally changed files can require manual inspection. This recovery applies to supported format-2 Windows portable transactions, not arbitrary old installers or every damaged installation.

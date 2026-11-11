---
id: updates
title: Update the application
description: Check releases, download an update and choose when to install it.
section: Start here
order: 290
parent: overview
revision: 4
---

## Check, download and install {#updates}

Open View > Application updates. Startup checking can be enabled in options. A newer stable GitHub release offers Later or Skip this version; downloads and installation remain separate actions.

1. Check for a release and read its notes.
2. Download or resume the compatible update. The application verifies its SHA-256 before allowing installation.
3. Choose Install update and restart when ready. Save or discard documents through the normal close prompts; Cancel aborts installation.
4. Close other instances using the same installation. The helper waits for ownership to be released rather than killing those processes.

Interrupted downloads can offer resume or a fresh download. Keep recovery backups until the installation is settled. Storage retirement moves retained data and does not immediately free disk space. Detailed history is informational, not proof that the installed files still match an earlier result.

Checks cannot read a private GitHub release anonymously. If no accessible compatible package is available, use the release page and the supplied archive manually. A connection error is not reported as “up to date”.

See [download resumption, cache storage and history](doc:update-storage#downloads) before reorganising retained update files.


## Windows installation {#windows-installation}

Use the per-user Windows installer or the complete portable ZIP. Both include the local MSVC runtime and complete offline help. User data remains in the writable installation data/ directory and is preserved by uninstall. From 0.2.3 installed copies update through verified Setup and portable copies use the same ZIP for in-app updates; no separate update archive is published. Versions through 0.2.2 require one manual upgrade to adopt this format. Startup checking is enabled by default; existing preferences remain in effect.

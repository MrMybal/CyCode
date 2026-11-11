---
id: updates
title: Update the application
description: Check releases, download an update and choose when to install it.
section: Start here
order: 290
parent: overview
revision: 5
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


## Consumer-specific update identity {#products}

`CyCode::Updates` is one native static library shared by the application and its
helper. Its identity is immutable for that target graph; no workspace, preference,
cache or network response can switch the product. Configure before adding Core:

```cmake
set(CYCODE_BUILD_UPDATES ON)
set(CYCODE_UPDATES_PRODUCT CyCodeIDE)
set(CYCODE_UPDATES_REPOSITORY MrMybal/CyCodeIDE)
set(CYCODE_UPDATES_VERSION "${PROJECT_VERSION}") # The host version, captured before Core's project().
add_subdirectory("${CYCODE_SOURCE}" "${CMAKE_BINARY_DIR}/core")
target_link_libraries(CyCodeIDE PRIVATE CyCode::Updates)
target_link_libraries(CyCodeIDEUpdater PRIVATE CyCode::Updates)
```

The three CMake variables and corresponding `cycode_updates` target properties
are the exact contract. Product identifiers are safe ASCII basenames (up to 64
characters), repositories are bounded `owner/repository` references, and versions
are numeric MAJOR.MINOR.PATCH with components at most 65535. Nonhistorical products
must supply their repository and host version explicitly. Do not reuse a library
already configured for another product; build separate application distributions.
Read `cycode::updates::productIdentity()` from `cycode/Updates.h`: `product`,
`repository`, `currentVersion`, `application`, `helper`, `installationMarker`,
`installationMarkerText`, `tokenEnvironment`, `requiredFiles` and
`acceptsLegacyMetadata`. Existing update operations retain their signatures.
The exported `updates::repository` now names this configured repository.

For this IDE profile the names are **CyCodeIDE.exe**, **CyCodeIDEUpdater.exe**,
**CyCodeIDE-installed.flag** with the exact bytes `CyCodeIDE per-user installation\n`,
and **CYCODEIDE_UPDATE_TOKEN** (then GH_TOKEN / GITHUB_TOKEN). Releases come only
from MrMybal/CyCodeIDE; assets are `CyCodeIDE-VERSION-windows-x64.zip` or, for an
installed copy, `CyCodeIDE-VERSION-windows-x64-setup.exe`. The stored ZIP root is
`CyCodeIDE-VERSION/`. Shared library configuration does not create the GUI helper:
the host supplies its UI, close/consent flow, installer and normal restart target.
Automatic update transport remains Windows x64; no new workspace MCP operation
is added to the 252-operation catalogue.

New manifests retain `updateFormat: 1`, platform/version/files and add exact
`product` and `repository` fields. New products require both. Matching identifiers
also bind partial/prepared downloads, outcomes, pending installation/recovery
journals, prepared recovery data, retirement records and history. Different
products/repositories, foreign executable names, ZIP roots or installation markers
are refused before replacement; foreign cache records are rejected/protected.
SHA-256, exact managed file sets, path/case/type bounds, leases, rollback and disk
budgets remain in force. New profiles minimally require their own executable,
helper, LICENSE and COPYRIGHT in the manifest. Their host additionally validates
the actual SDK, fonts, providers and documentation it distributes. `extensions/`
and `tools/python/` remain permitted managed paths; `VERSION.md` and VERSION.fr.md
are accepted. Private data/runtime/working caches remain excluded.

With no identity options, the historical MrMybal/CyCode profile retains its old
names, installed.flag bytes, required managed files, token name and compatibility
with unlabelled legacy metadata. Explicit foreign identity fields are refused even
there. A fork repository is a distinct identity and cannot opt into this legacy
exception merely by using the same product basename.

The pure `app/scripts/release_package.py` helpers accept a frozen
`UpdateProduct('CyCodeIDE', 'MrMybal/CyCodeIDE')` via keyword `product`:
`package_path`, `required_files`, `payload`, `validate_payload` and `write_archive`.
Pass host-specific requirements via `required` to payload/validation/archive.
They preserve all supplied providers/MCP bytes, validate manifest hashes/identity
before opening an archive destination, and use the product ZIP root. Use
`update=True` for the stored portable ZIP consumed by native Apply. They do not
execute any packaged code. `windows_installer.compile_installer` accepts the same
`product`, `required` and an explicit host-owned `script` (.iss). It stages the
correct marker and supplies UpdateProduct, UpdateRepository and InstallMarker
preprocessor values. A nonhistorical product cannot silently use CyCode's installer
script/registry identity. The host owns its Inno Setup script and AppId.

The production CyCode and CyCodeIDE update libraries compile on Windows x64.
`verify_update_profiles.py --cmake <cmake>` checks configuration without a compiler,
network, application or fixture executable. Data-only package tests use in-memory
ZIPs. Native profile/foreign-manifest regressions are added to the existing test
suite but are not executed here. Network checks, actual installer/apply/recovery,
GUI acceptance, release publication and Linux/macOS execution remain separate.

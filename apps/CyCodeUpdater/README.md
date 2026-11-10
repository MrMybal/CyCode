# CyCodeUpdater

Optional companion application for the standalone Windows x64 CyCodeCore package.
Core and its SDK/MCP do not depend on this window or start it implicitly.

The per-user installer offers the updater as a separate component. Its Start menu
shortcut checks GitHub releases when opened. The separate, initially unchecked
Windows sign-in option checks once per day and offers the application when a newer
`core-v` release is available; it exits when there is nothing to offer.

The application shows the installed version and release notes. It downloads the
normal Setup only after confirmation, checks its size and SHA-256 against the same
release's checksum asset and GitHub digest when provided, then opens the installer.
Close Core/MCP clients before installation. There is no silent automatic replacement
of their running processes. The portable ZIP remains a separate manual download.

The updater uses the private Python shipped with Core and standard Windows controls;
it adds no CyUI, browser engine, Python package or persistent Windows service.
English is the default; `--language fr` selects French. The French installer creates
its shortcut with that option. The launcher accepts `--check` for a JSON-only check
and `--background` for the optional sign-in check.

Only releases of `MrMybal/CyCode` with the Core tag and complete Windows Setup/checksum
assets are considered. Older desktop application releases and incomplete drafts are
ignored. Preview installations can receive preview releases. A check requires an
Internet connection; installed editing/search services do not require this check.

Private repositories require existing GitHub access. The updater can use the
`CYCODE_GITHUB_TOKEN`, `GH_TOKEN` or `GITHUB_TOKEN` environment variable, or an existing
noninteractive Git credential for github.com. Credentials stay in memory; do not put
them in source files, package manifests or diagnostic reports. HTTP authentication
is sent only to api.github.com and removed before redirects to asset hosts.

Check metadata and verified downloads are stored in the user's CyCodeCore updates
cache. Disabling the sign-in task leaves the application available for manual use.
Uninstalling removes the registered sign-in entry. SHA-256 verification is an
integrity check against the release publisher, not a code-signing certificate or an
independent antivirus verdict.

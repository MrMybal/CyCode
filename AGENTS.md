# Repository instructions

Follow app/AGENTS.md for implementation work.

## GitHub Actions paused by the owner (2026-11-09)

- Do not launch, rerun, enable or add automatically triggered GitHub Actions workflows without new explicit authorization. The owner paused Actions to preserve the account's remaining minutes.
- CyCode, CyCodeIDE and CyCodeReader workflows are manually disabled on GitHub. Keep them disabled. Do not push changes that could trigger Actions; any separately authorized push must preserve this pause.
- Use source/data checks and production-only local builds within the existing antivirus restrictions. Record unexecuted platform/native checks honestly; do not replace CI with local native test launches or change antivirus/billing settings.


## Production CLI antivirus hold (2026-11-09, 16:07 Europe/Paris)

Kaspersky reported PDM:Trojan.Win32.Generic for the active worktree's
runtime/core-extraction-build/app/Release/cycode_cli.exe during actual MCP/Unreal
validation. This includes the production CLI, not only generated test programs.
Suspend local native execution AND rebuilding/retrying the flagged binary while
this detection remains unexplained. Do not create renamed/relocated variants,
restore quarantine, add exclusions or change protection settings to continue.
Source/data/documentation checks remain permitted. Preserve failures as evidence;
the interrupted MCP acceptance must not be reported as passing. Resumption needs
the owner to resolve the antivirus issue and explicitly reauthorise native runs.

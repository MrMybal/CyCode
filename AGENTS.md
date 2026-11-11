# Repository instructions

Follow app/AGENTS.md for implementation work.

## GitHub Actions paused by the owner (2026-11-09)

- Do not launch, rerun, enable or add automatically triggered GitHub Actions workflows without new explicit authorization. The owner paused Actions to preserve the account's remaining minutes.
- CyCode, CyCodeIDE and CyCodeReader workflows are manually disabled on GitHub. Keep them disabled. Do not push changes that could trigger Actions; any separately authorized push must preserve this pause.
- Use source/data checks and production-only local builds within the existing antivirus restrictions. Record unexecuted platform/native checks honestly; do not replace CI with local native test launches or change antivirus/billing settings.

# CyCode finalization

This ledger tracks completion evidence. CapabilityCoverage.md describes implemented
interfaces; Roadmap.md retains functional gaps. A successful compile does not prove
runtime behavior, and an unavailable external tool is a skipped test, not a pass.

## Integration baseline

- Published installer/updater source: 3aa0338 (version 0.2.3).
- Development changes from the preceding editor/navigation milestones are merged
  on top, including source/SDK/plugin contracts, pinned dependencies and EN/FR help.
- Both installed and portable update behavior, MSVC runtime packaging and notices
  are retained. Existing local development files remain untouched.

## Validation gates

| Gate | Current evidence | Required before completion |
|---|---|---|
| Windows/Linux/macOS core | Published baseline fails all three CI jobs | New matrix must pass; diagnose actual compiler/test errors |
| Production GUI and CLI | Published 0.2.3 builds on Windows | Build merged sources, then validate actual supported platforms |
| Source/schema checks | Syntax and release-authoring checks pass | Run all current bounded data contracts and verify generated descriptor |
| Offline help and website | Published export passes | Regenerate and verify merged EN/FR content, links and reader interactions |
| Installer and application updates | Windows package CI passes | Clean-machine installation/uninstall and full installed/portable update, cancellation and recovery acceptance |
| Editor and SDK/plugins | Implemented milestones are documented individually | Runtime source/selection/Undo/lifecycle/permission and graphical acceptance |
| Advanced development workflows | See Roadmap.md | Complete explicitly missing services and validate against real tools |

Local native fixture/application launches remain suspended while the reported
antivirus removals are unexplained. Remote CI explicitly runs its native suite;
local syntax/data checks and production-only builds do not change that policy.
No signing key, Perforce/Diversion server or Unreal installation is fabricated.

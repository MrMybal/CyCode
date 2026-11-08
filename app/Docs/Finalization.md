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
| Windows/Linux/macOS core | Complete c2c72ba matrix collected failures and explicit skips on all three platforms | Validate fixes with real installed language/debug backends; resolve remaining package/snippet/navigation/Git/server cases |
| Production GUI and CLI | Merged Windows Release app/CLI/helper/providers compile; 61 UI files pass strict Clang syntax | Complete Linux/macOS production builds and actual Linux render evidence |
| Source/schema checks | 233 descriptors / 466 schemas match; 26 data tests pass; source checks pass | Keep these checks passing with subsequent changes |
| Offline help and website | Merged 610-page / 6316-route reader and DOM controls pass | Keep exports current with final fixes; validate actual browser/desktop help handoff |
| Installer and application updates | Actual c2c72ba Windows runner Setup, payload hashes, busy/pending guards, French repair and uninstall/data retention pass | Full installed/portable online update, cancellation/recovery/restart and pristine-machine runtime acceptance |
| Editor and SDK/plugins | Implemented milestones are documented individually | Runtime source/selection/Undo/lifecycle/permission and graphical acceptance |
| Advanced development workflows | See Roadmap.md | Complete explicitly missing services and validate against real tools |

Local native fixture/application launches remain suspended while the reported
antivirus removals are unexplained. Remote CI explicitly runs its native suite;
local syntax/data checks and production-only builds do not change that policy.
No signing key, Perforce/Diversion server or Unreal installation is fabricated.


## Portability and integration fixes under validation — 2026-11-08

- Preserve the executable alias when detecting C/C++ compiler drivers; match
  Windows CI to the Visual Studio 2022 generator actually used by the build.
- Serialize 128-bit libc++ filesystem timestamps exactly in Unreal/MSBuild caches.
- Use concrete node IDs in outline sorting, accepted by strict Clang/GCC parsing.
- Convert copies of process logs at JSON/display boundaries; keep raw process,
  Git blob and language protocol bytes intact. Preserve valid UTF-8 and decode
  legacy Windows console lines without changing a global code page.
- Validate run-to-cursor references as typed integers rather than comparing signed
  JSON values against UINT64_MAX. Normalize trailing directory components in
  linked Git worktree references without resolving redirected metadata.
- Keep plugin permission refusals strict, correct fixture expectations and report
  exact failing snippet/navigation lines, package-load errors and Git phases.
- Bound CTest runs, retain JUnit/build/test evidence, and install real pinned
  backends for integration. Missing tools or unsupported adapters are not passes.
- Explicit runner-only checks cover Linux hidden rendering and actual Windows
  Setup/repair/uninstall; their final results remain separate gates.

The ledger does not mark advanced Roadmap.md work complete. No new release is
published from a failing development matrix.

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
| Windows/Linux/macOS core | Complete 6566784 matrix: 54 Windows, 55 Linux and 53 macOS native tests passed, with 4/2/3 failures and 0/1/2 explicit skips | Validate fixes with real installed language/debug backends; resolve remaining package/snippet/navigation/Git/server cases |
| Production GUI and CLI | Full production app/CLI builds pass on Windows, Linux and macOS; Linux EN graphite / FR light-grey hidden rendering passes; 62 UI files pass strict Clang syntax | Finish actual keyboard/Undo, font/icon resources, detached windows and platform interaction acceptance |
| Source/schema checks | 233 descriptors / 466 schemas match; 29 data tests pass; source checks pass | Keep these checks passing with subsequent changes |
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

## Second native correction pass — 2026-11-08

- Actual signed-package loading on Windows reported an inaccessible lease file.
  Native lease opens now use extended absolute Windows paths without renaming the
  existing lease identity. The existing core suite adds shared/exclusive long-path
  coverage; no new fixture executable is introduced.
- Git submodule ignore preflight previously forced literal pathspec magic on a
  command that accepts filenames. A read-only Git query reproduced exit 128;
  other Git commands keep their literal pathspec protection.
- POSIX terminal supervision now distinguishes traced stop/continue reports and
  EINTR from an actual exit, avoiding premature termination during debugger attach.
- NetCoreDbg launch arguments distinguish the PAL and CRT quote rules. The real
  Unicode argv/environment test is retained; unsupported argument lists fail
  explicitly instead of silently changing the target's argv.
- Snippet order validates signed and unsigned JSON separately. Negative-case
  fixture failures report the caller line. Host-validation refusals are asserted
  through the actual plugin exception contract. Rust semantic refresh retries are
  bounded and still reject changed document revisions.
- Byte-preservation fixtures write binary streams on Windows. The C++ action
  fixture requests a deterministic missing-semicolon compiler fix rather than
  relying on optional typo correction. MSBuild failure reports include payloads
  so missing navigation is actionable; that remaining failure is not marked fixed.
- The Linux-only CI keyboard step exercises the existing twelve Rider/VS/VS Code
  input scenarios in hidden windows. A data-only test proves runner checks refuse
  local execution before spawning. These newly scheduled keyboard cases have not
  yet passed. No native test/application was launched locally.

Production run 42443628690 and native run 42443628691 retain the exact evidence
for 1f56042. Subsequent fixes require a new run. The Linux captures expose fallback
font/icon rendering, which remains a real interface issue, despite pixel checks.

## Portable typography, keyboard reports and remaining contracts — 2026-11-08

The 6566784 matrix now executes C# with the pinned actual language server. Its
snippet catalogue, real C++ actions, Rust services and Windows/Linux managed
debugging pass. Linux retains extension permission-assertion and inherited-Git
fixture failures; Windows additionally exposes session/repository rejection cases;
macOS retains strict replay preservation and attach liveness cases. New source
locations and structured payloads keep those failures actionable. Do not mark them
fixed until the next run.

MSBuild correctly returned a navigable C2065 error after external MSB8029 warnings;
the fixture now finds the error by severity/code/path instead of assuming order.
Owned test TMP directories get a Git ceiling only during explicit CTest, preventing
unversioned fixtures from inheriting the parent CI checkout. No application Git
discovery or plugin permission policy is weakened.

Checked-in Roboto regular/bold, JetBrains Mono and a 104-glyph Codicons-derived
control face use the existing CyUI/ImGui host font APIs. No CyUI source, widget,
context or new runtime dependency is copied. Source hashes, semantic mapping and
original licenses accompany the resources; optional authoring reproduces all bytes.
Actual TTF cmap/hash tests pass, as do 62 strict Clang UI syntax checks and the
Windows production build. The new font rendering still requires runner evidence.
Some optional CyUI decorations remain constrained by its Windows-only icon flag.

The first Linux keyboard run exposed a report path relative to the child process's
different working directory; it now passes an absolute path. The twelve actual
keyboard cases must still execute. macOS production now also has to run the hidden
EN/FR renderer, rather than merely compile it. The documentation verifier rejects
changed, unreviewed source-guide hashes with normalized checkout line endings.

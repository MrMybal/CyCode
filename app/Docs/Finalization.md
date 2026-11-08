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
| Windows/Linux/macOS core | ab82791: Windows 64 passed; Linux/macOS 63 passed each / 1 MSBuild platform skip each; three extra Mac replay passes | Preserve real installed adapter coverage; repeat the previously intermittent Mac Git replay path; complete remaining Roadmap workflows |
| Production GUI and CLI | 32ff605: full production app/CLI builds on all three; eight Linux/Mac EN/FR renders; Linux 12 keyboard, 6 clangd information and detached-window/two-instance cases pass | Execute corrected Mac primary-modifier/keyboard/information/viewports cases; obtain actual Windows graphical and wider physical keyboard/IME/DPI acceptance |
| Source/schema checks | 233 descriptors / 466 schemas match; 38 data tests pass; source checks pass | Keep these checks passing with subsequent changes |
| Offline help and website | Merged 610-page / 6316-route reader and DOM controls pass | Keep exports current with final fixes; validate actual browser/desktop help handoff |
| Installer and application updates | Actual 32ff605 Windows runner Setup, payload hashes, busy/pending guards, French repair and uninstall/data retention pass | Full installed/portable online update, cancellation/recovery/restart and pristine-machine runtime acceptance |
| Editor and SDK/plugins | Pinned Windows Python/SDK authoring is implemented; both source/wheel authoring give the same reviewed payload | Execute installed official-client/shared-Core/permission/plugin/lifecycle acceptance and remaining source/selection/Undo/graphical workflows |
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

## Completed a176a31 runners and next correction pass — 2026-11-08

Native run 42447474984 reports 57 Linux passes with only Windows-specific MSBuild
skipped. Windows reports 55 passes and three failures: saved rooted Git paths,
open sibling-worktree protection and final owned-directory cleanup. macOS reports
55 passes, two skips and a 240-second composite Git timeout. Its actual native
debugger attach/detach now passes. No skip counts as successful execution.

Production run 42447475064 passes all three full app/CLI builds. Linux and macOS
both actually render the English graphite and French light-grey views with the
bundled fonts and mapped icon glyphs. Linux's twelve real keyboard scenarios pass,
including editing/Undo, VS/VS Code chords, all three searches, double Shift,
command palette, keymap settings, reopen and navigation history. Linux captures
were visually inspected. Windows Setup/repair/uninstall passes again, including
actual managed-file hashes and retained user data. No native launch occurred on
the user's machine. The local-browser tool rejects file://, so its actual offline
handoff remains unverified; export/DOM checks do not substitute for that gate.

The next corrections preserve these assertions: session paths reject drive-less
root directories on Windows; path containment compares equivalent slash spellings
before case-insensitive names. Owned test cleanup validates the exact new temporary
directory and uses Windows extended paths to remove long lease filenames. A failed
cleanup reports remaining relative names and is still a failure, without retry.

The existing VCS executable now exposes seven complete workflow groups to CTest,
so each keeps its assertions and bounded timeout without one aggregate 240-second
deadline hiding later repository checks. No additional test executable is added.
The official macOS NetCoreDbg ZIP installer restores executable metadata and repairs
a matching non-executable cache, rather than skipping the real managed-debug case.
A data-only ZIP contract proves extraction/cache behavior without running code.

Unsaved Unicode-insensitive literal search now uses ripgrep fixed strings through
the existing numeric snapshots, matching saved-text simple-fold behavior. Exact
and ASCII-only matching keep the in-memory route; queries are folded once and
line views avoid whole-line preview copies. Preview truncation retains UTF-8
scalars. Public-API regressions retain disk bytes, columns, cancellation and limits.
The new native path/search cases and macOS managed-debug execution still require
their next runner results. See Workspace.md for search bounds and semantics.

## Document observation ownership and extended graphical gates — 2026-11-08

Workspace disk reads now revalidate document identity/revision/dirty state and
the exact disk baseline before publishing a conflict. Superseded observations
request another poll without writing into the replacement lifetime. Reload of
a closed document fails before opening one, avoiding resurrection by a stale
asynchronous call. Existing core/workspace fixtures assert the public refusal,
no extra open event and stable identity of a successful automatic reload. Their
actual execution remains pending for this correction; no race stress result is
invented from syntax checks.

The macOS a176a31 English graphite capture was also visually inspected and is
readable with bundled fonts/icons. The next production workflow requests eight
theme/language captures and actual detached native editor/panel windows plus two
simultaneous Engine/bridge instances. These new runner-only scripts retain real
GUI behavior and source bytes. They refuse local execution; no native GUI/helper
or fixture has been launched on the user's machine. This new gate does not cover
CyToolsCore/MCP, a second physical monitor, IME or multiple physical DPI settings.

## 8490f94 results and the remaining distribution gap — 2026-11-08

Production run 42457286801 passes all three complete builds and repeats the real
Windows install/repair/uninstall and Linux/macOS EN/FR renders. The Linux keyboard
suite passes again. Native run 42457286802 passes 63 Linux tests with only MSBuild
skipped. macOS passes 62 tests, including actual .NET debugging, Unicode search
and every repository/worktree assertion; MSBuild is skipped. Its isolated Git
replay case fails while inspecting a merge reference. That refusal remains strict;
the next build adds the exact reference, child status and bounded UTF-8 output.
The Windows native job is still running at this entry; its result is not assumed.

The next graphical checks also use real clangd hover/signature responses through
Rider/Visual Studio/VS Code input, alongside eight theme/language captures and
native viewport/two-instance acceptance. Sources stay byte-identical; the Unicode
render fixture includes accents, Greek and Cyrillic. These added checks are pending.

Package inspection confirms that the current Windows payload includes the MCP
facade but requires separate installation of its Python/SDK environment. This
remains incomplete for the requested bundled integration. Preparation has verified
the local CyToolsCore 0.10.1 wheel against all 50 package files at published commit
05352e1d2f58f423fa2ad57b8eec48d2609f2626, apart from checkout line endings. The
32-wheel Windows/Python-3.13 candidate has 1,657 files / 41,877,451 bytes before
normal wheel installation mapping. The official Python 3.13.17 embedded archive
matches its published SHA-256. These are inventory/provenance checks only: no
new interpreter, downloaded package, native application or fixture was executed
locally, and the candidate environment is not yet packaged or runtime-validated.

## Windows 8490f94 completed — 2026-11-08

Native job 128658905967 completed: 63 passes and one core-fixture failure, with
no skips. Actual extensions/package loading and long-path cleanup pass. All
seven Git groups pass, including the previously refused open-sibling repair
and complete repository maintenance assertions. Session path rejection also
passes. The remaining failure is an occupied Save-as byte fixture: its text-mode
ofstream produced CRLF on Windows while the assertion expected LF. Save-as had
already correctly refused the existing file. The fixture now deliberately writes
and compares exact CRLF bytes on every platform, retaining the refusal assertion
and adding source locations. Its next Windows run must continue through the
remaining composite core/update/search contracts; those were not reached here.

The next correction batch retains the document-observation ownership fixes,
bounded macOS Git-reference diagnostics and extended actual desktop acceptance.
Production-only Windows compilation, MSVC syntax checks, data contracts and the
full local CyDoc export pass locally. No native application/helper/test was run
locally, no antivirus setting was changed, and no new public release is claimed.

## Complete native matrix and MCP distribution implementation — 2026-11-08

Native run 42459406339 passes all 64 Windows tests, and 63 each on Linux/macOS
with only MSBuild platform-skipped there. Windows now reaches the actual owned
update staging, rollback/recovery, draft and Unicode search contracts. Mac's
previous Git-reference refusal does not recur; it is not called root-caused from
one pass. The next workflow repeats only that path three times with diagnostics.

Production run 42459406453 passes Linux's eight EN/FR theme renders, twelve
keyboard cases, six actual clangd hover/signature cases and real detached editor/
panel plus two-instance Engine isolation. Mac's eight renders pass but the first
information-keyboard case fails: injected physical Control becomes ImGui Super
under macOS behavior. Scripted Primary now supplies the actual platform primary
modifier through the same event queue; explicit Control input remains available.
The next Mac run also executes the twelve editing/search/keymap cases. No result
is substituted for actual system/physical-keyboard/IME/DPI acceptance.

The Windows MCP runtime is implemented in release authoring: pinned official
CPython, all 32 wheel inputs, published SDK code and exact installed payload.
Provided-wheel and clean Git-source builds produce the same 1,567 files /
60,974,548 bytes. No venv or local settings are copied. Native startup and copied
MCP config prefer tools/python, with -B -X utf8 and no system-Python fallback.
The ordinary private development interpreter remains available for source builds.
Original dependency notices and the full CPython license remain in the package.

Six additional data-only cases pass for raw ZIP names, links/traversal, SDK source
identity, wheel layout/import isolation, pinned-inventory forgery and corrupt-cache
preservation. Their malformed ZIP writer preserves raw names before CPython's
Windows separator/NUL normalization. The loader rejects original malformed names.
Actual installed MCP/official-client execution is scheduled only on Windows CI,
after repair and before uninstall. It is still pending. It checks shared Core,
private identities/jobs, Unicode live edits, stale guards, plugins and shutdown.
No new interpreter, application, native fixture or SDK Runtime was run locally.
The SDK wheel backend was rebuilt with isolated pure authoring tools only.

## Windows actual graphical acceptance prepared — 2026-11-08

The next production workflow includes Windows rendering and editing/information/
viewport acceptance using an isolated runner-only software renderer fixture.
The exact upstream Mesa archive and all three selected x64 DLLs are hashed;
local inspection streamed members to memory only. No driver was written/loaded
locally or added to the product payload. The CI preparer guards before all
filesystem/network/process actions. Test reports expose actual GL metadata and
require llvmpipe on Windows. Font checks retain Windows system fonts and require
real glyph availability; keyboard source bytes are explicit LF on every host.
The new Windows path remains pending runner execution. It does not imply physical
keyboard/IME/DPI/multi-monitor or real-GPU performance verification.

## Shared semantic desktop acceptance prepared — 2026-11-08

Existing native GUI navigation/highlight/refactoring scripts now have explicit
runner guards, production executable selection and separate owned fixtures.
The next matrix requests three Unicode definition/reference cases, two actual
clangd color views and fifteen keyboard-driven symbol/rename/format/apply/Undo
cases. Six diff themes plus French are retained for later visual inspection.
Actual command-availability waits bound startup/apply/Undo without bypassing
input handlers. C# managed semantic acceptance remains separate; no backend or
process is simulated and no new native fixture executable is added. This is
prepared coverage, still awaiting actual remote results.

## macOS scripted chord cadence correction — 2026-11-08

Production ab82791 reaches the actual Rider editing case, then Visual Studio's
comment chord expires and dispatches Copy. At the observed roughly 24 seconds
per 115 frames, the authored nine-frame gap can exceed the unchanged 1.5-second
keymap deadline. Double Shift's unchanged 0.4-second window has the same fixture
issue. Explicit scripted input now declares a fixed 60 Hz ImGui delta and records
the actual maximum SDL frame delta; ordinary physical input and native deadlines
retain real clocks. Actual focus/queue/commands/services/Undo remain exercised.
This is a deterministic cadence fixture, not a physical timing/performance claim.
The corrected keyboard/information/refactoring matrix remains pending.

## ab82791 actual native matrix and installed MCP progress — 2026-11-08

Native run 42462705540 passes all 64 Windows cases and 63 on each POSIX platform
with only MSBuild skipped. macOS additionally passes the targeted replay case
three times (135.66 / 136.38 / 127.18 seconds); the intermittent refusal is not
called root-caused. Production 42462705649 builds all platforms and passes the
extended Linux desktop suite. Mac's primary-modifier editing passes; its next
multi-stroke case exposes the slow-frame cadence issue described above.

Windows actually installs and repairs the newly composed package, runs the
bundled CPython 3.13.17 and imports the exact installed dependencies and SDK.
Two official MCP clients share the real Core service. Catalogue discovery,
Unicode native live editing, read-only denial, private job isolation and stale
revision refusal reach their assertions successfully. The subsequent plugin
inventory job fails strict output validation: native `snippetCatalogues` was
missing from the published output schema. This is an actual contract bug, not a
runtime dependency/import failure. The schema now declares typed catalogue
summaries while preserving closed output objects; older hosts may omit this
additive property. One new data regression covers valid/absent inventories and
malformed/missing/unknown properties, giving 38 total data-only checks.

The corrected full installed SDK/plugin/shutdown/unchanged-runtime/uninstall
sequence remains pending. No failure is presented as a completed installer/MCP
pass, no new release was published, and no local native runtime was launched.

Contract regeneration also exposed a stale hard-coded 0.2.2 in write_manifest.
CyDoc's version consistency check stopped the build before commit/publication.
The generator now preserves the descriptor's release-owned version and writes
stable LF. A data-only regression regenerates a future version twice and asserts
version preservation and byte-identical output; the total data suite is 38 cases.

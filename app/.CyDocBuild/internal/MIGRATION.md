# Local CyDoc update — 2026-11-06

Private maintenance record; this directory is not content or a distribution input.
The initial migration was prepared locally. The owner subsequently authorised
the Reader migration, source pushes and the CyCode 0.2.2 update. Publication
receipts remain under the ignored runtime audit directory.

## Replaced integration

The former `app/documentation` source directory collided with the required
`app/Documentation` output on Windows. Its original files are retained in the
ignored `.cache/legacy-integration` during this local migration; tracked sources
now live in `.CyDocBuild`. The old builder/source importer/dependency resolver
and public developer-maintenance page were removed. Compatibility script names
delegate to this workspace. Technical project instructions remain in `app/Docs`
and `AGENTS.md`; none is imported into the human manual.

CyDoc is pinned to 6ddf9a582defb2f11fbb3f992afef6f98dcadc93 and installed from an
officially built local npm-pack archive, with a committed npm dependency lock.
The upstream developer checkouts have not been changed. Only image and gallery
are selected; gallery image children and accessible static SVG diagrams need no
graph/mesh/Three runtime. All relevant notices are copied into DocData/licenses.

The manual has 40 task guides, 21 service groups and 227 operation pages in each
locale (576 localised pages). English is the source/default; all French pages
have matching page and explicit heading IDs and translation revision metadata.
Public operation schemas come from CyTool.json through the public structured API
importer. Plugin C ABI signatures/examples are authored against PluginAPI.h.
Implementation/source-file browsing, imported audits, test histories and build
maintenance instructions are no longer part of navigation or search. DocAI.md
stays separate. Legacy page IDs are aliases; the official legacy-export migration
creates the hash/query-preserving index.html redirect.

Native panel/command help targets Documentation.html. CMake, install and portable
packaging copy the complete Documentation tree. The packager excludes the private
workspace and no longer ships the old app/Docs source collection as the manual.
Only the shared Pages composer in the CyCodeReader checkout was adapted to accept
the new CyCode export; Reader's own application and manual were not migrated.

## Local verification

- Clean CMake configuration with MSVC/Windows x64 and BUILD_TESTING=OFF.
- Release CyCode target compiled successfully; no native app/test/helper started.
- Official CyDoc check in a fresh private staging application; EN/FR complete,
  no missing/outdated/untracked translations, local links/assets checked.
- 5,968 internal routes verified; 25 native topic destinations resolve.
- DOM fixture passed hierarchy, translated API/search, Ctrl+K, six themes,
  accessible controls, media enlargement/Escape, gallery and reading positions.
  Script/resource loading disabled; zero network requests.
- Ownership checks rejected unknown files and modified generated files without
  replacing them. A complete documentation ZIP round-trip preserved every hash.
- Existing Reader export and new Code export composed into one local Pages tree.
- Python/JSON source checks and Git whitespace checks passed.

## Remaining acceptance

The automated browser explicitly rejects file:// under its URL security policy.
No server, raw browser command or alternate browser surface was used to bypass
that restriction. A Codex request to show the local reader was queued before
this rejection; actual visual file-origin rendering could not be confirmed.
Open app/Documentation/Documentation.html manually to inspect the real layout,
long captions, touch, DPI, tooltips and file-origin storage in the chosen browser.
Native F1/menu interactions were not executed under the ongoing antivirus test
suspension. Linux/macOS builds and real browsers were not tested.

The stock export shell initially declares French in its static HTML template;
the reader sets the actual document language from the selected/default locale
when mounted. Default English and explicit French routes passed component checks.
No generated HTML or upstream library was patched to hide that upstream detail.

Release packaging intentionally requires a clean committed checkout. The exact
documentation selection/archive was validated separately before committing.
The authorised follow-up publishes both manuals and CyCode 0.2.2 after source,
export, payload and Gitleaks verification. Reader's own release version is not
changed by this documentation migration.

## Coverage and contextual-help completion — 2026-11-07

Ten additional task guides in each language cover physical files/trash/monitoring,
project creation, multilanguage tasks, structure/references/hierarchies, diagnostics,
execution controls, data/instruction breakpoints, update storage/history, custom
VCS providers and the offline reader. Existing sections, figures, code examples
and operation contracts remain. Personal snippets are no longer incorrectly
listed as unavailable. Verification histories were removed from human prose;
implementation limitations remain explicit. All 88 technical source guides remain
in Docs/ with stable per-guide destinations and source fingerprints in coverage.json.

HelpTopics.h is the one native panel/Context-help catalogue. Its 26 topics now
include section anchors, Occurrences and precise resource/task destinations.
Document windows retain editing help; contributed panels use extension help.
verify.py checks actual panel IDs and both languages against the compiled reader.
The public neutral API importer remains the appropriate source-data integration:
233 real operation contracts, separately reviewed French summaries and complete
JSON schemas, without raw implementation/source browsing or an invented C++ index.

The official Git checkout at e038c474577e1f305df3eba421f2b9c03b0f1d8d was inspected
with all four AI guides. Its reader/compiler/native helper match the existing
6ddf9a582defb2f11fbb3f992afef6f98dcadc93 pin, which is retained. Image/gallery only;
new flow diagrams are accessible static SVG. Existing logo, assets, licences,
legacy index redirect and both build/distribution copies remain. No other app or
upstream source is edited, no server is introduced, no publication is performed.

Completed checks: 610 localized pages, 6,314 internal routes, 88 source-guide
mappings, 26 native help destinations; full EN/FR revisions and anchors; permanent
DOM fixture for old and new chapters, API/local search, six themes, media/gallery,
Escape, accessible controls, reading positions and legacy routes, without requests.
Three identical 19-file exports and a documentation ZIP round-trip match hashes.
Source checks pass 56 Python / 24 JSON files. MSVC /Zs and Clang syntax-only pass
all three affected UI units. The Windows x64 Release GUI build succeeds with
BUILD_TESTING=OFF, without reported warnings. No project executable/test is run.

The current browser-tool attempt to open file:// was rejected by its URL policy,
which permits only HTTP(S). No server, alternate surface or indirect browser
execution is used. Real local rendering, native F1/menu/browser handoff, touch/DPI,
file-origin storage and Linux/macOS acceptance still need target checks. The
export itself remains local file:// with no CDN or runtime network dependency.

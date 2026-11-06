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

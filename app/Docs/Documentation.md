# Local documentation with CyDoc

CyCode's Help menu, action catalogue and panel help buttons open the complete
local manual in the system browser. `Ctrl+F1` opens the manual, `Shift+F1` opens
context help. `F1` opens help in the Rider and Visual Studio profiles; the VS Code
profile retains its existing F1 action palette. Quick documentation for source
symbols remains a language-service action.

## Generate

From the repository root, with Python 3.11+, Git access to the public
CyDoc repository, and Node.js 22+ / npm:

```text
python app/scripts/build_documentation.py
python app/scripts/verify_documentation.py runtime/documentation/site
```

An optional `--cydoc-source PATH` accepts a clean local checkout at the exact
revision in `app/documentation/cydoc.lock.json`. Preparation uses a separate Git
archive or detached cache under `runtime/deps`; it never changes that checkout.
The lock pins CyDoc 0.1.0 at
`774f2a7eabf7dd181a98ffc5add115b580b67f42`. The generator uses CyDoc's checked
export and resource providers, without copying or changing its viewer sources.

Open `runtime/documentation/site/index.html` directly from disk. The exported
reader uses classic local scripts, selected media bundles, CSS, compiled page
data and local images. No server, CDN, account, Node installation or network is
required to read it. Ordinary external source/licence links are optional.

## Build and distribution

GUI CMake builds generate documentation by default. `CyCode_documentation`
publishes the full export under the build directory's `help/` and next to the
executable as `app/help/`. The Windows packager includes this complete tree and
the documentation lock. Installation copies help to `app/help/` too.
`CYDOC_DIR` is the optional clean checkout override. A fresh dependency
fetch requires GitHub and npm access; prepared caches work offline.

`CYCODE_BUILD_DOCUMENTATION=OFF` is an explicit development-only opt-out: help
actions report that help was disabled. Core-only builds do not need CyDoc or
Node. A distributable GUI package must contain the generated help export.
The normal app build keeps `BUILD_TESTING=OFF`.

## Authoring and migration

- `app/documentation/manual/en` and `fr`: paired user guides, stable page IDs and
  explicit section IDs. Increase the English `revision` and update French
  `sourceRevision` after reviewing the matching translation.
- `app/documentation/assets`: layout illustrations and favicon. These are
  labelled diagrams, not screenshots of an untested desktop session.
- `app/documentation/project.json`: imported original documents, local media,
  page aliases and the explicit distributable source allow-list.
- Existing README, French README, technical guides, code examples and notices
  remain authoring inputs. Markdown links are rewritten to stable CyDoc routes;
  source links outside the small lexical index point to the recorded source
  revision. No runtime/dependency tree is indexed.
- API reference comes from the real `app/CyTool.json` operation schemas. CyDoc's
  neutral API import and C++/Python source index do not parse a compiler AST.
  Bilingual native help annotations demonstrate source-driven documentation.

The export contains `migration-report.json`, including original-file/page
mapping and unresolved legacy references, and `translation-report.json`.
English is the default/fallback language; original English technical pages and
operation schemas are not presented as completed French translations. Historical
validation pages retain their dates and are marked as historical evidence.

Only `image` and `gallery` are selected: diagrams, logos and collections need
these. Static `diagram` blocks compile to SVG, so `graph` is unnecessary. No
video, GIF, mesh or Three.js runtime is shipped. CyDoc and its selected compiler
dependency notices are retained in the exported `licenses/` directory.

The upstream reader provides themes, keyboard navigation, in-window media
enlargement and browser-local reading positions. Initial theme follows the
reader/system preference; the stock exporter does not propagate the app's CyUI
theme or density. File-origin storage behavior varies by browser. Windows help
uses the platform file-URL converter; extended paths are not supported by that
API. Linux/macOS URL branches and desktop help interaction need platform testing.

## Verification scope

The optional component check uses the pinned CyDoc development dependencies:

```text
node app/scripts/verify_documentation_dom.mjs runtime/documentation/site runtime/deps/cydoc-774f2a7
```

It loads compiled manual data into a DOM fixture with resource/script loading
disabled. It checks guide routes, language fallback, search, light/dark themes,
gallery navigation, in-window enlargement/Escape and reading/navigation position
restoration. This is not a browser or native desktop acceptance test. Real file-URL
rendering, menu clicks, DPI, touch and Linux/macOS builds remain separate checks.
The stock reader’s MIT badges describe CyDoc; the application remains GPL-3.0-only.

The migration was compiled in Windows Release. Export integrity/link checks and
DOM component fixtures passed. Native tests were not executed. The source-comment
extractor in the pinned CyDoc version does not propagate translation revision
metadata: the bilingual `help-routing` annotation is explicitly reported as
untracked. Authored guide translations have matching explicit revision metadata.

The repository-root launcher receives the same generated export in `app/help/`.
These generated directories are ignored by Git; edit the authoring sources instead.

## Published reader

[English documentation](https://mrmybal.github.io/CyCodeReader/cycode/#/en/overview) · [Documentation française](https://mrmybal.github.io/CyCodeReader/cycode/#/fr/overview)

`.github/workflows/documentation.yml` regenerates the same complete export on
every main-branch push or manual dispatch, using the pinned CyDoc revision and
only the image/gallery modules. Node.js 22 and Python 3.11 are build tools; the
published reader needs neither. Hash routes preserve pages, sections, languages
and media under the Pages project URL. Offline `file://` help remains bundled.
Only the verified export is uploaded, never application data or dependency caches.

Regenerate and verify the publication artifact from the repository root:

```text
python app/scripts/build_documentation.py --output runtime/pages
python app/scripts/verify_documentation.py runtime/pages
```

The private source repository cannot use Pages on the current GitHub plan.
The existing public `MrMybal/CyCodeReader` repository hosts both manuals. Its
`cycode-docs` branch contains only the generated CyCode export in `site/`.
The CyCode source workflow updates that branch using `DOCS_DEPLOY_KEY`, a write
deploy key scoped to CyCodeReader, with no access to private CyCode source.
The Reader workflow builds its own manual from main, checks the generated branch,
and composes the Pages artifact: Reader at `/`, CyCode at `/cycode/`.
No extra repository, private source checkout or local application data is needed.
The publication helper refuses unknown or locally modified generated files.
Keep the deploy secret configured when transferring CI.

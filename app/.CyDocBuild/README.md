# CyCode documentation toolchain

The user manual is authored here, separately from developer notes in `app/Docs`.
Only the generated `app/Documentation/` tree is distributed. Never package this
workspace, `internal/`, caches, source checkouts or dependency installations.

From the repository root, with Python 3.11+, Node.js 22+, npm and Git:

```text
python app/.CyDocBuild/build.py
python app/.CyDocBuild/verify.py app/Documentation
node app/.CyDocBuild/verify_dom.mjs app/Documentation
```

Open `app/Documentation/Documentation.html` directly with a browser. It needs no
server, CDN, Node or network connection. `DocAI.md` is separate assistant guidance;
`DocData/` contains all reader resources, media and notices. `index.html` preserves
old page links through CyDoc's official legacy-export migration.

`prepare.py` follows the official local-package distribution: clone the exact
commit in `cydoc.lock.json`, `npm ci`, build, `npm pack`, then install that archive
under this workspace with `npm ci` and the committed dependency lock. CyDoc is
not installed from an npm-registry package.
Initial preparation needs repository/npm access; an installed cache works offline.
The lock pins `6ddf9a582defb2f11fbb3f992afef6f98dcadc93`. No upstream checkout is edited.
The integration/API/authoring guides were also compared with upstream revision
`e038c474577e1f305df3eba421f2b9c03b0f1d8d`: the reader, compiler and native route
helper are unchanged, so this migration retains its reproducible runtime pin.

`cydoc.config.json` selects the image and gallery modules only. The gallery uses
image children. Static flow diagrams compile to SVG without graph or Three.js.
The official stock reader supplies top search/Ctrl+K, the wider sidebar, nested
navigation, tooltips, six themes and in-window media enlargement. App theme/DPI
preferences are not injected into the system browser.

Paired `manual/en` and `manual/fr` pages use identical page/heading IDs. Increase
English `revision` and French `sourceRevision` together after reviewing the
translation. Titles, descriptions and media explanations must also be translated.
The export rejects incomplete EN/FR. API keys and code examples stay technical.

`api.py` takes public JSON Schemas from `app/CyTool.json` and reviewed French
summaries from `api-fr.json`. `generate_api.mjs` calls the public `cydoc/api-import`
adapter to produce private Markdown inputs. No implementation/source tree is
indexed or distributed. Plugin C ABI examples are checked against
`app/include/cycode/PluginAPI.h`.

`coverage.json` maps every retained `app/Docs` guide to a stable user task or
reference section and its legacy address. The recorded source hashes identify the
reviewed inputs; they are not automatically equated with translation quality.
Developer/build/validation histories remain outside the human manual. New guide
files require an explicit coverage entry. The verifier also reads `src/HelpTopics.h`
and checks all built-in panel destinations and anchors in English and French.
Native Context help and panel buttons use that one catalogue; document windows
route to editing and contributed panels to extension help.

Both CyDoc `build` and `check` write output. Our wrapper calls `check` in a fresh
private staging application, verifies the result and copies only owned files to
`app/Documentation/` and optional repeated `--output PATH` destinations. Changed
or unknown destination files block replacement. Generated files are not edited.

GUI CMake builds depend on `CyCode_documentation`, which also copies the manual
to `BUILD/Documentation` and beside the executable under `app/Documentation`.
Installation and the Windows packager include that complete folder. Core-only
builds need no documentation toolchain. `CYCODE_BUILD_DOCUMENTATION=OFF` is an
explicit development opt-out; `BUILD_TESTING` stays OFF for ordinary builds.
The scripts under `app/scripts/` retain compatibility entry points only.

The documentation GitHub workflow builds and verifies this same export before
updating the generated `cycode-docs` branch of the existing CyCodeReader repository.
Its shared Pages composer must understand the `Documentation.html`/`DocData`
format. Do not include local data or the private workspace in that publication.
Public URLs remain compatible through `index.html`. This local update is not a
request to push or deploy.

Component checks use Happy DOM from the pinned generator's build dependencies,
disable resource/script loading and make no network requests. They exercise
routes, language/search, hierarchy, six themes, media and reading positions.
They do not establish real-browser layout, file-origin policies, touch/DPI or
native F1 behaviour. Record those checks and platform limits under `internal/`,
never in the human manual.

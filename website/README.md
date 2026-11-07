# CyCode showcase

Static HTML/CSS/JavaScript for local preview and GitHub Pages deployment.
It has no package installation, framework runtime, CDN, analytics or server-side
dependency. The application documentation remains the official CyDoc reader.

From the repository root, with Python 3.11+, Node.js 22+, npm and Git available:

```text
python website/build.py
python website/verify.py runtime/showcase
python -m http.server 4387 --bind 127.0.0.1 --directory runtime/showcase
```

Open `http://127.0.0.1:4387/` or use `?lang=fr`. Stop the preview with Ctrl+C.
The complete generated directory also works as static files. Once the CyDoc
manual has been generated, `--skip-docs-build` reuses that verified export.
The pinned CyDoc repository is public. The documentation preparation uses Git
and npm; no additional repository credential is needed by the Pages workflow.

Only `runtime/showcase/` is a Pages payload. Its `.nojekyll`, `index.html`, `Documentation.html`,
`assets/`, `manual/`, `LICENSE.txt` and ownership manifest belong together. Do not serve or
publish the repository root, `data/`, authoring inputs or other `runtime/` folders.
`.github/workflows/pages.yml` rebuilds the website and complete CyDoc export on
relevant pushes to `main` or manual dispatch. Only the verified output is uploaded
to Pages; the deploy job uses the `github-pages` environment and scoped Pages/OIDC
permissions. No native application or native tests are run by this workflow.
The existing publication to CyCodeReader is retained for older documentation links.

Public entry points: <https://mrmybal.github.io/CyCode/> (English by default),
<https://mrmybal.github.io/CyCode/?lang=fr> (French), and
`Documentation.html#/en/overview` or `Documentation.html#/fr/overview` for the manual.
The stable manual entry preserves the requested language and section while
redirecting to the complete, content-versioned CyDoc reader.

`content.json` contains complete English/French copy. English is the default;
the selected URL language also controls documentation links. Native language
identifiers and screenshot UI retain their original spelling. The gallery uses
keyboard tabs; the in-page image dialog closes with Escape and returns focus.

Scripts, CSS and images use SHA-256-derived filenames. Each complete, unchanged
CyDoc export goes under a content-versioned directory, renewing its own resource
URLs without patching generated viewer files. Unknown/modified generated output
blocks replacement instead of being erased. Source templates remain separate.

The screenshots are real application captures of illustrative development
workspaces. They are not mockups or proof of current runtime acceptance. The page
states the development status and the Windows-only published package; Linux/macOS
validation, backend-dependent services and incomplete feature coverage remain
explicit. The public release page is the download entry point so it can follow
future releases without hardcoding an old package. Demo source files are supplied
for future captures; no compilation or test execution is required for this site.

The site's source, existing logo and application screenshots retain CyCode's
GPL-3.0-only licence. The copied manual includes its existing dependency notices.

Local verification (2026-11-07, Chromium on Windows): generated with the complete
CyDoc build; verified all local references, bilingual content and asset hashes;
checked desktop layouts at 1280×720, 1366×768 and 1440×900 and mobile layouts at
360×800 and 390×844. The hero image remained entirely visible at the tested
desktop sizes. Gallery keyboard controls, image enlargement/Escape/focus return,
FAQ keyboard activation, chapter navigation, translated documentation links,
CyDoc search/illustrations/language switching and the public release destination
were checked in the browser over loopback HTTP. Browser console checks were clear.
This does not validate native application behavior, physical mobile devices,
Safari/Firefox, Linux/macOS or file-origin browser loading.

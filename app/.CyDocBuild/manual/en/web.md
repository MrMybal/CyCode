---
id: web
title: Web and Node.js
description: Edit web files and run scripts declared by the project.
section: Work in CyCode
order: 170
parent: languages
revision: 2
---

## Editing services {#web}

JavaScript/JSX, TypeScript/TSX, HTML and CSS have dedicated syntax handling. Configure the TypeScript, HTML or CSS language server for completion, diagnostics and supported navigation. HTML embedded script/style regions use the corresponding syntax parsers.

## npm tasks {#npm}

Open `package.json` or its project folder, then use Project tasks. Select an existing build/test/script name. Install dependencies is explicit: npm uses `ci` with a lockfile, otherwise `install`. Project lifecycle scripts execute according to npm configuration.

Run long-lived development scripts in the integrated terminal and stop them there. CyCode does not provide browser preview, browser JavaScript debugging or a replacement for the framework's own tooling.

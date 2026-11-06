---
id: plugins
title: Plugins
description: Plugins — local application manual
section: Guides
order: 50
revision: 1
---

## Extensions {#extensions}

Open the extension manager to review available plugins and options. Native contracts are versioned and use the documented UTF-8/JSON formats. Callbacks and libraries must remain alive during calls; STL/ImGui objects do not cross the ABI boundary.

## Develop a plugin {#development}

Start with the preserved Plugins reference and indexed PluginAPI header. Check this application’s ABI version. Shipped examples demonstrate the actual contract; a descriptor alone does not implement behavior.

```diagram
{"type": "dependency", "title": "Extension ownership / Responsabilités", "nodes": [{"id": "ui", "label": "GUI / menus", "x": 10, "y": 10}, {"id": "native", "label": "Native application", "x": 280, "y": 10}, {"id": "plugin", "label": "Versioned plugin ABI", "x": 550, "y": 10}, {"id": "sdk", "label": "CyToolsCore SDK / MCP", "x": 280, "y": 150}], "edges": [{"from": "ui", "to": "native"}, {"from": "native", "to": "plugin"}, {"from": "native", "to": "sdk"}]}
```

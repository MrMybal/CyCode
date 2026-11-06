---
id: tour
title: Visual tour
description: Visual tour — local application manual
section: Guides
order: 15
revision: 1
---

## Interface landmarks {#interface}

The following layout map explains application regions. It is an explanatory diagram. Enlargement and zoom remain inside the reader.

```image
{"id": "interface-image", "src": "assets/interface-map.svg", "alt": "CyCode interface map / plan explicatif", "controls": "minimal"}
```

```gallery
{"id": "interface-gallery", "controls": "minimal", "items": [{"type": "image", "src": "assets/interface-map.svg", "alt": "CyCode layout map"}, {"type": "image", "src": "assets/application.png", "alt": "CyCode application logo"}]}
```

## Local help flow {#help-flow}

```diagram
{"type": "flow", "title": "Offline help / Aide locale", "nodes": [{"id": "app", "label": "CyCode", "x": 10, "y": 10}, {"id": "url", "label": "file:// + stable route", "x": 280, "y": 10}, {"id": "reader", "label": "CyDoc reader", "x": 550, "y": 10}, {"id": "local", "label": "Local data / assets", "x": 550, "y": 150}], "edges": [{"from": "app", "to": "url"}, {"from": "url", "to": "reader"}, {"from": "reader", "to": "local"}]}
```

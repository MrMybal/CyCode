---
id: tour
title: Visite illustrée
description: Visite illustrée — manuel local de l’application
section: Guides
order: 15
sourceRevision: 1
---

## Repères de l’interface {#interface}

Le plan suivant explique les zones de l’application ; il s’agit d’un schéma de disposition. L’agrandissement et le zoom restent dans le lecteur.

```image
{"id": "interface-image", "src": "assets/interface-map.svg", "alt": "CyCode interface map / plan explicatif", "controls": "minimal"}
```

```gallery
{"id": "interface-gallery", "controls": "minimal", "items": [{"type": "image", "src": "assets/interface-map.svg", "alt": "CyCode layout map"}, {"type": "image", "src": "assets/application.png", "alt": "CyCode application logo"}]}
```

## Parcours local de l’aide {#help-flow}

```diagram
{"type": "flow", "title": "Offline help / Aide locale", "nodes": [{"id": "app", "label": "CyCode", "x": 10, "y": 10}, {"id": "url", "label": "file:// + stable route", "x": 280, "y": 10}, {"id": "reader", "label": "CyDoc reader", "x": 550, "y": 10}, {"id": "local", "label": "Local data / assets", "x": 550, "y": 150}], "edges": [{"from": "app", "to": "url"}, {"from": "url", "to": "reader"}, {"from": "reader", "to": "local"}]}
```

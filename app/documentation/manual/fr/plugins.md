---
id: plugins
title: Extensions
description: Extensions — manuel local de l’application
section: Guides
order: 50
sourceRevision: 1
---

## Extensions {#extensions}

Ouvrez le gestionnaire d’extensions pour examiner les plugins présents et leurs options. Les contrats natifs sont versionnés et utilisent les formats UTF-8/JSON documentés. Les callbacks et leurs bibliothèques doivent rester vivants pendant les appels ; les objets STL/ImGui ne traversent pas la frontière ABI.

## Développer un plugin {#development}

Commencez par le guide Plugins conservé dans les références et l’en-tête PluginAPI indexé. Vérifiez la version ABI attendue par cette application. Les exemples livrés montrent le contrat réellement disponible ; un descripteur seul n’implémente pas une fonction.

```diagram
{"type": "dependency", "title": "Extension ownership / Responsabilités", "nodes": [{"id": "ui", "label": "GUI / menus", "x": 10, "y": 10}, {"id": "native", "label": "Native application", "x": 280, "y": 10}, {"id": "plugin", "label": "Versioned plugin ABI", "x": 550, "y": 10}, {"id": "sdk", "label": "CyToolsCore SDK / MCP", "x": 280, "y": 150}], "edges": [{"from": "ui", "to": "native"}, {"from": "native", "to": "plugin"}, {"from": "native", "to": "sdk"}]}
```

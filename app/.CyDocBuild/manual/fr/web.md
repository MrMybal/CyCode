---
id: web
title: Web et Node.js
description: Modifiez les fichiers web et lancez les scripts déclarés par le projet.
section: Travailler dans CyCode
order: 170
parent: languages
sourceRevision: 2
---

## Services d’édition {#web}

JavaScript/JSX, TypeScript/TSX, HTML et CSS possèdent un traitement de syntaxe dédié. Configurez le serveur TypeScript, HTML ou CSS pour la complétion, les diagnostics et les navigations prises en charge. Les zones script et style intégrées au HTML utilisent les analyseurs de syntaxe correspondants.

## Tâches npm {#npm}

Ouvrez `package.json` ou son dossier, puis utilisez les tâches de projet. Sélectionnez un nom de script de compilation, de test ou d’exécution existant. L’installation des dépendances est explicite : npm utilise `ci` avec un fichier de verrouillage, sinon `install`. Les scripts de cycle de vie suivent la configuration npm du projet.

Lancez les scripts de développement persistants dans le terminal intégré et arrêtez-les depuis ce terminal. CyCode ne fournit ni aperçu de navigateur, ni débogage JavaScript de navigateur, ni remplacement des outils du framework.

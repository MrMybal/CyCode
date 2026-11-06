---
id: api-language
title: Services de langage
description: Opérations publiques pour Services de langage.
section: API et extensions
parent: api-links
order: 103
sourceRevision: 2
---

Opérations publiques pour Services de langage.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`language_hierarchy_prepare`](doc:operation-language-hierarchy-prepare-language-hierarchy-prepare) — Préparer une hiérarchie de types ou d’appels auprès du serveur de langage.
- [`language_hierarchy_expand`](doc:operation-language-hierarchy-expand-language-hierarchy-expand) — Développer un élément de hiérarchie précédemment obtenu.
- [`language_hierarchy_status`](doc:operation-language-hierarchy-status-language-hierarchy-status) — Consulter le résultat d’une requête de hiérarchie de langage.
- [`language_symbols`](doc:operation-language-symbols-language-symbols) — Rechercher des symboles dans l’espace de travail.
- [`language_symbol_resolve`](doc:operation-language-symbol-resolve-language-symbol-resolve) — Résoudre les détails d’un symbole précédemment retourné.
- [`language_document_symbols`](doc:operation-language-document-symbols-language-document-symbols) — Lister les symboles du document courant.
- [`language_rename`](doc:operation-language-rename-language-rename) — Préparer le renommage d’un symbole via le serveur de langage.
- [`language_format`](doc:operation-language-format-language-format) — Préparer le formatage d’un document via le serveur de langage.
- [`language_configure`](doc:operation-language-configure-language-configure) — Configurer un serveur de langage pour l’espace de travail.
- [`language_preferences`](doc:operation-language-preferences-language-preferences) — Lire ou modifier les préférences des services de langage.
- [`language_preferences_forget`](doc:operation-language-preferences-forget-language-preferences-forget) — Supprimer les préférences de langage enregistrées pour la portée choisie.
- [`language_status`](doc:operation-language-status-language-status) — Consulter l’état des serveurs de langage.
- [`language_highlight`](doc:operation-language-highlight-language-highlight) — Obtenir la coloration syntaxique ou sémantique disponible pour un document.
- [`language_complete`](doc:operation-language-complete-language-complete) — Demander des propositions de complétion à une position du document.
- [`language_completion_resolve`](doc:operation-language-completion-resolve-language-completion-resolve) — Résoudre les détails d’une proposition de complétion.
- [`language_completion_prepare`](doc:operation-language-completion-prepare-language-completion-prepare) — Préparer l’application d’une proposition de complétion sélectionnée.
- [`language_code_actions`](doc:operation-language-code-actions-language-code-actions) — Demander les actions de code disponibles pour une plage du document.
- [`language_code_action_prepare`](doc:operation-language-code-action-prepare-language-code-action-prepare) — Préparer les changements d’une action de code sélectionnée.
- [`language_hover`](doc:operation-language-hover-language-hover) — Obtenir les informations de survol à une position du document.
- [`language_signature`](doc:operation-language-signature-language-signature) — Obtenir l’aide sur la signature de l’appel courant.
- [`language_declaration`](doc:operation-language-declaration-language-declaration) — Rechercher la déclaration d’un symbole.
- [`language_type_definition`](doc:operation-language-type-definition-language-type-definition) — Rechercher la définition du type d’un symbole.
- [`language_implementation`](doc:operation-language-implementation-language-implementation) — Rechercher les implémentations d’un symbole.
- [`language_document_highlights`](doc:operation-language-document-highlights-language-document-highlights) — Lister les occurrences du symbole dans le document.
- [`language_definition`](doc:operation-language-definition-language-definition) — Rechercher la définition d’un symbole.
- [`language_references`](doc:operation-language-references-language-references) — Rechercher les références d’un symbole.

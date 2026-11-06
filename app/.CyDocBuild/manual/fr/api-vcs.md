---
id: api-vcs
title: Contrôle de versions
description: Opérations publiques pour Contrôle de versions.
section: API et extensions
parent: api-links
order: 101
sourceRevision: 2
---

Opérations publiques pour Contrôle de versions.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`vcs_replay_state`](doc:operation-vcs-replay-state-vcs-replay-state) — Consulter l’état d’un rebase ou d’un cherry-pick Git en cours.
- [`vcs_replay_plan`](doc:operation-vcs-replay-plan-vcs-replay-plan) — Préparer une action de rebase ou de cherry-pick Git pour examen.
- [`vcs_replay_apply`](doc:operation-vcs-replay-apply-vcs-replay-apply) — Exécuter une action de rebase ou de cherry-pick précédemment préparée.
- [`vcs_merge_state`](doc:operation-vcs-merge-state-vcs-merge-state) — Consulter l’état de la fusion Git en cours.
- [`vcs_merge_plan`](doc:operation-vcs-merge-plan-vcs-merge-plan) — Préparer la finalisation ou l’abandon d’une fusion Git.
- [`vcs_merge_commit`](doc:operation-vcs-merge-commit-vcs-merge-commit) — Créer le commit de fusion à partir du plan examiné.
- [`vcs_merge_quit`](doc:operation-vcs-merge-quit-vcs-merge-quit) — Quitter une fusion Git selon le plan examiné.
- [`vcs_checkout_plan`](doc:operation-vcs-checkout-plan-vcs-checkout-plan) — Préparer une restauration de fichiers Git en précisant sa portée.
- [`vcs_checkout_apply`](doc:operation-vcs-checkout-apply-vcs-checkout-apply) — Appliquer le plan de restauration Git examiné.
- [`vcs_checkout_undo`](doc:operation-vcs-checkout-undo-vcs-checkout-undo) — Annuler une restauration Git prise en charge par le service.
- [`vcs_conflict_list`](doc:operation-vcs-conflict-list-vcs-conflict-list) — Lister les fichiers en conflit dans le dépôt Git choisi.
- [`vcs_conflict_inspect`](doc:operation-vcs-conflict-inspect-vcs-conflict-inspect) — Examiner les versions et les blocs d’un fichier en conflit.
- [`vcs_conflict_plan`](doc:operation-vcs-conflict-plan-vcs-conflict-plan) — Préparer une résolution de conflit pour examen.
- [`vcs_conflict_apply`](doc:operation-vcs-conflict-apply-vcs-conflict-apply) — Appliquer une résolution de conflit précédemment préparée.
- [`vcs_worktree_plan`](doc:operation-vcs-worktree-plan-vcs-worktree-plan) — Préparer le déplacement ou la réparation d’un worktree Git.
- [`vcs_worktree_move`](doc:operation-vcs-worktree-move-vcs-worktree-move) — Déplacer un worktree Git à partir d’un plan examiné.
- [`vcs_worktree_repair`](doc:operation-vcs-worktree-repair-vcs-worktree-repair) — Réparer les métadonnées d’un worktree Git selon le plan examiné.
- [`vcs_submodule_settings_plan`](doc:operation-vcs-submodule-settings-plan-vcs-submodule-settings-plan) — Préparer une modification de configuration d’un sous-module Git.
- [`vcs_submodule_settings`](doc:operation-vcs-submodule-settings-vcs-submodule-settings) — Appliquer la configuration de sous-module Git préparée.
- [`vcs_submodule_deinit_plan`](doc:operation-vcs-submodule-deinit-plan-vcs-submodule-deinit-plan) — Préparer la désinitialisation d’un sous-module Git.
- [`vcs_submodule_deinit`](doc:operation-vcs-submodule-deinit-vcs-submodule-deinit) — Désinitialiser un sous-module Git selon le plan examiné.
- [`vcs_submodule_add_plan`](doc:operation-vcs-submodule-add-plan-vcs-submodule-add-plan) — Préparer l’ajout d’un sous-module Git et examiner sa destination.
- [`vcs_submodule_add`](doc:operation-vcs-submodule-add-vcs-submodule-add) — Ajouter un sous-module Git à partir du plan examiné.
- [`vcs_submodules`](doc:operation-vcs-submodules-vcs-submodules) — Lister les sous-modules Git et leur état.
- [`vcs_submodule_init`](doc:operation-vcs-submodule-init-vcs-submodule-init) — Initialiser la configuration des sous-modules Git sélectionnés.
- [`vcs_submodule_sync`](doc:operation-vcs-submodule-sync-vcs-submodule-sync) — Synchroniser les URL des sous-modules Git configurés.
- [`vcs_submodule_update`](doc:operation-vcs-submodule-update-vcs-submodule-update) — Mettre à jour les sous-modules Git sélectionnés.
- [`vcs_repositories`](doc:operation-vcs-repositories-vcs-repositories) — Lister les dépôts de contrôle de versions découverts.
- [`vcs_worktrees`](doc:operation-vcs-worktrees-vcs-worktrees) — Lister les worktrees du dépôt Git.
- [`vcs_worktree_add`](doc:operation-vcs-worktree-add-vcs-worktree-add) — Créer un worktree Git avec la référence choisie.
- [`vcs_worktree_remove`](doc:operation-vcs-worktree-remove-vcs-worktree-remove) — Retirer un worktree Git selon les contraintes de l’opération.
- [`vcs_worktree_lock`](doc:operation-vcs-worktree-lock-vcs-worktree-lock) — Verrouiller un worktree Git avec un motif facultatif.
- [`vcs_worktree_unlock`](doc:operation-vcs-worktree-unlock-vcs-worktree-unlock) — Déverrouiller un worktree Git.
- [`vcs_inspect`](doc:operation-vcs-inspect-vcs-inspect) — Examiner les branches, les remotes et les informations du dépôt Git.
- [`vcs_init`](doc:operation-vcs-init-vcs-init) — Initialiser un dépôt Git dans l’espace de travail.
- [`vcs_history`](doc:operation-vcs-history-vcs-history) — Lire l’historique Git avec les filtres demandés.
- [`vcs_show`](doc:operation-vcs-show-vcs-show) — Afficher le contenu ou les changements d’un objet Git.
- [`vcs_blame`](doc:operation-vcs-blame-vcs-blame) — Lire l’origine Git des lignes d’un fichier.
- [`vcs_branch_create`](doc:operation-vcs-branch-create-vcs-branch-create) — Créer une branche Git à partir de la référence choisie.
- [`vcs_branch_switch`](doc:operation-vcs-branch-switch-vcs-branch-switch) — Basculer vers une branche Git.
- [`vcs_branch_rename`](doc:operation-vcs-branch-rename-vcs-branch-rename) — Renommer une branche Git.
- [`vcs_branch_delete`](doc:operation-vcs-branch-delete-vcs-branch-delete) — Supprimer une branche Git selon les contraintes demandées.
- [`vcs_remote_add`](doc:operation-vcs-remote-add-vcs-remote-add) — Ajouter un remote au dépôt Git.
- [`vcs_remote_set`](doc:operation-vcs-remote-set-vcs-remote-set) — Modifier l’URL d’un remote Git.
- [`vcs_remote_remove`](doc:operation-vcs-remote-remove-vcs-remote-remove) — Retirer un remote Git.
- [`vcs_fetch`](doc:operation-vcs-fetch-vcs-fetch) — Récupérer les références d’un remote Git.
- [`vcs_pull`](doc:operation-vcs-pull-vcs-pull) — Intégrer les changements du remote Git selon la stratégie choisie.
- [`vcs_push`](doc:operation-vcs-push-vcs-push) — Envoyer les références Git sélectionnées vers un remote.
- [`vcs_merge`](doc:operation-vcs-merge-vcs-merge) — Fusionner une référence Git dans la branche courante.
- [`vcs_stash_save`](doc:operation-vcs-stash-save-vcs-stash-save) — Mettre les changements Git de côté dans un stash.
- [`vcs_stash_apply`](doc:operation-vcs-stash-apply-vcs-stash-apply) — Appliquer un stash Git en le conservant.
- [`vcs_stash_pop`](doc:operation-vcs-stash-pop-vcs-stash-pop) — Appliquer un stash Git puis le retirer en cas de réussite.
- [`vcs_stash_drop`](doc:operation-vcs-stash-drop-vcs-stash-drop) — Supprimer un stash Git sélectionné.
- [`vcs_conflicts`](doc:operation-vcs-conflicts-vcs-conflicts) — Consulter les conflits Git et les versions disponibles.
- [`vcs_resolve`](doc:operation-vcs-resolve-vcs-resolve) — Marquer une résolution de conflit Git avec la stratégie demandée.
- [`vcs_status`](doc:operation-vcs-status-vcs-status) — Lire l’état des fichiers du dépôt Git.
- [`vcs_diff`](doc:operation-vcs-diff-vcs-diff) — Lire les différences Git pour la portée choisie.
- [`vcs_log`](doc:operation-vcs-log-vcs-log) — Lire une liste bornée de commits Git.
- [`vcs_stage`](doc:operation-vcs-stage-vcs-stage) — Ajouter les chemins sélectionnés à l’index Git.
- [`vcs_hunks`](doc:operation-vcs-hunks-vcs-hunks) — Lister les blocs de différences Git pouvant être sélectionnés.
- [`vcs_apply_hunks`](doc:operation-vcs-apply-hunks-vcs-apply-hunks) — Appliquer une sélection de blocs Git avec validation de l’état examiné.
- [`vcs_unstage`](doc:operation-vcs-unstage-vcs-unstage) — Retirer les chemins sélectionnés de l’index Git.
- [`vcs_commit`](doc:operation-vcs-commit-vcs-commit) — Créer un commit Git à partir de l’index courant.
- [`vcs_providers`](doc:operation-vcs-providers-vcs-providers) — Lister les fournisseurs de contrôle de versions et leurs capacités.
- [`vcs_context`](doc:operation-vcs-context-vcs-context) — Résoudre le fournisseur de contrôle de versions applicable à un chemin.
- [`vcs_configure`](doc:operation-vcs-configure-vcs-configure) — Configurer les associations entre chemins et fournisseurs de contrôle de versions.
- [`vcs_provider_action`](doc:operation-vcs-provider-action-vcs-provider-action) — Invoquer une action déclarée par le fournisseur de contrôle de versions choisi.

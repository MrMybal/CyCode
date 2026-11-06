---
id: api-debug
title: Débogage
description: Opérations publiques pour Débogage.
section: API et extensions
parent: api-links
order: 112
sourceRevision: 2
---

Opérations publiques pour Débogage.

Ces contrats décrivent le service natif exposé par l’intégration SDK/MCP officielle. La disponibilité dépend du service configuré, du projet et des autorisations. Consultez le [contrat commun](doc:api-links#contract) pour les révisions, les aperçus et les tâches asynchrones.

## Opérations {#operations}

- [`debug_adapters`](doc:operation-debug-adapters-debug-adapters) — Lister les adaptateurs de débogage disponibles.
- [`debug_profile`](doc:operation-debug-profile-debug-profile) — Lire ou préparer un profil de débogage.
- [`debug_configure`](doc:operation-debug-configure-debug-configure) — Configurer le profil de débogage actif.
- [`debug_status`](doc:operation-debug-status-debug-status) — Consulter l’état de la session de débogage.
- [`debug_breakpoints`](doc:operation-debug-breakpoints-debug-breakpoints) — Lire ou modifier les points d’arrêt de source.
- [`debug_launch`](doc:operation-debug-launch-debug-launch) — Lancer une session avec l’adaptateur de débogage configuré.
- [`debug_attach`](doc:operation-debug-attach-debug-attach) — Attacher le débogueur à une cible explicitement choisie.
- [`debug_run`](doc:operation-debug-run-debug-run) — Lancer la cible configurée sans session de débogage interactive.
- [`debug_continue`](doc:operation-debug-continue-debug-continue) — Reprendre l’exécution de la cible arrêtée.
- [`debug_pause`](doc:operation-debug-pause-debug-pause) — Demander la suspension de l’exécution de la cible.
- [`debug_hot_reload`](doc:operation-debug-hot-reload-debug-hot-reload) — Demander le remplacement à chaud des classes Java prises en charge.
- [`debug_cancel_reload`](doc:operation-debug-cancel-reload-debug-cancel-reload) — Annuler la préparation du remplacement à chaud Java.
- [`debug_next`](doc:operation-debug-next-debug-next) — Avancer à la ligne suivante sans entrer dans les appels.
- [`debug_step_in`](doc:operation-debug-step-in-debug-step-in) — Entrer dans l’appel courant avec le débogueur.
- [`debug_step_out`](doc:operation-debug-step-out-debug-step-out) — Sortir de l’appel courant avec le débogueur.
- [`debug_step_back`](doc:operation-debug-step-back-debug-step-back) — Reculer d’un pas si l’adaptateur prend en charge cette capacité.
- [`debug_reverse_continue`](doc:operation-debug-reverse-continue-debug-reverse-continue) — Reprendre l’exécution en sens inverse si l’adaptateur le permet.
- [`debug_restart_frame`](doc:operation-debug-restart-frame-debug-restart-frame) — Redémarrer une frame si l’adaptateur le permet.
- [`debug_step_in_targets`](doc:operation-debug-step-in-targets-debug-step-in-targets) — Lister les cibles d’entrée proposées par l’adaptateur.
- [`debug_step_into_target`](doc:operation-debug-step-into-target-debug-step-into-target) — Entrer dans une cible d’appel précédemment retournée.
- [`debug_stop`](doc:operation-debug-stop-debug-stop) — Arrêter ou détacher la session de débogage selon son mode.
- [`debug_run_to_cursor`](doc:operation-debug-run-to-cursor-debug-run-to-cursor) — Reprendre l’exécution jusqu’à un point d’arrêt temporaire au curseur.
- [`debug_restore_run_to_cursor`](doc:operation-debug-restore-run-to-cursor-debug-restore-run-to-cursor) — Restaurer les points d’arrêt après une exécution jusqu’au curseur.
- [`debug_goto_targets`](doc:operation-debug-goto-targets-debug-goto-targets) — Lister les destinations de repositionnement proposées par l’adaptateur.
- [`debug_goto_target`](doc:operation-debug-goto-target-debug-goto-target) — Repositionner l’exécution vers une destination précédemment retournée.
- [`debug_evaluate`](doc:operation-debug-evaluate-debug-evaluate) — Évaluer une expression dans le contexte de débogage choisi.
- [`debug_variables`](doc:operation-debug-variables-debug-variables) — Lire les variables d’une portée ou d’un objet de débogage.
- [`debug_memory`](doc:operation-debug-memory-debug-memory) — Lire une plage de mémoire lorsque l’adaptateur le permet.
- [`debug_set_variable`](doc:operation-debug-set-variable-debug-set-variable) — Modifier une variable lorsque l’adaptateur le permet.
- [`debug_set_expression`](doc:operation-debug-set-expression-debug-set-expression) — Modifier une expression lorsque l’adaptateur le permet.
- [`debug_write_memory`](doc:operation-debug-write-memory-debug-write-memory) — Écrire une plage de mémoire lorsque l’adaptateur le permet.
- [`debug_disassemble`](doc:operation-debug-disassemble-debug-disassemble) — Lire le désassemblage d’une plage prise en charge par l’adaptateur.
- [`debug_exception_info`](doc:operation-debug-exception-info-debug-exception-info) — Lire les détails de l’exception courante.
- [`debug_data_breakpoint_info`](doc:operation-debug-data-breakpoint-info-debug-data-breakpoint-info) — Demander les possibilités de point d’arrêt sur une donnée.
- [`debug_data_breakpoints`](doc:operation-debug-data-breakpoints-debug-data-breakpoints) — Configurer des points d’arrêt sur données si l’adaptateur les prend en charge.
- [`debug_instruction_breakpoints`](doc:operation-debug-instruction-breakpoints-debug-instruction-breakpoints) — Configurer des points d’arrêt d’instruction si l’adaptateur les prend en charge.
- [`debug_function_breakpoints`](doc:operation-debug-function-breakpoints-debug-function-breakpoints) — Configurer des points d’arrêt de fonction si l’adaptateur les prend en charge.
- [`debug_exception_breakpoints`](doc:operation-debug-exception-breakpoints-debug-exception-breakpoints) — Configurer les filtres d’arrêt sur exceptions exposés par l’adaptateur.
- [`debug_frame`](doc:operation-debug-frame-debug-frame) — Choisir une frame de la pile d’appels courante.
- [`debug_thread`](doc:operation-debug-thread-debug-thread) — Choisir un thread de la session de débogage.
- [`debug_source`](doc:operation-debug-source-debug-source) — Lire une source exposée par l’adaptateur de débogage.

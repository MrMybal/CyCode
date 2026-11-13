#pragma once
#include <cyui/Core.h>
inline void registerManagerLocale() {
    cyui::registerTranslations("fr", {
        {"Agents and skills", "Agents et skills"}, {"Dependencies", "Dépendances"}, {"Updates", "Mises à jour"},
        {"Local preview", "Aperçu local"},
        {"Project", "Projet"}, {"Scope", "Portée"}, {"This project", "Ce projet"}, {"My user account", "Mon compte utilisateur"},
        {"Access", "Accès"}, {"Read only", "Lecture seule"}, {"Read, edit and build", "Lecture, édition et compilation"},
        {"Install skill", "Installer le skill"}, {"Configure MCP", "Configurer le MCP"}, {"Browse", "Parcourir"},
        {"MCP catalogue", "Catalogue MCP"}, {"Code (compact)", "Code (compact)"}, {"Full catalogue", "Catalogue complet"},
        {"Preview changes", "Voir les modifications"}, {"Apply reviewed changes", "Appliquer les modifications vérifiées"},
        {"Managed connections", "Connexions gérées"}, {"Refresh connections", "Actualiser les connexions"},
        {"Remove connection", "Retirer la connexion"}, {"Refresh", "Actualiser"}, {"Ready", "Prêt"},
        {"Working", "Opération en cours"}, {"Changes applied. Reload the affected clients.", "Modifications appliquées. Rechargez les clients concernés."},
        {"No managed connection", "Aucune connexion gérée"}, {"Select applications and preview their configuration changes.", "Sélectionnez les applications et prévisualisez leurs modifications."},
        {"Installed", "Installé"}, {"Missing", "Absent"}, {"Install private clangd", "Installer clangd pour Core"},
        {"Official downloads", "Téléchargements officiels"}, {"Install clangd", "Installer clangd"},
        {"I accept the Apache-2.0 license with LLVM exception", "J’accepte la licence Apache-2.0 avec exception LLVM"},
        {"Install", "Installer"}, {"Cancel", "Annuler"}, {"Cancel download", "Annuler le téléchargement"},
        {"No global PATH or Windows installation is changed.", "Le PATH global et les installations Windows ne sont pas modifiés."},
        {"Existing versions and running sessions are preserved.", "Les versions existantes et les sessions actives sont conservées."},
        {"Reload MCP clients after selecting a new dependency.", "Rechargez les clients MCP après sélection d’une dépendance."},
        {"Check for updates", "Vérifier les mises à jour"}, {"Download and install", "Télécharger et installer"},
        {"Update managed connections after installation", "Actualiser les connexions gérées après installation"},
        {"Confirm update", "Confirmer la mise à jour"}, {"Release notes", "Notes de version"},
        {"No update offered", "Aucune mise à jour proposée"}, {"Close", "Fermer"},
        {"Language", "Langue"}, {"Theme", "Thème"}, {"Compact density", "Densité compacte"},
        {"Source", "Source"}, {"Destination", "Destination"}, {"Download", "Téléchargement"},
        {"C/C++ semantic analysis", "Analyse sémantique C/C++"}, {"Project text search", "Recherche textuelle du projet"},
        {"Git repositories", "Dépôts Git"}, {"CMake projects", "Projets CMake"}, {"C#/.NET projects", "Projets C#/.NET"},
        {"C++ inspections", "Inspections C++"}, {"Native debugging", "Débogage natif"},
        {"Skill instructions and Core tools are separate. Reload your client after applying a connection.", "Les instructions du skill et les outils Core sont distincts. Rechargez votre client après application d’une connexion."},
        {"Code exposes common tools. Other operations remain available on demand.", "Code expose les outils courants. Les autres opérations restent accessibles à la demande."},
        {"An idle language server is not necessarily missing.", "Un serveur de langage en attente n’est pas nécessairement absent."},
        {"Stopping download. Complete or cancel any visible installer.", "Arrêt du téléchargement. Terminez ou annulez tout installateur visible."}
    });
}

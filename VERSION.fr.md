# CyCode 0.2.3

Windows x64 · ALPHA · 7 novembre 2026.

Cette version ajoute l’installateur Windows par utilisateur, en anglais/français,
avec raccourcis, runtime MSVC fourni et désinstallation Windows. Les données de
l’application et les outils supplémentaires sont conservés à la désinstallation.

Les copies installées téléchargent et lancent l’installateur vérifié pour leurs mises à jour.
Les copies portables utilisent le même ZIP pour l’extraction et les mises à jour intégrées. Aucun `-update.zip` séparé n’est publié. Le moteur existant conserve
vérification SHA-256, contrôle des fichiers, sauvegardes et restauration.
L’installateur refuse les instances ouvertes et les récupérations en attente.

Les versions jusqu’à 0.2.2 utilisent l’ancien nom de paquet : installez 0.2.3 une
première fois avec Setup ou extrayez son ZIP dans un dossier distinct. Les mises
à jour suivantes passent par l’application. La vérification au démarrage reste
activée par défaut ; les préférences déjà enregistrées sont conservées.

- [Installateur Windows](https://github.com/MrMybal/CyCode/releases/download/v0.2.3/CyCode-0.2.3-windows-x64-setup.exe).
- [ZIP portable](https://github.com/MrMybal/CyCode/releases/download/v0.2.3/CyCode-0.2.3-windows-x64.zip).
- [Notes anglaises et limites](app/Docs/Releases/v0.2.3.md).

La compilation Windows Release et les contrôles de sources, données et paquets
sont effectués. L’installateur, l’application et l’assistant n’ont pas été exécutés
localement ; le parcours complet de mise à jour et Linux/macOS restent à valider.
Windows 10/11 x64 et OpenGL 3.3 sont requis. CyCode reste en alpha sous GPL-3.0-only.

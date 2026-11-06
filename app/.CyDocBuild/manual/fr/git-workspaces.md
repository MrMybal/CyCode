---
id: git-workspaces
title: Dépôts, worktrees et sous-modules
description: Choisissez la bonne racine Git avant de modifier les copies liées.
section: Travailler dans CyCode
parent: git
order: 314
sourceRevision: 1
---

## Choisir un dépôt {#repositories}

Contrôle de versions propose Automatique (fichier actif) et les dépôts découverts. Le mode automatique suit la racine Git la plus proche du fichier actif ; une association de fournisseur externe plus précise est prioritaire. Vérifiez la racine affichée avant l’ajout à l’index, le commit ou une action globale au dépôt. Les chemins affichés par CyCode restent relatifs à l’espace de travail.

## Worktrees liés {#worktrees}

Ouvrez Worktrees et actualisez. Créez une copie sur une nouvelle branche, une branche locale existante ou une révision vérifiée en mode détaché. Le nouveau dossier ne doit pas exister et son parent doit exister dans l’espace de travail. Le verrouillage est un verrou administratif Git sur la copie, pas un verrou sur le contenu des fichiers.

La suppression exige une copie liée propre et examinée ; elle conserve la branche. Les copies principale, courante, verrouillées ou indisponibles et les destinations contenant des documents ouverts sont refusées. Le déplacement et la réparation des liens utilisent un aperçu explicite dont les identités de catalogue et de source doivent rester courantes. Ouvrir une copie listée dans une nouvelle fenêtre d’application est une action séparée.

## Sous-modules {#submodules}

Ouvrez Sous-modules et sélectionnez un module de premier niveau. Register enregistre son URL localement ; Synchronise actualise cette inscription. Update to recorded commit récupère la révision inscrite dans l’index du parent, généralement avec HEAD détachée. Cloner ou récupérer exige la case explicite correspondante.

L’ajout prévisualise d’abord l’adresse, le nom, la branche et le nouveau dossier. L’action séparée de clonage et d’ajout à l’index y inscrit `.gitmodules` et le gitlink, sans créer de commit du parent. Examinez les résultats partiels après un échec. Annuler abandonne un aperçu avant son lancement ; les effets Git ne sont pas des transactions d’annulation de l’éditeur.

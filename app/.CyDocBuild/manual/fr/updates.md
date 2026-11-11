---
id: updates
title: Mettre à jour l’application
description: Consultez les versions, téléchargez une mise à jour et choisissez son installation.
section: Bien démarrer
order: 290
parent: overview
sourceRevision: 5
---

## Vérifier, télécharger et installer {#updates}

Ouvrez les mises à jour de l’application depuis le menu Affichage. La vérification au démarrage est réglable dans les options. Une nouvelle version stable GitHub propose de reporter ou d’ignorer cette version ; téléchargement et installation restent distincts.

1. Recherchez une version et lisez ses notes.
2. Téléchargez ou reprenez la mise à jour compatible. L’application vérifie son SHA-256 avant d’autoriser l’installation.
3. Choisissez l’installation et le redémarrage lorsque vous êtes prêt. Enregistrez ou abandonnez les documents dans les demandes de fermeture habituelles ; Annuler interrompt l’installation.
4. Fermez les autres instances utilisant cette installation. L’assistant attend la libération de sa propriété plutôt que de tuer ces processus.

Un téléchargement interrompu peut proposer une reprise ou un téléchargement neuf. Conservez les sauvegardes de récupération tant que l’installation n’est pas réglée. Le retrait de stockage déplace les données conservées et ne libère pas immédiatement leur espace. L’historique détaillé est indicatif ; il ne prouve pas que les fichiers installés correspondent toujours à un ancien résultat.

La vérification anonyme ne peut pas lire une version GitHub privée. Sans paquet compatible accessible, utilisez manuellement la page de version et l’archive fournie. Une erreur de connexion n’est pas présentée comme une application à jour.

Consultez [reprise de téléchargement, stockage et historique](doc:update-storage#downloads) avant de réorganiser les fichiers conservés.


## Installation Windows {#windows-installation}

Utilisez l’installateur Windows par utilisateur ou le ZIP portable complet. Les deux incluent le runtime MSVC local et le manuel hors ligne. Les données restent dans data/ de l’installation accessible en écriture et sont conservées à la désinstallation. Depuis 0.2.3 les copies installées utilisent Setup vérifié et les copies portables utilisent le même ZIP pour leurs mises à jour, sans archive de mise à jour séparée. Les versions jusqu’à 0.2.2 nécessitent une première installation manuelle. La vérification au démarrage est active par défaut ; les préférences existantes restent appliquées.


## Identité de mise à jour propre au consommateur {#products}

`CyCode::Updates` est une bibliothèque native statique commune à l’application et
à son assistant. Son identité est figée pour ce graphe de cibles ; projet,
préférences, cache et réseau ne peuvent pas changer de produit. Configurez avant
l’ajout du Core :

```cmake
set(CYCODE_BUILD_UPDATES ON)
set(CYCODE_UPDATES_PRODUCT CyCodeIDE)
set(CYCODE_UPDATES_REPOSITORY MrMybal/CyCodeIDE)
set(CYCODE_UPDATES_VERSION "${PROJECT_VERSION}") # Version hôte capturée avant project() du Core.
add_subdirectory("${CYCODE_SOURCE}" "${CMAKE_BINARY_DIR}/core")
target_link_libraries(CyCodeIDE PRIVATE CyCode::Updates)
target_link_libraries(CyCodeIDEUpdater PRIVATE CyCode::Updates)
```

Ces trois variables CMake et les propriétés homonymes de `cycode_updates` forment
le contrat exact. Produit : nom ASCII sûr de 64 caractères au plus. Dépôt : référence
bornée `owner/repository`. Version : MAJOR.MINOR.PATCH numérique, chaque composante
au plus 65535. Tout produit autre que l’identité historique doit préciser dépôt et
version hôte. Ne réutilisez pas une bibliothèque configurée pour un autre produit ;
compilez des distributions séparées. Lisez `cycode::updates::productIdentity()` dans
`cycode/Updates.h` : `product`, `repository`, `currentVersion`, `application`, `helper`,
`installationMarker`, `installationMarkerText`, `tokenEnvironment`, `requiredFiles`
et `acceptsLegacyMetadata`. Les opérations de mise à jour gardent leurs signatures.
La variable exportée `updates::repository` désigne désormais le dépôt configuré.

Le profil IDE utilise **CyCodeIDE.exe**, **CyCodeIDEUpdater.exe** et
**CyCodeIDE-installed.flag**, dont les octets exacts sont
`CyCodeIDE per-user installation\n`. Le jeton éventuel est **CYCODEIDE_UPDATE_TOKEN**,
puis GH_TOKEN / GITHUB_TOKEN. Seules les Releases de MrMybal/CyCodeIDE conviennent.
Paquets : `CyCodeIDE-VERSION-windows-x64.zip`, ou
`CyCodeIDE-VERSION-windows-x64-setup.exe` pour une copie installée. La racine ZIP
stockée est `CyCodeIDE-VERSION/`. Cette bibliothèque ne crée pas l’assistant
graphique : l’hôte possède interface, consentement/fermeture, installateur et cible
de redémarrage. Le transport automatique reste Windows x64 ; aucune opération MCP
de projet n’est ajoutée au catalogue de 252 opérations.

Le manifeste conserve `updateFormat: 1`, plateforme/version/fichiers et ajoute
`product` et `repository` exacts, tous deux requis pour un nouveau produit.
Ces identifiants lient aussi téléchargements partiels/préparés, résultats,
journaux d’installation/reprise en attente, données de récupération préparées,
retraits de cache et historique. Produit/dépôt, exécutables, racines ZIP ou
marqueurs d’une autre application sont refusés avant remplacement ; les caches
étrangers sont rejetés/protégés. SHA-256, listes exactes, limites de chemins/casse/
types, verrous, rollback et budgets disque restent actifs. Un nouveau profil exige
au minimum son exécutable, son assistant, LICENSE et COPYRIGHT. L’hôte vérifie aussi
son véritable SDK, ses polices, providers et manuel. `extensions/`, `tools/python/`,
`VERSION.md` et VERSION.fr.md restent autorisés ; données privées, runtime et caches
de travail ne font pas partie du paquet.

Sans options, le profil historique MrMybal/CyCode conserve noms, installed.flag,
fichiers requis, nom du jeton et compatibilité des anciennes métadonnées sans
identité. Une identité étrangère explicite est refusée même dans ce profil. Un fork
est une autre identité ; garder le nom CyCode n’accorde pas cette exception.

Les fonctions pures de `app/scripts/release_package.py` acceptent l’objet immuable
`UpdateProduct('CyCodeIDE', 'MrMybal/CyCodeIDE')` avec le mot-clé `product` :
`package_path`, `required_files`, `payload`, `validate_payload`, `write_archive`.
`required` précise les exigences de l’hôte pour création/validation/archive.
Tous les octets providers/MCP fournis sont conservés. Identité et empreintes du
manifeste sont vérifiées avant ouverture de la destination ZIP ; sa racine suit
le produit. Utilisez `update=True` pour le ZIP portable stocké consommé par Apply.
Aucun code empaqueté n’est exécuté. `windows_installer.compile_installer` accepte
`product`, `required` et un `script` (.iss) explicite appartenant à l’hôte. Il prépare
le bon marqueur et les valeurs UpdateProduct, UpdateRepository, InstallMarker du
préprocesseur. Un nouveau produit ne peut pas utiliser silencieusement le script
ou l’identité registre CyCode. Le script Inno Setup et AppId appartiennent à l’hôte.

Les bibliothèques de production CyCode et CyCodeIDE compilent sous Windows x64.
`verify_update_profiles.py --cmake <cmake>` contrôle la configuration sans compilateur,
réseau, application ni exécutable de fixture. Les tests de paquets utilisent des ZIP
en mémoire. Des régressions natives de profil/manifeste étranger sont ajoutées à la
suite existante, mais ne sont pas exécutées ici. Réseau réel, installation/Apply/
récupération, interactions graphiques, publication et Linux/macOS restent distincts.

# CyCode 0.2.2 — Alpha

[README français](README.fr.md) · [English release notes](app/Docs/Releases/v0.2.2.md)

Version publiée le **6 novembre 2026**, disponible en paquet portable **Windows x64**.
Le statut du produit reste **Alpha** : une version expérimentale pour les essais
et les retours, avec des fonctions incomplètes et des outils externes à installer.

## Changements de cette version

- Manuel local et en ligne fondé sur le lecteur CyDoc, avec guides anglais/français,
  sommaire imbriqué, recherche globale dans l’en-tête (`Ctrl+K`), six thèmes, images,
  galeries et schémas statiques.
- 40 guides pratiques sur l’édition, les projets et Unreal, les langages, le
  débogage, les extensions, le contrôle de versions et les mises à jour.
- Références publiques traduites et schémas JSON des 227 opérations SDK/MCP de cette
  release, ainsi que le contrat C public des plugins.
- Aide et raccourcis contextuels vers `Documentation.html`. Lecteur complet dans
  les archives portable et de mise à jour, avec compatibilité des anciens liens.

Les ajouts ultérieurs du code en développement, notamment navigation C++ et
catalogues d’extraits, ne sont pas annoncés comme livrés dans le ZIP 0.2.2.
Consulter l’[historique](CHANGELOG.md) et le [guide développeur](app/README.md).

## Limites connues

- Seul le paquet Windows x64 est publié. Les compilations graphiques complètes et
  le fonctionnement Linux/macOS restent à valider.
- La couverture fonctionnelle complète de Rider n’est pas atteinte. Les capacités
  des serveurs de langage, débogueurs et fournisseurs VCS dépendent des outils
  installés. Coloration, compilation et services sémantiques sont distincts.
- Compilateurs, SDK, Unreal Engine, serveurs de langage et débogueurs sont fournis
  séparément ; le MCP optionnel nécessite le SDK CyToolsCore et son environnement.
- La validation d’exécution de l’application, du MCP et de la mise à jour de cette
  release reste à compléter. Les essais d’installation sur machine vierge et les
  contrôles hors ligne réels, tactiles et DPI restent des vérifications distinctes.

Ces limites ne constituent pas une liste de bugs confirmés. La release ne publie
pas de liste bug par bug ; les problèmes reproductibles peuvent être signalés
dans le [suivi des problèmes](https://github.com/MrMybal/CyCode/issues).

## Téléchargement et documentation

- [Release 0.2.2 et empreintes](https://github.com/MrMybal/CyCode/releases/tag/v0.2.2).
- [Paquet portable Windows x64](https://github.com/MrMybal/CyCode/releases/download/v0.2.2/CyCode-0.2.2-windows-x64.zip).
- [Documentation française](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview).

Extraire l’archive complète dans un dossier accessible en écriture. Visual C++ 2022
x64 et OpenGL 3.3 sont nécessaires. L’archive `-update.zip` sert uniquement au
parcours explicite de mise à jour. CyCode est sous [GPL-3.0-only](LICENSE) ; ses
bibliothèques conservent leurs licences respectives.

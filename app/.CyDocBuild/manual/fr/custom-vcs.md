---
id: custom-vcs
title: Développer un fournisseur de contrôle de versions
description: Exposer les actions réelles de votre service via ABI2 sans imiter Git.
section: Travailler dans CyCode
parent: plugin-api
order: 281
sourceRevision: 1
---

## Enregistrer le fournisseur {#provider}

Utilisez le [contrat natif](doc:plugin-api#contract), un ID d’extension indépendant
et une contribution `vcs` dans son espace de noms. Annoncez uniquement les capacités
implémentées. Un service peut publier des changements ou gérer des réservations
sans zone de préparation Git, remotes ni sémantique Push/Pull.

```json
{
  "id": "example.revisions.provider",
  "title": "Example revisions",
  "capabilities": ["status", "diff", "publish"],
  "actions": [
    {"id": "diff", "title": "Working differences", "scope": "repository", "fields": []},
    {"id": "publish", "title": "Publish changes", "scope": "repository",
     "fields": [{"id": "message", "title": "Description", "required": true}]}
  ]
}
```

Ceci est un exemple de descripteur, pas un service implémenté. Fournissez son
callback de commandes et un manifeste compatible. Status retourne des chemins
relatifs au projet et les vraies métadonnées du service. Diff fournit un patch
unifié à Révision ; log/blame un texte en lecture seule. Une vue binaire ou de
conflit non représentable exige un panneau/contrat explicite au lieu d’un faux texte Git.

## Routage et accès {#routing}

Les associations de `.cycode/vcs.json` choisissent la racine correspondante la
plus précise ; Git reste le choix intégré par défaut. Un fournisseur associé
indisponible le reste jusqu’à réactivation/réinstallation ou réassociation
explicite. Menus et formulaires utilisent ses actions. Les opérations Git typées
restent propres à Git ; [vcs_provider_action](doc:operation-vcs-provider-action-vcs-provider-action)
utilise le fournisseur et les permissions prudentes extension/processus/projet/VCS.

Les lectures reconnues sont status/inspect/diff/log/history/show/blame. Tout autre
ID, même un nom ressemblant à une lecture, est traité comme une mutation avec
gardes de buffers modifiés et d’état disque. Copiez les résultats hôte avant la
requête suivante ; interceptez les exceptions à la frontière C et attendez la fin
du travail possédé avant désactivation/arrêt. Une DLL native est du code de confiance
dans l’application, sans isolation de processus. Authentification, publication
distante et historique restent la responsabilité du service. Voir
[les associations de projets](doc:version-control#providers).

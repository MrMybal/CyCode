---
id: plugin-api
title: Développer un plugin natif
description: Utilisez les points d’entrée C publics et les contributions JSON UTF-8.
section: Travailler dans CyCode
order: 280
parent: plugins
sourceRevision: 6
---

## Inclusion et points d’entrée {#contract}

Incluez `cycode/PluginAPI.h` depuis les en-têtes du SDK installé ou la distribution source correspondante. Compilez une bibliothèque partagée compatible. `CYCODE_PLUGIN_ABI` vaut 1 et `CYCODE_PLUGIN_ABI_V2` vaut 2. L’ABI 2 ajoute la propriété des instances et des contributions plus riches sans changer l’ABI 1.

```cpp
int cycode_plugin_init_v2(const CyCodeHostV2* host, void** plugin_instance);
void cycode_plugin_shutdown_v2(void* plugin_instance);
void cycode_plugin_active_v2(void* plugin_instance, uint32_t active);
```

L’export d’initialisation est requis pour l’ABI 2 ; les callbacks de fermeture et d’activation sont facultatifs selon leur contrat. Vérifiez `abi_version`, `struct_size` et les bits de capacité avant de lire les champs facultatifs.

`host` désigne le contrat de l’hôte courant. Écrivez l’état propre à cet hôte dans `*plugin_instance` ; retournez 0 si l’initialisation réussit et une valeur non nulle en cas d’échec. L’hôte transmet cette instance aux callbacks de commande, d’activation et de fermeture de l’ABI 2. `active` vaut 1 après activation et 0 après désactivation ; les callbacks d’activation et de fermeture ne doivent pas appeler l’hôte.

L’ABI 1 utilise `int cycode_plugin_init(const CyCodeHostV1* host)` et la fonction facultative `void cycode_plugin_shutdown(void)`. Son callback de commande est `const char* callback(const char* input_json)` ; l’ABI 2 ajoute le paramètre d’instance : `const char* callback(void* plugin_instance, const char* input_json)`. L’hôte copie le résultat appartenant au plugin avant le callback suivant.

## Catalogues d’extraits déclaratifs {#snippet-catalogues}

Après les contrôles ABI2 habituels, vérifiez `struct_size >= sizeof(CyCodeHostV2)`
et `host->features & CYCODE_HOST_FEATURE_SNIPPETS_V2`. Pendant l’initialisation,
passez le type `snippets`, le JSON UTF-8 suivant et `nullptr` comme callback à
contribute. Cet exemple exige l’ID de manifeste `example.code` :

```json
{
  "id": "example.code.snippets",
  "title": "Extraits d’exemple",
  "order": 20,
  "definitions": {
    "Encadrer": { "prefix": "wrap", "scope": "cpp,hlsl,glsl", "body": "(${1:$TM_SELECTED_TEXT})$0" }
  }
}
```

L’ID doit utiliser l’espace de noms du plugin. Le titre facultatif tient sur une
ligne / 160 octets ; order vaut 0 par défaut, entre -10000 et 10000. Seuls
id/title/order/definitions sont acceptés. Les définitions, non vides, suivent la
syntaxe du catalogue enregistré ; callbacks, chemins, substitution du propriétaire
et conditions when/context sont refusés. Par catalogue : 256 Kio, 256 définitions
et corps de 8 Kio. Registre : 32 catalogues / 1 Mio, en comptant les enregistrements
inactifs et la provenance ajoutée par l’hôte. Aucune autorisation supplémentaire
n’est nécessaire pour proposer/lire ; appliquer un plan exige toujours workspace.write.

L’instantané actif est trié par ordre/ID après les extraits du projet. La provenance
est conservée, avec 512 propositions communes et les budgets de données habituels.
Un changement du registre invalide émission/préparation et acceptation graphique
en attente. Les plans concrets déjà retournés conservent leurs contrôles habituels.
C’est une insertion déclarative, sans callback fournisseur ni requête LSP, pas une
isolation du reste du code natif. Voir [utiliser les extraits](doc:refactoring#plugin-snippets).

## Préférences d’extraits personnels {#user-snippets}

L’hôte JSON expose snippets.userRead et snippets.userSave pour le stockage personnel
fixe configuré par l’intégration. Lire ne le crée pas. Écrire exige settings.write ;
workspace.write ne suffit pas, et les callbacks en lecture seule ne peuvent pas
écrire même avec cette permission. Aucun chemin en paramètre ni changement de
structure ABI C. Utilisez la révision de la lecture, conservez les erreurs et
redemandez les extraits après un changement. Engine par défaut retourne available=false.
Voir les [extraits personnels](doc:refactoring#user-snippets) et les contrats de
[lecture](doc:operation-snippets-user-read-snippets-user-read) /
[sauvegarde](doc:operation-snippets-user-save-snippets-user-save).

## Capturer du code sélectionné {#capture-snippet}

snippets.userDraft / SDK [snippets_user_draft](doc:operation-snippets-user-draft-snippets-user-draft)
exporte une sélection courante de l’espace de travail en brouillon JSON personnel.
Fournissez path, expectedRevision et une sélection UTF-16 ordonnée. Ce service hôte
en lecture seule est accessible aux callbacks en lecture seule, sans création de
fichier/verrou ni changement de structure ABI. Le résultat inclut le catalogue
initial, le brouillon, le nom unique et le chemin/révision source. Les limites de
source/échappement/catalogue sont vérifiées nativement. Examinez le brouillon puis
enregistrez séparément avec catalogue.revision et settings.write. Voir la
[capture du code sélectionné](doc:refactoring#capture-snippet).

## Contributions et appels {#contributions}

`host->contribute(context, kind, descriptor_json, callback)` enregistre les descripteurs pris en charge durant l’initialisation. `host->request(context, method, parameters_json)` retourne le JSON `{ok,result,error}` appartenant à l’hôte durant un callback autorisé ; vérifiez `ok` avant de lire `result`.

`contribute` retourne 0 en cas de réussite et une valeur non nulle en cas d’échec. `kind` choisit la famille de descripteurs ; `descriptor_json` contient ses champs et `callback` traite les commandes concernées. Passez `host->context` comme premier argument. `request` reçoit le nom de méthode annoncé par l’hôte et ses paramètres JSON ; la chaîne retournée reste valide jusqu’à la requête hôte suivante.

Le JSON utilise UTF-8. Conservez les callbacks et leurs états valides et ne laissez jamais traverser une exception à la frontière C. Copiez les chaînes avant que l’appel suivant les invalide. Ne transmettez ni objets STL, ni objets ImGui, ni allocateurs dans l’ABI. Arrêtez et attendez les travaux du plugin avant la fin d’une désactivation ou fermeture. Les bibliothèques restent chargées jusqu’à la destruction de l’hôte.

L’hôte accepte des contributions pour les commandes, menus, panneaux, contrôles de versions, projets, syntaxes, modèles et serveurs de langage. Les abonnements aux événements et les adaptateurs de débogage déclaratifs exigent les bits de capacité et autorisations correspondants. Un descripteur de modèle ou d’adaptateur de débogage n’a pas de callback de commande. Une commande de menu doit désigner une commande fournie par le même plugin.

```cpp
#include <cycode/PluginAPI.h>
#include <stddef.h>
static const char* hello(void*, const char*) {
    return "{\"message\":\"Hello\"}";
}
extern "C" CYCODE_PLUGIN_EXPORT
int cycode_plugin_init_v2(const CyCodeHostV2* host, void** instance) {
    if (!host || !instance || host->abi_version != CYCODE_PLUGIN_ABI_V2 ||
        host->struct_size < offsetof(CyCodeHostV2, features))
        return 1;
    *instance = nullptr;
    return host->contribute(host->context, "command",
        "{\"id\":\"example.plugin.hello\",\"title\":\"Hello\"}", hello);
}
```

Cet exemple exige un manifeste de plugin portant l’ID `example.plugin` et une bibliothèque partagée compatible. Les ID de contribution doivent commencer par cet ID suivi d’un point. Consultez la [gestion des extensions](doc:plugins#development). Les plugins natifs sont du code de confiance exécuté dans l’application.

Un [fournisseur de contrôle de versions personnalisé](doc:custom-vcs#provider) peut exposer ses propres menus et actions.

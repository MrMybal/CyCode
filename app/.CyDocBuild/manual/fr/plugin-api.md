---
id: plugin-api
title: Développer un plugin natif
description: Utilisez les points d’entrée C publics et les contributions JSON UTF-8.
section: Travailler dans CyCode
order: 280
parent: plugins
sourceRevision: 2
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
        "{\"id\":\"hello\",\"title\":\"Hello\"}", hello);
}
```

Cet exemple de point d’entrée nécessite aussi un manifeste de plugin et une compilation en bibliothèque partagée compatible. Consultez la [gestion des extensions](doc:plugins#development). Les plugins natifs sont du code de confiance exécuté dans l’application.

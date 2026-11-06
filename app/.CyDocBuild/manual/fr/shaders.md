---
id: shaders
title: HLSL et GLSL
description: Colorez et compilez un shader sélectionné avec les outils externes.
section: Travailler dans CyCode
order: 180
parent: languages
sourceRevision: 2
---

## Compiler un shader {#compile}

Choisissez la tâche de shader dans les tâches de projet. Sélectionnez le fichier source, l’étape, le point d’entrée ou la cible, ainsi que les chemins d’inclusion et macros adaptés au compilateur. HLSL utilise DXC ; GLSL utilise glslang. Installez le compilateur séparément et examinez sa sortie dans Problèmes.

Une compilation réussie produit une sortie DXIL ou SPIR-V limitée en taille. Aucun aperçu GPU, moteur d’exécution ou débogueur de shaders n’est fourni. GLSL Analyzer propose les opérations d’édition qu’il prend en charge ; les services HLSL dépendent d’un serveur compatible explicitement configuré.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

Cet exemple illustre un texte HLSL. La cible du compilateur et le pipeline graphique doivent être choisis pour le projet réel.

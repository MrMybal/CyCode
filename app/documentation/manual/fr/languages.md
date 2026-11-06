---
id: languages
title: Langages et coloration
description: Langages et coloration — manuel local de l’application
section: Guides
order: 40
sourceRevision: 1
---

## Coloration {#syntax}

Les syntaxes proviennent du catalogue partagé CyUI. Choisissez Automatic ou un langage précis. C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, HLSL, GLSL et les journaux font partie des modes partagés ; le catalogue installé reste la référence pour la liste complète. Les extensions peuvent enregistrer des syntaxes via les contrats documentés.

## Services de langage {#services}

CyCode connecte ses services aux serveurs configurés, avec des capacités variables par backend. Compilation, diagnostic, complétion, débogage et coloration sont des fonctions distinctes. Pour Unreal, l’analyse dépend aussi du contexte moteur/projet et des commandes de compilation. Consultez Languages, Unreal et Roadmap pour les limites exactes.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

Exemple de coloration, pas une preuve de compilation sur cette machine.

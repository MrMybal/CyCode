---
id: languages
title: Languages and highlighting
description: Languages and highlighting — local application manual
section: Guides
order: 40
revision: 1
---

## Highlighting {#syntax}

Syntax modes come from the shared CyUI catalogue. Choose Automatic or a specific mode. Shared modes include C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, HLSL, GLSL and logs; the installed catalogue is authoritative for the complete list. Extensions can register syntaxes through the documented contracts.

## Language services {#services}

CyCode connects services to configured servers, with backend-specific capabilities. Build, diagnostics, completion, debugging and highlighting are separate. Unreal analysis also depends on engine/project context and compile commands. Consult Languages, Unreal and Roadmap for precise limits.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

Highlighting example, not evidence of compiler execution on this machine.

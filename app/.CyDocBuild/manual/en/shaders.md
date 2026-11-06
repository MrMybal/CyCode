---
id: shaders
title: HLSL and GLSL
description: Highlight and compile a selected shader with external tools.
section: Work in CyCode
order: 180
parent: languages
revision: 2
---

## Compile a shader {#compile}

Choose the shader task in Project tasks. Select the source file, stage, entry point/target and include paths/macros appropriate to the selected compiler. HLSL uses DXC; GLSL uses glslang. Install the compiler separately and inspect its output in Problems.

Successful compilation produces bounded DXIL or SPIR-V output. There is no GPU shader preview, runtime or shader debugger. GLSL Analyzer offers supported editing operations; HLSL language-service support depends on an explicitly configured compatible backend.

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    return float4(0.2, 0.6, 1.0, 1.0);
}
```

This example demonstrates HLSL text. Its compiler target and graphics pipeline must be chosen for the actual project.

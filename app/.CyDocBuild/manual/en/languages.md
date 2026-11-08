---
id: languages
title: Highlighting and language services
description: Separate syntax modes from completion, diagnostics and refactoring.
section: Work in CyCode
order: 110
revision: 3
---

## Syntax modes {#syntax}

The shared CyUI catalogue supplies 102 syntax modes, including C/C++, C#, Rust, Java, JavaScript/TypeScript, HTML/CSS, Python, Lua, JSON, Markdown, HLSL, GLSL, Verse and logs. CyCode chooses the syntax from the document path. Log highlighting distinguishes Error and Warning lines.

Syntax remains useful without a compiler or language server. A syntax mode is not a complete parser, project index or debugger.

## Configure a service {#services}

Open the language-server controls in Problems. Review availability, enable/restart/disable the selected server and adjust its project-specific executable arguments/settings. Additional installed servers can start when their files are opened. Inspection alone does not install a toolchain.

| Language | Service |
| --- | --- |
| C/C++ and Unreal | clangd |
| C# | csharp-ls / Roslyn |
| Rust | rust-analyzer |
| Java | Eclipse JDT LS |
| JavaScript / TypeScript | typescript-language-server |
| HTML / CSS | VS Code language servers |
| Python / GLSL | Pyright / GLSL Analyzer |

Capabilities vary by server. HTML does not advertise the same reference/rename operations as C++; GLSL does not provide every symbol operation. See [shader tools](doc:shaders#compile) for compiler requirements.

## Verse {#verse}

Files ending in `.verse`, including generated `.digest.verse` files, select Verse
automatically. Highlighting covers keywords, primitive types, `@editable`
attributes, `<public>` / `<suspends>` specifiers, call names, numbers, strings
and interpolation expressions. `#`, nested `<# ... #>` and indented `<#>`
comments retain their state across lines.

```verse
score_counter := class:
    var Score:int = 0
    Add(Amount:int)<transacts>:void =
        set Score += Amount
```

This syntax mode does not start a Verse language server or compile UEFN projects.
Verse compilation and live Fortnite integration are not included.
See the [Book of Verse](https://verselang.github.io/book/00_overview/) for the language.

# HLSL and GLSL compilation

Project tasks can select `system:shader` alongside the existing language build
adapters. A standalone folder containing shader sources is detected automatically
when it has no primary Cargo/npm/JVM/CMake model. For an existing C++/Unreal
workspace, open View > Project tasks and choose Use shader tasks in this project.
That explicit secondary panel does not replace the primary CMake/MSBuild/Unreal
build shortcuts. Select a workspace-relative source, or choose a detected source.
A single candidate is selected automatically. Include-only `.hlsli` files are not
offered as standalone compilation targets.

The app uses separately installed tools discovered through PATH or explicit
`CYCODE_DXC` / `CYCODE_GLSLANGVALIDATOR` executable overrides. No shader compiler
is downloaded or executed by opening/inspecting a project. Configure saves the
profile and checks the selected source/options without invoking the compiler;
without a selected source it only saves the incomplete profile for later choice.
Build executes the actual compiler through the existing native ProjectTasks
process owner, cancellation token, output limit and configurable total deadline.
GUI Build saves dirty buffers first; native/MCP execution refuses remaining dirty
documents using the same workspace guard as the other project adapters.

| Input | Compiler | Targets |
| --- | --- | --- |
| HLSL | Microsoft DXC | DXIL or Vulkan SPIR-V, Vulkan 1.0–1.3 |
| GLSL | Khronos glslangValidator | SPIR-V for Vulkan 1.0–1.3 target environments or OpenGL |

Stage options are vertex, fragment, geometry, tessellation control/evaluation,
compute, mesh and task. GLSL additionally accepts its ray-tracing stages. Automatic
stage selection uses `.vert`, `.frag`, `.comp`, etc., including compound suffixes
such as `.frag.hlsl` / `.comp.glsl`. A plain `.hlsl` / `.glsl` requires explicit
stage choice. HLSL uses the selected entry point and shader model (default `6_0`),
mapped to the actual DXC target profile. The compiler decides whether that model
supports the selected stage/features. GLSL source entry remains `main`. HLSL
ray-tracing library/export workflows are explicitly refused rather than inferred.

Typed fields in the saved project profile and existing `project_*` MCP/plugin
requests are `shaderSource`, `shaderLanguage`, `shaderStage`, `shaderTarget`,
`shaderEntry`, `shaderModel`, `shaderDefines` and `shaderIncludes`. Definitions
contain unique `NAME` or `NAME=value` entries; include directories are checked
workspace paths. Each option remains one argv argument, including spaces/Unicode.
Application arguments and arbitrary compiler flag injection are not supported by
this adapter. Existing `process.execute` permissions apply to execution.

Build writes a fresh `.cycode/shader-build/<id>/shader.dxil` or `shader.spv` and
only publishes an artifact after successful, noncancelled, nontruncated completion
and bounded size/container-magic checks. Those checks identify the container; the
actual compiler owns semantic validation. Failed partial outputs are not published.
Recognized DXC Clang-format and glslang diagnostics map guarded workspace paths to
Problems; unknown formats/external source locations remain in the build log.
Ambiguous nonzero GLSL numeric source IDs are not mapped to an invented file.

Artifacts describe the current compile result, not a reusable dependency cache.
Inspection does not claim a fresh build from an old output; rebuild explicitly
after include changes. Source discovery is capped at 4096 candidates / 200,000
visited entries / depth 64. Process output is capped at 4 MiB, published diagnostics
at 5000 and the generated artifact at 64 MiB. No shader binary is run as a program.

This implements compilation, not a GPU renderer or live preview. Reflection,
resource binding/material editors, renderer-specific shader permutations,
cross-stage pipeline validation, WGSL, legacy FXC models and Unreal's shader
compilation environment are separate work. Shader Task does not reproduce Unreal
engine-generated macros, virtual includes or ShaderCompileWorker.

Windows production compilation and syntax-only checks cover the planner,
ProjectTasks integration, panel and added contract sources. No DXC/glslang process,
shader test executable, GPU preview or app window was launched during the current
antivirus interruption. Actual tool versions/outputs/diagnostics and graphical
acceptance remain pending; Linux/macOS remain untested.

Primary contracts: [DXC options](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/Support/HLSLOptions.td),
[DXC SPIR-V code generation](https://github.com/microsoft/DirectXShaderCompiler/wiki/SPIR%E2%80%90V-CodeGen),
[glslang standalone options](https://github.com/KhronosGroup/glslang/blob/main/StandAlone/StandAlone.cpp).

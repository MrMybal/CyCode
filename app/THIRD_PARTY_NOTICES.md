# Third-party notices

CyCode's own license has not been selected. No project-wide MIT grant is implied.
Dependencies retain their original licenses. Copies are in licenses/ or the
corresponding runtime package, as detailed below.

| Component | License | Source |
|---|---|---|
| CyUI | MIT | Owner's CyImgui_Interface checkout; public repository not configured |
| Dear ImGui | MIT | https://github.com/ocornut/imgui |
| TextEditor | MIT | https://github.com/goossens/ImGuiColorTextEdit |
| SDL3 | zlib | https://github.com/libsdl-org/SDL |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| CyToolsCore | MIT | Owner's CyToolsCore wheel/check-out; public repository not configured |
| MCP Python SDK | MIT | https://github.com/modelcontextprotocol/python-sdk |
| ripgrep | MIT / Unlicense | https://github.com/BurntSushi/ripgrep |
| libvterm | MIT | https://www.leonerd.org.uk/code/libvterm/ |
| CodeLLDB | MIT | https://github.com/vadimcn/codelldb |
| NetCoreDbg | MIT | https://github.com/Samsung/netcoredbg |
| csharp-ls | MIT | https://github.com/razzmatazz/csharp-language-server |
| Private Microsoft .NET SDK/runtime | Included Microsoft terms and component licenses | https://github.com/dotnet/sdk |
| Bundled LLDB/LLVM | Apache-2.0 with LLVM exceptions | https://github.com/llvm/llvm-project |
| CodeLLDB's embedded Python | PSF and included third-party terms | https://github.com/python/cpython |

Exact downloaded C++ source revisions/archive hashes: dependencies.lock.json.
TextEditor retains its MIT notice. scripts/prepare_texteditor.py creates a build-local
copy with CyCode's lexical color roles/context callback, versioned semantic display
overlays and read-only token queries;
the original downloaded source is preserved.
The private Python environment also includes the dependencies declared by the real
CyToolsCore/MCP wheels. Their installed dist-info metadata and license files are
preserved by pip. They are not replaced by CyCode's future project license.
The private environment and SDK wheel are not committed to this repository.

The build installs a checksum-pinned official ripgrep binary in runtime/tools.
Its exact version, archives and hashes are in runtime-tools.lock.json. The original
COPYING, LICENSE-MIT and UNLICENSE accompany it in runtime/tools/licenses/ripgrep.
Git, CMake, clangd and the C++ runtime remain external development prerequisites.
Segoe UI/Consolas/MDL2 are loaded from Windows when available and are not redistributed.
Other OSes currently use the library's font and icon fallbacks.

The C# server package is pinned in language-tools.lock.json; its own license is
copied to licenses/csharp-ls.txt. Its managed tool package, including Roslyn,
MSBuild, Ionide LSP, FSharp and ILSpy assemblies, is preserved under
runtime/tools/csharp-ls. Upstream dependency licenses remain applicable; a complete
transitive notice audit and distributable runtime package remain prerequisites for
public binary distribution. The private SDK archive's LICENSE.txt and
ThirdPartyNotices.txt are preserved in runtime/tools/csharp-dotnet and copied to
licenses/Dotnet-SDK.txt and licenses/Dotnet-ThirdPartyNotices.txt. Project-specific
SDKs/targeting packs remain development prerequisites.

The optional managed debug adapter is the official NetCoreDbg 3.2.0-1092 release,
pinned with SHA-256 hashes in debugger-tools.lock.json. Its MIT license is copied
to licenses/NetCoreDbg.txt; upstream package contents and component notices remain
in runtime/tools/netcoredbg. No Microsoft Visual Studio debugger is redistributed.

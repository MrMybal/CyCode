# CyCode common native source API 1

Canonical UTF-8 paths, text file I/O and snippet parsing shared by CyCode Core,
CyCodeIDE and CyCodeReader. Extracted byte-for-byte from CyCodeReader
15979a5477c10a967cbd6e78c5cfd58d4966ea69 (`editor/` API 1).
The `cycode` namespace, headers and `cycode_files` / `cycode_snippets` targets
remain compatible. This directory needs only C++20 and the locked nlohmann/json
headers. Add it once with `CYCODE_COMMON_DEPS` set before `add_subdirectory`.
It does not configure Engine, SDL, ImGui, CyUI, Tree-sitter, plugins or tests.
GPL-3.0-only; see the repository LICENSE and COPYRIGHT.

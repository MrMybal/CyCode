#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cycode {
// Declarations found by the native structural scanner. This is a fast lexical
// outline (no preprocessor, no overload or template resolution), intended for
// indexing whole projects without a compilation database. clangd remains the
// owner of exact definitions, references and types.
enum class SymbolKind : uint8_t {
    Namespace = 1, Class, Struct, Union, Enum, Interface, Function, Method, Constructor,
    Destructor, Field, Variable, EnumMember, Macro, TypeAlias, Property
};
const char *scannedSymbolKindName(SymbolKind) noexcept;
std::optional<SymbolKind> symbolKindFromName(std::string_view) noexcept;
bool symbolKindIsContainer(SymbolKind) noexcept;

enum class SourceLanguage : uint8_t { None, CFamily, CSharp, Python };
// By extension: C/C++/Objective-C headers and sources, HLSL/USF/USH, ISPC, CUDA,
// Java/JavaScript-like brace languages map to CFamily; .cs to CSharp; .py to Python.
SourceLanguage sourceLanguageForPath(std::string_view path) noexcept;

struct ScannedSymbol {
    std::string name, scope, detail;
    SymbolKind kind = SymbolKind::Function;
    uint32_t line = 0, column = 0, endLine = 0; // 1-based; byte column of the name
    int32_t parent = -1;                       // index of the enclosing symbol in the same result
    bool declaration = false;                  // prototype/forward member declaration without a body
};

// Bounded: at most 200000 symbols and 64 nesting levels per text; longer
// results are cut (the scanner never throws on malformed source).
std::vector<ScannedSymbol> scanSymbols(std::string_view text, SourceLanguage language);
// #include / #import targets (C family) or import modules (Python), in order.
std::vector<std::pair<uint32_t, std::string>> scanIncludes(std::string_view text, SourceLanguage language);
} // namespace cycode

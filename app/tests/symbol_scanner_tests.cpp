// Structural scanner regressions found on Unreal sources (CyAtlas localization trial, 2026-11-12).
#include "cycode/SymbolScanner.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace cycode;

static void require(bool value, const std::string &message) {
    if (!value)
        throw std::runtime_error(message);
}
static const ScannedSymbol *find(const std::vector<ScannedSymbol> &symbols, std::string_view name, uint32_t line) {
    for (const auto &symbol : symbols)
        if (symbol.name == name && symbol.line == line)
            return &symbol;
    return nullptr;
}
static size_t count(const std::vector<ScannedSymbol> &symbols, std::string_view name) {
    size_t n = 0;
    for (const auto &symbol : symbols)
        n += symbol.name == name;
    return n;
}

int main() {
    try {
        // 1. A comma inside the template arguments of a return type is part of the type.
        const std::string box = "namespace Chaos\n"                                   // 1
                                "{\n"                                                 // 2
                                "\ttemplate<class T, int d>\n"                        // 3
                                "\tclass TBox final : public FImplicitObject\n"       // 4
                                "\t{\n"                                               // 5
                                "\tpublic:\n"                                         // 6
                                "\t\tTArray<TVector<T, d>> ComputeSamplePoints() const\n" // 7
                                "\t\t{\n"                                             // 8
                                "\t\t\treturn BoundingBox().ComputeSamplePoints();\n" // 9
                                "\t\t}\n"                                             // 10
                                "\t\tTMap<FName, int32> Counts() const;\n"            // 11
                                "\t\tTFunction<void(int32)> MakeVisitor() const { return {}; }\n" // 12
                                "\t\tTVector<T, 3> Clamp(const TVector<T, 3>& X) const;\n" // 13
                                "\t};\n"                                              // 14
                                "\tvoid Use(TBox<float, 3>& Box)\n"                   // 15
                                "\t{\n"                                               // 16
                                "\t\tFoo(A, B);\n"                                    // 17
                                "\t}\n"                                               // 18
                                "\tstatic TAutoConsoleVariable<int32> CVarX(TEXT(\"p.X\"), 1, TEXT(\"x\"));\n" // 19
                                "}\n";
        const auto symbols = scanSymbols(box, SourceLanguage::CFamily);
        const auto *sample = find(symbols, "ComputeSamplePoints", 7);
        require(sample && sample->kind == SymbolKind::Method && !sample->declaration && sample->scope == "Chaos::TBox",
                "TArray<TVector<T, d>> return type must keep the method");
        const auto *counts = find(symbols, "Counts", 11);
        require(counts && counts->declaration, "TMap<K, V> declaration must be recorded");
        require(find(symbols, "MakeVisitor", 12), "TFunction<void(int32)> return type must keep the method");
        const auto *clamp = find(symbols, "Clamp", 13);
        require(clamp && clamp->kind == SymbolKind::Method && clamp->declaration,
                "a literal inside parameter template arguments is a type, not a constructor argument");
        require(count(symbols, "Foo") == 0, "a call inside a body is not a declaration");
        require(find(symbols, "Use", 15), "free function after the class");
        const auto *cvar = find(symbols, "CVarX", 19);
        require(cvar && cvar->kind == SymbolKind::Variable, "a console variable constructed with literals stays a variable");

        // 2. An elaborated type specifier in a parameter list does not define a class.
        const std::string sphere = "namespace Chaos\n"                                          // 1
                                   "{\n"                                                        // 2
                                   "\ttemplate<typename T, int d>\n"                            // 3
                                   "\tstruct TSphereSpecializeSamplingHelper\n"                 // 4
                                   "\t{\n"                                                      // 5
                                   "\t\tstatic FORCEINLINE void ComputeSamplePoints(TArray<TVec3<T>>& Points, "
                                   "const class TSphere<T, 3>& Sphere, const int32 NumPoints)\n" // 6
                                   "\t\t{\n"                                                    // 7
                                   "\t\t}\n"                                                    // 8
                                   "\t};\n"                                                     // 9
                                   "\ttemplate<class T, int d>\n"                               // 10
                                   "\tclass TSphere final : public FImplicitObject\n"           // 11
                                   "\t{\n"                                                      // 12
                                   "\t};\n"                                                     // 13
                                   "\tstruct FKeep* MakeKeep();\n"                              // 14
                                   "}\n";
        const auto more = scanSymbols(sphere, SourceLanguage::CFamily);
        const auto *helper = find(more, "ComputeSamplePoints", 6);
        require(helper && helper->kind == SymbolKind::Method && helper->scope == "Chaos::TSphereSpecializeSamplingHelper",
                "method with a class-keyword parameter must belong to its struct");
        require(count(more, "TSphere") == 1 && find(more, "TSphere", 11), "only the real TSphere definition is a class");
        require(count(more, "FKeep") == 0 && find(more, "MakeKeep", 14), "struct X* f() stays a function declaration");
        std::cout << "symbol scanner regressions: ok\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

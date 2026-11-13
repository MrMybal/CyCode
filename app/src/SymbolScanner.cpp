#include "cycode/SymbolScanner.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace cycode {
namespace {
constexpr size_t maximumSymbols = 200000;
constexpr size_t maximumDepth = 64;
constexpr size_t maximumDetail = 200;

constexpr std::array<std::pair<SymbolKind, const char *>, 16> kindNames{{
    {SymbolKind::Namespace, "namespace"}, {SymbolKind::Class, "class"}, {SymbolKind::Struct, "struct"},
    {SymbolKind::Union, "union"}, {SymbolKind::Enum, "enum"}, {SymbolKind::Interface, "interface"},
    {SymbolKind::Function, "function"}, {SymbolKind::Method, "method"},
    {SymbolKind::Constructor, "constructor"}, {SymbolKind::Destructor, "destructor"},
    {SymbolKind::Field, "field"}, {SymbolKind::Variable, "variable"}, {SymbolKind::EnumMember, "enumMember"},
    {SymbolKind::Macro, "macro"}, {SymbolKind::TypeAlias, "typeAlias"}, {SymbolKind::Property, "property"},
}};

bool identStart(unsigned char c) { return std::isalpha(c) || c == '_' || c == '$' || c >= 0x80; }
bool identPart(unsigned char c) { return std::isalnum(c) || c == '_' || c == '$' || c >= 0x80; }

enum class TokenType : uint8_t { Ident, Number, Punct, String };
struct Token {
    TokenType type;
    uint32_t offset, length, line, column;
};

struct Lexer {
    std::string_view text;
    SourceLanguage language;
    std::vector<Token> tokens;
    std::vector<ScannedSymbol> macros;
    size_t at = 0;
    uint32_t line = 1;
    size_t lineStart = 0;
    bool lineHasCode = false;

    void newline() {
        ++line;
        lineStart = at + 1;
        lineHasCode = false;
    }
    void skipToLineEnd(bool continuation) {
        while (at < text.size()) {
            if (text[at] == '\n') {
                if (continuation && at > 0 && (text[at - 1] == '\\' || (text[at - 1] == '\r' && at > 1 && text[at - 2] == '\\'))) {
                    newline();
                    ++at;
                    continue;
                }
                return;
            }
            ++at;
        }
    }
    void skipBlockComment() {
        at += 2;
        while (at + 1 < text.size() && !(text[at] == '*' && text[at + 1] == '/')) {
            if (text[at] == '\n') newline();
            ++at;
        }
        at = std::min(text.size(), at + 2);
    }
    void skipQuoted(char quote, bool verbatim) {
        ++at;
        while (at < text.size()) {
            const char c = text[at];
            if (c == '\n') {
                if (!verbatim) return; // unterminated ordinary literal ends at the line
                newline();
            } else if (c == '\\' && !verbatim) {
                if (at + 1 < text.size() && text[at + 1] == '\n') {
                    ++at;
                    newline();
                }
                at += 2;
                continue;
            } else if (c == quote) {
                if (verbatim && at + 1 < text.size() && text[at + 1] == quote) {
                    at += 2;
                    continue;
                }
                ++at;
                return;
            }
            ++at;
        }
    }
    bool skipRawString() {
        // R"delimiter( ... )delimiter"
        size_t open = at + 2, paren = text.find('(', open);
        if (paren == std::string_view::npos || paren - open > 16) return false;
        const std::string closing = ")" + std::string(text.substr(open, paren - open)) + "\"";
        const auto end = text.find(closing, paren + 1);
        const auto stop = end == std::string_view::npos ? text.size() : end + closing.size();
        for (size_t i = at; i < stop; ++i)
            if (text[i] == '\n') {
                at = i;
                newline();
            }
        at = stop;
        return true;
    }
    void directive() {
        // at points to '#'
        ++at;
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) ++at;
        const size_t nameStart = at;
        while (at < text.size() && identPart(static_cast<unsigned char>(text[at]))) ++at;
        const auto name = text.substr(nameStart, at - nameStart);
        if (name == "define") {
            while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) ++at;
            const size_t macroStart = at;
            while (at < text.size() && identPart(static_cast<unsigned char>(text[at]))) ++at;
            if (at > macroStart && macros.size() < maximumSymbols) {
                ScannedSymbol symbol;
                symbol.name = std::string(text.substr(macroStart, at - macroStart));
                symbol.kind = SymbolKind::Macro;
                symbol.line = symbol.endLine = line;
                symbol.column = static_cast<uint32_t>(macroStart - lineStart + 1);
                size_t end = at;
                while (end < text.size() && text[end] != '\n' && end - macroStart < maximumDetail) ++end;
                symbol.detail = std::string(text.substr(macroStart, end - macroStart));
                while (!symbol.detail.empty() && (symbol.detail.back() == '\r' || symbol.detail.back() == '\\' ||
                                                  symbol.detail.back() == ' '))
                    symbol.detail.pop_back();
                macros.push_back(std::move(symbol));
            }
        }
        // Preprocessor lines may contain apostrophes or comments; skip them as a unit.
        while (at < text.size()) {
            if (text[at] == '\n') {
                if (at > 0 && (text[at - 1] == '\\' || (text[at - 1] == '\r' && at > 1 && text[at - 2] == '\\'))) {
                    newline();
                    ++at;
                    continue;
                }
                break;
            }
            if (text[at] == '/' && at + 1 < text.size() && text[at + 1] == '*') {
                skipBlockComment();
                continue;
            }
            if (text[at] == '/' && at + 1 < text.size() && text[at + 1] == '/') {
                skipToLineEnd(true);
                break;
            }
            ++at;
        }
    }
    void push(TokenType type, size_t start, size_t length) {
        tokens.push_back({type, static_cast<uint32_t>(start), static_cast<uint32_t>(length), line,
                          static_cast<uint32_t>(start - lineStart + 1)});
        lineHasCode = true;
    }
    void run() {
        const bool csharp = language == SourceLanguage::CSharp;
        while (at < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[at]);
            if (c == '\n') {
                newline();
                ++at;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
                ++at;
                continue;
            }
            if (c == '/' && at + 1 < text.size() && text[at + 1] == '/') {
                skipToLineEnd(!csharp);
                continue;
            }
            if (c == '/' && at + 1 < text.size() && text[at + 1] == '*') {
                skipBlockComment();
                continue;
            }
            if (c == '#' && !lineHasCode) {
                directive();
                continue;
            }
            if (c == '"' || c == '\'') {
                const size_t start = at;
                skipQuoted(static_cast<char>(c), false);
                push(TokenType::String, start, at - start);
                continue;
            }
            if (csharp && (c == '@' || c == '$') && at + 1 < text.size()) {
                size_t quote = at + 1;
                if (quote < text.size() && (text[quote] == '@' || text[quote] == '$')) ++quote;
                if (quote < text.size() && text[quote] == '"') {
                    const size_t start = at;
                    const bool verbatim = text.substr(at, quote - at).find('@') != std::string_view::npos;
                    at = quote;
                    skipQuoted('"', verbatim);
                    push(TokenType::String, start, at - start);
                    continue;
                }
            }
            if (identStart(c)) {
                const size_t start = at;
                while (at < text.size() && identPart(static_cast<unsigned char>(text[at]))) ++at;
                // Raw/prefixed string literals: R"..", u8R"..", L"..", u8"..".
                if (!csharp && at < text.size() && text[at] == '"') {
                    const auto prefix = text.substr(start, at - start);
                    if (prefix == "R" || prefix == "u8R" || prefix == "LR" || prefix == "uR" || prefix == "UR") {
                        at -= 1;
                        if (skipRawString()) {
                            push(TokenType::String, start, at - start);
                            continue;
                        }
                        ++at;
                    } else if (prefix == "L" || prefix == "u8" || prefix == "u" || prefix == "U") {
                        skipQuoted('"', false);
                        push(TokenType::String, start, at - start);
                        continue;
                    }
                }
                push(TokenType::Ident, start, at - start);
                continue;
            }
            if (std::isdigit(c)) {
                const size_t start = at;
                while (at < text.size() && (identPart(static_cast<unsigned char>(text[at])) || text[at] == '.' ||
                                            text[at] == '\''))
                    ++at;
                push(TokenType::Number, start, at - start);
                continue;
            }
            if ((c == ':' && at + 1 < text.size() && text[at + 1] == ':') ||
                (c == '-' && at + 1 < text.size() && text[at + 1] == '>')) {
                push(TokenType::Punct, at, 2);
                at += 2;
                continue;
            }
            push(TokenType::Punct, at, 1);
            ++at;
        }
    }
};

const std::unordered_set<std::string_view> &controlWords() {
    static const std::unordered_set<std::string_view> words{
        "if", "for", "while", "switch", "catch", "return", "sizeof", "alignof", "decltype", "static_assert",
        "alignas", "throw", "new", "delete", "else", "do", "case", "goto", "typeid", "noexcept", "requires",
        "co_return", "co_await", "co_yield", "foreach", "using", "lock", "fixed", "checked", "unchecked",
        "nameof", "typeof", "default", "when", "and", "or", "not", "__declspec", "__attribute__", "defined",
        "_Pragma", "__pragma", "operator", "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast"};
    return words;
}
const std::unordered_set<std::string_view> &qualifierWords() {
    static const std::unordered_set<std::string_view> words{
        "const", "noexcept", "override", "final", "volatile", "throw", "mutable", "&", "&&", "where",
        "requires", "try", "abstract", "sealed", "new", "async"};
    return words;
}
bool macroLike(std::string_view name) {
    bool letter = false;
    for (const char c : name) {
        if (std::islower(static_cast<unsigned char>(c))) return false;
        letter |= std::isupper(static_cast<unsigned char>(c)) != 0;
    }
    return letter && name.size() > 1;
}

struct Scope {
    enum Type { Namespace, TypeBody, EnumBody, Block, Transparent, Initializer } type;
    std::string name;
    int32_t symbol = -1;
    SymbolKind typeKind = SymbolKind::Namespace;
};

class CFamilyParser {
  public:
    CFamilyParser(std::string_view text, const std::vector<Token> &tokens, SourceLanguage language)
        : text_(text), tokens_(tokens), csharp_(language == SourceLanguage::CSharp) {}
    std::vector<ScannedSymbol> run() {
        size_t start = 0;
        for (size_t i = 0; i < tokens_.size() && symbols_.size() < maximumSymbols; ++i) {
            const auto value = view(i);
            const bool punct = tokens_[i].type == TokenType::Punct;
            if (punct && value == "(") {
                ++parens_;
                continue;
            }
            if (punct && value == ")") {
                if (parens_) --parens_;
                continue;
            }
            if (punct && value == "[") {
                ++brackets_;
                continue;
            }
            if (punct && value == "]") {
                if (brackets_) --brackets_;
                continue;
            }
            if (parens_ || brackets_) continue;
            if (punct && value == "{") {
                if (!open(start, i)) start = i + 1;
            } else if (punct && value == "}") {
                if (top() == Scope::EnumBody) enumerator(start, i);
                const bool continues = top() == Scope::Initializer;
                close(i);
                if (!continues) start = i + 1;
            } else if (punct && value == ";") {
                if (top() == Scope::EnumBody) enumerator(start, i);
                else if (top() != Scope::Block) statement(start, i);
                start = i + 1;
            } else if (punct && value == ",") {
                if (top() == Scope::EnumBody) {
                    enumerator(start, i);
                    start = i + 1;
                }
            } else if (punct && value == ":" && top() == Scope::TypeBody && i > start &&
                       (view(i - 1) == "public" || view(i - 1) == "private" || view(i - 1) == "protected" ||
                        view(i - 1) == "signals" || view(i - 1) == "slots")) {
                start = i + 1; // access specifier, possibly after GENERATED_BODY()
            }
        }
        return std::move(symbols_);
    }

  private:
    std::string_view text_;
    const std::vector<Token> &tokens_;
    bool csharp_;
    std::vector<Scope> scopes_;
    std::vector<ScannedSymbol> symbols_;
    size_t parens_ = 0, brackets_ = 0;

    std::string_view view(size_t i) const { return text_.substr(tokens_[i].offset, tokens_[i].length); }
    bool is(size_t i, std::string_view value) const { return i < tokens_.size() && view(i) == value; }
    bool ident(size_t i) const { return i < tokens_.size() && tokens_[i].type == TokenType::Ident; }
    Scope::Type top() const { return scopes_.empty() ? Scope::Namespace : scopes_.back().type; }
    bool inBody() const {
        for (const auto &scope : scopes_)
            if (scope.type == Scope::Block || scope.type == Scope::Initializer) return true;
        return false;
    }
    std::string qualifiedScope() const {
        std::string result;
        for (const auto &scope : scopes_) {
            if (scope.name.empty() || scope.type == Scope::Block || scope.type == Scope::Transparent) continue;
            if (!result.empty()) result += csharp_ ? "." : "::";
            result += scope.name;
        }
        return result;
    }
    int32_t parentSymbol() const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope)
            if (scope->symbol >= 0) return scope->symbol;
        return -1;
    }
    std::string slice(size_t first, size_t last) const { // tokens [first,last)
        if (first >= last) return {};
        const auto begin = tokens_[first].offset;
        const auto end = tokens_[last - 1].offset + tokens_[last - 1].length;
        std::string result;
        result.reserve(std::min<size_t>(end - begin, maximumDetail));
        bool space = false;
        for (size_t i = begin; i < end && result.size() < maximumDetail; ++i) {
            const char c = text_[i];
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                space = !result.empty();
                continue;
            }
            if (space) result += ' ';
            space = false;
            result += c;
        }
        return result;
    }
    void push(Scope::Type type, std::string name = {}, int32_t symbol = -1, SymbolKind kind = SymbolKind::Namespace) {
        if (scopes_.size() >= maximumDepth) {
            // Deep nesting: keep counting braces without recording deeper scopes.
            scopes_.push_back({Scope::Block, {}, -1, kind});
            return;
        }
        scopes_.push_back({type, std::move(name), symbol, kind});
    }
    int32_t emit(ScannedSymbol symbol) {
        if (symbols_.size() >= maximumSymbols) return -1;
        symbol.parent = parentSymbol();
        symbol.scope = qualifiedScope();
        if (!symbol.endLine) symbol.endLine = symbol.line;
        symbols_.push_back(std::move(symbol));
        return static_cast<int32_t>(symbols_.size() - 1);
    }
    ScannedSymbol symbolAt(size_t token, SymbolKind kind, std::string detail = {}) const {
        ScannedSymbol symbol;
        symbol.name = std::string(view(token));
        symbol.kind = kind;
        symbol.line = tokens_[token].line;
        symbol.column = tokens_[token].column;
        symbol.detail = std::move(detail);
        return symbol;
    }
    // Skips leading attribute/macro prefixes: [[...]], [Attr(...)] (C#), UPROPERTY(...),
    // API macros, template<...> headers and C++/C# modifiers that never name the entity.
    size_t skipPrefix(size_t first, size_t last) const {
        for (;;) {
            const size_t before = first;
            if (first < last && is(first, "[")) {
                size_t depth = 0;
                for (; first < last; ++first) {
                    if (is(first, "[")) ++depth;
                    else if (is(first, "]") && --depth == 0) {
                        ++first;
                        break;
                    }
                }
            }
            if (first + 1 < last && ident(first) && is(first + 1, "(") && macroLike(view(first))) {
                size_t depth = 0, i = first + 1;
                for (; i < last; ++i) {
                    if (is(i, "(")) ++depth;
                    else if (is(i, ")") && --depth == 0) break;
                }
                if (i < last) first = i + 1;
            }
            if (first < last && is(first, "template") && first + 1 < last && is(first + 1, "<")) {
                size_t depth = 0, i = first + 1;
                for (; i < last; ++i) {
                    if (is(i, "<")) ++depth;
                    else if (is(i, ">") && --depth == 0) break;
                    else if (is(i, "(")) { // template<int N = (1 > 0)> is rare; stop safely
                        while (i < last && !is(i, ")")) ++i;
                    }
                }
                first = std::min(last, i + 1);
            }
            if (first == before) return first;
        }
    }
    // Finds the parameter list of a function-like statement: identifier '(' at depth 0
    // whose closing ')' is followed only by qualifiers, an initializer list, '= 0/default/delete',
    // a trailing return type or nothing.
    bool functionShape(size_t first, size_t last, bool definition, size_t &nameToken, size_t &closeParen) const {
        size_t angles = 0;
        for (size_t i = first; i < last; ++i) {
            if (is(i, "<")) ++angles;
            else if (is(i, ">") && angles) --angles;
            if (!is(i, "(") || i == first) continue;
            size_t name = i - 1;
            if (!ident(name) && !is(name, ">")) continue;
            if (is(name, ">")) continue; // template-id call or cast; not a declarator name we can name reliably
            const auto label = view(name);
            if (controlWords().contains(label)) {
                if (label == "operator") continue;
                return false;
            }
            // operator overloads: "operator ==(" / "operator()(" / "operator new("
            size_t depth = 0, close = i;
            for (; close < last; ++close) {
                if (is(close, "(")) ++depth;
                else if (is(close, ")") && --depth == 0) break;
            }
            if (close >= last) return false;
            if (angles) continue; // inside template arguments: TFunction<void(int)>
            size_t after = close + 1;
            bool valid = true;
            while (after < last) {
                const auto token = view(after);
                if (qualifierWords().contains(token) || is(after, "&") || is(after, "*")) {
                    if ((token == "throw" || token == "noexcept" || token == "requires") && is(after + 1, "(")) {
                        size_t nested = 0;
                        for (++after; after < last; ++after) {
                            if (is(after, "(")) ++nested;
                            else if (is(after, ")") && --nested == 0) break;
                        }
                    }
                    ++after;
                    continue;
                }
                if (token == "->" || token == ":" || token == "=>") break; // trailing type, init list, C# body
                if (token == "=" && !definition) break;
                if (ident(after) && macroLike(token)) { // UE/MSVC trailing macros, e.g. PURE_VIRTUAL handled by '='
                    ++after;
                    if (is(after, "(")) {
                        size_t nested = 0;
                        for (; after < last; ++after) {
                            if (is(after, "(")) ++nested;
                            else if (is(after, ")") && --nested == 0) break;
                        }
                        ++after;
                    }
                    continue;
                }
                valid = false;
                break;
            }
            if (!valid) continue;
            nameToken = name;
            closeParen = close;
            return true;
        }
        return false;
    }
    bool function(size_t first, size_t last, bool definition) {
        first = skipPrefix(first, last);
        size_t name = 0, close = 0;
        if (first >= last || !functionShape(first, last, definition, name, close)) return false;
        // Build A::B::name, also ~Name and operator forms.
        size_t qualifiedStart = name;
        std::string prefix;
        if (qualifiedStart > first && is(qualifiedStart - 1, "~")) --qualifiedStart;
        while (qualifiedStart >= first + 2 && is(qualifiedStart - 1, "::") && ident(qualifiedStart - 2))
            qualifiedStart -= 2;
        if (view(name) == "operator" || (name > first && is(name - 1, "operator"))) return false;
        const bool destructor = name > first && is(name - 1, "~");
        std::vector<std::string> qualifiers;
        for (size_t i = qualifiedStart; i < name; ++i)
            if (ident(i)) qualifiers.emplace_back(view(i));
        const bool hasReturnType = qualifiedStart > first;
        const std::string label(view(name));
        const bool inType = top() == Scope::TypeBody;
        const std::string owner = inType ? scopes_.back().name : (qualifiers.empty() ? std::string{} : qualifiers.back());
        const bool constructor = !destructor && !owner.empty() && label == owner;
        if (!hasReturnType && !constructor && !destructor && qualifiers.empty())
            return false; // macro invocation such as IMPLEMENT_MODULE(...)
        if (hasReturnType) {
            // Statements that are expressions, not declarations. Commas and parentheses inside
            // template arguments belong to the return type: TArray<TVector<T, d>> F(),
            // TMap<K, V> G(), TFunction<void(int)> H().
            size_t angles = 0;
            for (size_t i = first; i < qualifiedStart; ++i) {
                if (is(i, "<")) ++angles;
                else if (is(i, ">") && angles) --angles;
                else if (is(i, "=") || is(i, ".") || is(i, "->") || is(i, "return") ||
                         (!angles && (is(i, "(") || is(i, ","))))
                    return false;
            }
        }
        if (!definition && hasReturnType && !destructor) {
            // "TAutoConsoleVariable<bool> CVarX(TEXT("p.X"), true, ...);" constructs a variable:
            // a literal argument that is not a parameter default ("= value") is not a declaration.
            // Literals inside template arguments are types: Clamp(const TVector<T, 3>& X) const;
            size_t argumentAngles = 0;
            for (size_t i = name + 2; i < close; ++i) {
                if (is(i, "<")) ++argumentAngles;
                else if (is(i, ">") && argumentAngles) --argumentAngles;
                const bool literal = tokens_[i].type == TokenType::String || tokens_[i].type == TokenType::Number ||
                                     is(i, "true") || is(i, "false") || is(i, "nullptr");
                if (literal && !argumentAngles && !is(i - 1, "=")) {
                    emit(symbolAt(name, top() == Scope::TypeBody ? SymbolKind::Field : SymbolKind::Variable,
                                  slice(first, close + 1)));
                    return true;
                }
            }
        }
        SymbolKind kind = SymbolKind::Function;
        if (destructor) kind = SymbolKind::Destructor;
        else if (constructor) kind = SymbolKind::Constructor;
        else if (inType || !qualifiers.empty()) kind = SymbolKind::Method;
        auto symbol = symbolAt(name, kind, slice(first, close + 1));
        if (destructor) symbol.name = "~" + symbol.name;
        symbol.declaration = !definition;
        int32_t index = -1;
        if (!qualifiers.empty() && !inType) {
            // Out-of-class definition: record the written qualification as scope.
            std::string written;
            for (const auto &part : qualifiers) {
                if (!written.empty()) written += csharp_ ? "." : "::";
                written += part;
            }
            index = emit(std::move(symbol));
            if (index >= 0) {
                auto &stored = symbols_[static_cast<size_t>(index)];
                stored.scope = stored.scope.empty() ? written : stored.scope + (csharp_ ? "." : "::") + written;
            }
        } else
            index = emit(std::move(symbol));
        if (definition) push(Scope::Block, {}, index);
        return true;
    }
    bool typeDefinition(size_t first, size_t last, bool definition) {
        first = skipPrefix(first, last);
        size_t parens = 0;
        for (size_t i = first; i < last; ++i) {
            const auto word = view(i);
            if (word == "(") ++parens;
            else if (word == ")" && parens) --parens;
            // An elaborated type specifier in a parameter list, "F(const class TSphere<T, 3>& S) {",
            // names a type; it does not define one.
            if (parens) continue;
            SymbolKind kind;
            if (word == "class") kind = SymbolKind::Class;
            else if (word == "struct") kind = SymbolKind::Struct;
            else if (word == "union") kind = SymbolKind::Union;
            else if (word == "enum") kind = SymbolKind::Enum;
            else if (csharp_ && (word == "interface")) kind = SymbolKind::Interface;
            else if (csharp_ && word == "record") kind = SymbolKind::Class;
            else if (word == "__interface") kind = SymbolKind::Interface;
            else continue;
            if (i > first && (is(i - 1, "<") || is(i - 1, ",") || is(i - 1, "(") || is(i - 1, "friend"))) return false;
            size_t j = i + 1;
            if (kind == SymbolKind::Enum && (is(j, "class") || is(j, "struct"))) ++j;
            size_t nameToken = 0;
            bool named = false;
            for (; j < last; ++j) {
                if (is(j, "(")) return false; // struct X* f() — a function returning a type
                if (is(j, ":") || is(j, "<") || is(j, "where") || is(j, "{")) break;
                if (ident(j) && view(j) != "final" && view(j) != "sealed" && view(j) != "abstract" &&
                    view(j) != "partial" && view(j) != "alignas") {
                    nameToken = j;
                    named = true;
                }
                if (is(j, "::")) named = false; // nested-name: keep the last component
            }
            if (!named && j < last && is(j, "<") == false && nameToken == 0) {
                // anonymous struct/enum: still a scope
                if (definition) push(kind == SymbolKind::Enum ? Scope::EnumBody : Scope::TypeBody, {}, -1, kind);
                return definition;
            }
            if (!definition) return true; // forward declaration: no symbol
            const auto end = std::min(last, j);
            auto symbol = symbolAt(nameToken, kind, slice(i, std::max(end, nameToken + 1)));
            const auto index = emit(std::move(symbol));
            push(kind == SymbolKind::Enum ? Scope::EnumBody : Scope::TypeBody, std::string(view(nameToken)), index, kind);
            return true;
        }
        return false;
    }
    // Returns true when the brace belongs to the current statement (an initializer).
    bool open(size_t first, size_t brace) {
        if (inBody() || top() == Scope::EnumBody) {
            push(Scope::Block);
            return false;
        }
        const size_t start = skipPrefix(first, brace);
        if (start < brace && is(start, "namespace")) {
            std::vector<size_t> names;
            for (size_t i = start + 1; i < brace; ++i)
                if (ident(i) && view(i) != "inline") names.push_back(i);
            if (names.empty()) {
                push(Scope::Transparent);
                return false;
            }
            for (size_t n = 0; n < names.size(); ++n) {
                const auto index = emit(symbolAt(names[n], SymbolKind::Namespace, slice(start, brace)));
                if (n + 1 < names.size()) push(Scope::Transparent, std::string(view(names[n])), index);
                else push(Scope::Namespace, std::string(view(names[n])), index);
            }
            // C++17 nested namespaces open several scopes with one brace; collapse them.
            if (names.size() > 1) {
                std::string joined;
                for (size_t n = 0; n < names.size(); ++n) {
                    if (n) joined += csharp_ ? "." : "::";
                    joined += view(names[n]);
                }
                const auto last = scopes_.back().symbol;
                scopes_.resize(scopes_.size() - names.size());
                push(Scope::Namespace, joined, last);
            }
            return false;
        }
        if (start < brace && is(start, "extern") && (brace == start + 1 || (brace == start + 2 && tokens_[start + 1].type == TokenType::String))) {
            push(Scope::Transparent);
            return false;
        }
        if (typeDefinition(first, brace, true)) return false;
        // Constructor initializer braces "Foo::Foo() : A{1}, B{2} {" continue the statement.
        if (brace > start && (ident(brace - 1) || is(brace - 1, ">"))) {
            for (size_t i = start; i + 1 < brace; ++i)
                if (is(i, ")") && is(i + 1, ":")) {
                    push(Scope::Initializer);
                    return true;
                }
        }
        if (function(first, brace, true)) return false;
        // C# property "Type Name { get; set; }" or C++ brace-initialized member "int X{0}".
        if (top() == Scope::TypeBody && start + 1 < brace && ident(brace - 1)) {
            bool plain = true;
            for (size_t i = start; i < brace; ++i)
                if (is(i, "(") || is(i, "=")) plain = false;
            if (plain && !controlWords().contains(view(brace - 1)) && !macroLike(view(brace - 1))) {
                const auto index = emit(symbolAt(brace - 1, csharp_ ? SymbolKind::Property : SymbolKind::Field,
                                                 slice(start, brace)));
                push(Scope::Block, {}, csharp_ ? index : -1);
                return false;
            }
        }
        push(Scope::Block);
        return false;
    }
    void close(size_t brace) {
        if (scopes_.empty()) return;
        const auto scope = scopes_.back();
        scopes_.pop_back();
        if (scope.symbol >= 0 && static_cast<size_t>(scope.symbol) < symbols_.size())
            symbols_[static_cast<size_t>(scope.symbol)].endLine = tokens_[brace].line;
        // Collapsed nested namespaces were recorded as one scope; nothing else to close.
    }
    void enumerator(size_t first, size_t last) {
        first = skipPrefix(first, last);
        if (first < last && ident(first))
            emit(symbolAt(first, SymbolKind::EnumMember, slice(first, last)));
    }
    void statement(size_t first, size_t last) {
        first = skipPrefix(first, last);
        if (first >= last) return;
        const auto head = view(first);
        if (head == "using") {
            // using Alias = Type;   (using namespace / using-declarations are not symbols)
            if (first + 2 < last && ident(first + 1) && is(first + 2, "="))
                emit(symbolAt(first + 1, SymbolKind::TypeAlias, slice(first, last)));
            return;
        }
        if (head == "typedef") {
            // typedef A B;  typedef void (*Name)(int);
            for (size_t i = first + 1; i + 2 < last; ++i)
                if (is(i, "(") && is(i + 1, "*") && ident(i + 2)) {
                    emit(symbolAt(i + 2, SymbolKind::TypeAlias, slice(first, last)));
                    return;
                }
            if (ident(last - 1)) emit(symbolAt(last - 1, SymbolKind::TypeAlias, slice(first, last)));
            return;
        }
        if (head == "friend" || head == "return" || head == "namespace" || head == "goto" || head == "break" ||
            head == "continue" || head == "static_assert" || head == "#")
            return;
        if (typeDefinition(first, last, false)) return; // forward declaration
        if (function(first, last, false)) return;
        // Field / variable: type tokens followed by name [= init | [N] | : bits].
        size_t end = last, angles = 0;
        for (size_t i = first; i < last; ++i) {
            if (is(i, "<")) ++angles;
            else if (is(i, ">") && angles) --angles;
            else if (!angles && (is(i, "=") || is(i, "[") || (is(i, ":") && top() == Scope::TypeBody))) {
                end = i;
                break;
            } else if (!angles && is(i, "(")) return; // call or unrecognised declarator
        }
        if (end <= first + 1 || !ident(end - 1)) return;
        const auto name = view(end - 1);
        if (controlWords().contains(name) || macroLike(name)) return;
        const auto kind = top() == Scope::TypeBody ? SymbolKind::Field : SymbolKind::Variable;
        if (kind == SymbolKind::Variable && top() != Scope::Namespace && top() != Scope::Transparent) return;
        // Several declarators: "int A, B;"
        size_t segment = first;
        bool emitted = false;
        for (size_t i = first; i < end; ++i)
            if (is(i, ",") && i > segment && ident(i - 1)) {
                emit(symbolAt(i - 1, kind, slice(first, i)));
                emitted = true;
                segment = i + 1;
            }
        (void)emitted;
        emit(symbolAt(end - 1, kind, slice(first, end)));
    }
};

std::vector<ScannedSymbol> scanPython(std::string_view text) {
    std::vector<ScannedSymbol> symbols;
    struct Open {
        size_t indent;
        int32_t symbol;
    };
    std::vector<Open> stack;
    uint32_t line = 0, lastCode = 0;
    size_t at = 0;
    bool triple = false;
    char tripleQuote = 0;
    while (at <= text.size() && symbols.size() < maximumSymbols) {
        const auto newline = text.find('\n', at);
        const auto end = newline == std::string_view::npos ? text.size() : newline;
        auto current = text.substr(at, end - at);
        ++line;
        at = end + 1;
        if (!current.empty() && current.back() == '\r') current.remove_suffix(1);
        // Skip docstring/triple-quoted bodies.
        auto scanTriple = [&](std::string_view part) {
            for (size_t i = 0; i + 2 < part.size(); ++i)
                if ((part[i] == '"' || part[i] == '\'') && part[i + 1] == part[i] && part[i + 2] == part[i]) {
                    if (!triple) {
                        triple = true;
                        tripleQuote = part[i];
                    } else if (part[i] == tripleQuote)
                        triple = false;
                    i += 2;
                }
        };
        const bool wasTriple = triple;
        scanTriple(current);
        if (wasTriple) {
            if (newline == std::string_view::npos) break;
            continue;
        }
        size_t indent = 0;
        while (indent < current.size() && (current[indent] == ' ' || current[indent] == '\t'))
            indent += 1;
        const auto body = current.substr(indent);
        if (body.empty() || body.front() == '#') {
            if (newline == std::string_view::npos) break;
            continue;
        }
        while (!stack.empty() && indent <= stack.back().indent) {
            if (stack.back().symbol >= 0) symbols[static_cast<size_t>(stack.back().symbol)].endLine = std::max<uint32_t>(lastCode, symbols[static_cast<size_t>(stack.back().symbol)].line);
            stack.pop_back();
        }
        lastCode = line;
        std::string_view rest = body;
        bool async = false;
        if (rest.starts_with("async ")) {
            async = true;
            rest.remove_prefix(6);
        }
        SymbolKind kind;
        size_t skip;
        if (rest.starts_with("def ")) {
            kind = SymbolKind::Function;
            skip = 4;
        } else if (rest.starts_with("class ")) {
            kind = SymbolKind::Class;
            skip = 6;
        } else {
            if (newline == std::string_view::npos) break;
            continue;
        }
        size_t nameStart = skip;
        while (nameStart < rest.size() && rest[nameStart] == ' ') ++nameStart;
        size_t nameEnd = nameStart;
        while (nameEnd < rest.size() && identPart(static_cast<unsigned char>(rest[nameEnd]))) ++nameEnd;
        if (nameEnd == nameStart) {
            if (newline == std::string_view::npos) break;
            continue;
        }
        ScannedSymbol symbol;
        symbol.name = std::string(rest.substr(nameStart, nameEnd - nameStart));
        symbol.kind = kind;
        symbol.line = symbol.endLine = line;
        symbol.column = static_cast<uint32_t>(indent + (async ? 6 : 0) + nameStart + 1);
        symbol.detail = std::string(body.substr(0, std::min(body.size(), maximumDetail)));
        std::string scope;
        int32_t parent = -1;
        for (const auto &open : stack)
            if (open.symbol >= 0) {
                if (!scope.empty()) scope += '.';
                scope += symbols[static_cast<size_t>(open.symbol)].name;
                parent = open.symbol;
            }
        if (kind == SymbolKind::Function && parent >= 0 &&
            symbols[static_cast<size_t>(parent)].kind == SymbolKind::Class)
            symbol.kind = symbol.name == "__init__" ? SymbolKind::Constructor : SymbolKind::Method;
        symbol.scope = std::move(scope);
        symbol.parent = parent;
        symbols.push_back(std::move(symbol));
        if (stack.size() < maximumDepth) stack.push_back({indent, static_cast<int32_t>(symbols.size() - 1)});
        if (newline == std::string_view::npos) break;
    }
    for (const auto &open : stack)
        if (open.symbol >= 0)
            symbols[static_cast<size_t>(open.symbol)].endLine =
                std::max<uint32_t>(lastCode, symbols[static_cast<size_t>(open.symbol)].line);
    return symbols;
}
} // namespace

const char *scannedSymbolKindName(SymbolKind kind) noexcept {
    for (const auto &[value, name] : kindNames)
        if (value == kind) return name;
    return "unknown";
}
std::optional<SymbolKind> symbolKindFromName(std::string_view name) noexcept {
    for (const auto &[value, label] : kindNames)
        if (name == label) return value;
    return std::nullopt;
}
bool symbolKindIsContainer(SymbolKind kind) noexcept {
    switch (kind) {
    case SymbolKind::Namespace: case SymbolKind::Class: case SymbolKind::Struct: case SymbolKind::Union:
    case SymbolKind::Enum: case SymbolKind::Interface: return true;
    default: return false;
    }
}
SourceLanguage sourceLanguageForPath(std::string_view path) noexcept {
    const auto dot = path.find_last_of('.');
    const auto slash = path.find_last_of('/');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return SourceLanguage::None;
    std::string extension(path.substr(dot + 1));
    for (auto &c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::unordered_set<std::string_view> cfamily{
        "c", "cc", "cpp", "cxx", "c++", "h", "hh", "hpp", "hxx", "h++", "inl", "ipp", "tpp", "ixx", "cppm",
        "m", "mm", "cu", "cuh", "hlsl", "hlsli", "fx", "fxh", "usf", "ush", "glsl", "vert", "frag", "comp",
        "ispc", "isph", "metal", "java", "js", "jsx", "ts", "tsx", "go", "rs", "swift", "kt", "kts", "dart"};
    if (extension == "cs") return SourceLanguage::CSharp;
    if (extension == "py" || extension == "pyi") return SourceLanguage::Python;
    return cfamily.contains(extension) ? SourceLanguage::CFamily : SourceLanguage::None;
}
std::vector<ScannedSymbol> scanSymbols(std::string_view text, SourceLanguage language) {
    if (language == SourceLanguage::None) return {};
    if (language == SourceLanguage::Python) return scanPython(text);
    Lexer lexer{text, language};
    lexer.tokens.reserve(text.size() / 5 + 16);
    lexer.run();
    auto symbols = CFamilyParser(text, lexer.tokens, language).run();
    if (!lexer.macros.empty()) {
        symbols.reserve(symbols.size() + lexer.macros.size());
        // Macros are file-level; keep source order by line for outlines.
        std::vector<ScannedSymbol> merged;
        merged.reserve(symbols.size() + lexer.macros.size());
        std::vector<int32_t> remap(symbols.size());
        size_t a = 0, b = 0;
        while (a < symbols.size() || b < lexer.macros.size()) {
            const bool takeMacro = b < lexer.macros.size() &&
                                   (a >= symbols.size() || lexer.macros[b].line < symbols[a].line);
            if (takeMacro) merged.push_back(std::move(lexer.macros[b++]));
            else {
                remap[a] = static_cast<int32_t>(merged.size());
                merged.push_back(std::move(symbols[a++]));
            }
            if (merged.size() >= maximumSymbols) break;
        }
        for (auto &symbol : merged)
            if (symbol.kind != SymbolKind::Macro && symbol.parent >= 0 &&
                static_cast<size_t>(symbol.parent) < remap.size())
                symbol.parent = remap[static_cast<size_t>(symbol.parent)];
        return merged;
    }
    return symbols;
}
std::vector<std::pair<uint32_t, std::string>> scanIncludes(std::string_view text, SourceLanguage language) {
    std::vector<std::pair<uint32_t, std::string>> result;
    uint32_t line = 0;
    for (size_t at = 0; at <= text.size() && result.size() < 4096;) {
        const auto newline = text.find('\n', at);
        const auto end = newline == std::string_view::npos ? text.size() : newline;
        auto current = text.substr(at, end - at);
        ++line;
        at = end + 1;
        size_t i = 0;
        while (i < current.size() && (current[i] == ' ' || current[i] == '\t')) ++i;
        current.remove_prefix(i);
        if (language == SourceLanguage::Python) {
            if (current.starts_with("import ") || current.starts_with("from ")) {
                auto value = std::string(current.substr(0, std::min<size_t>(current.size(), 200)));
                while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) value.pop_back();
                result.emplace_back(line, std::move(value));
            }
        } else if (language == SourceLanguage::CSharp) {
            if (current.starts_with("using ") && current.find('(') == std::string_view::npos) {
                auto value = std::string(current.substr(6, std::min<size_t>(current.size() - 6, 200)));
                while (!value.empty() && (value.back() == '\r' || value.back() == ';' || value.back() == ' ')) value.pop_back();
                result.emplace_back(line, std::move(value));
            }
        } else if (language == SourceLanguage::CFamily && current.starts_with("#")) {
            current.remove_prefix(1);
            while (!current.empty() && (current.front() == ' ' || current.front() == '\t')) current.remove_prefix(1);
            if (current.starts_with("include") || current.starts_with("import")) {
                const auto open = current.find_first_of("<\"");
                if (open != std::string_view::npos) {
                    const char closer = current[open] == '<' ? '>' : '"';
                    const auto close = current.find(closer, open + 1);
                    if (close != std::string_view::npos && close - open < 512)
                        result.emplace_back(line, std::string(current.substr(open, close - open + 1)));
                }
            }
        }
        if (newline == std::string_view::npos) break;
    }
    return result;
}
} // namespace cycode

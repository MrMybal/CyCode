"""Extend a build-local copy of the pinned editor, leaving downloaded sources intact."""
import argparse
from pathlib import Path

def prepare(source, destination):
    if destination.resolve() == source.resolve() or destination.resolve().is_relative_to(source.resolve()):
        raise RuntimeError('Prepare the editor in a separate build directory')
    edits = {
        'TextEditor.h': [
            ('\t\tinline void setBeforeState(const Cursors& cursors) { before = cursors; }', '\t\tbool preserveTabs = false;\n\t\tinline void setBeforeState(const Cursors& cursors) { before = cursors; }'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid ReplaceTextPreservingTabs(const std::string_view& text);\n\tvoid SetLanguage(const Language* language);'),
            ('\t\tcurrentLineHighlightBorder,\n\t\tcount', '\t\tcurrentLineHighlightBorder,\n\t\tfunction,\n\t\tmacro,\n\t\tattribute,\n\t\tinactive,\n\t\tfield,\n\t\tparameter,\n\t\tenumerator,\n\t\tcount'),
            ('\t\tColor color = Color::text;', '\t\tColor color = Color::text;\n\t\tColor semanticColor = Color::count;'),
            ('\t} colorizer;', '\t} colorizer;\n\tsize_t semanticVersion = ~size_t(0);'),
            ('\t\tinline Iterator& operator++() { glyph++; return *this; }', '\t\tinline Iterator& operator++() { glyph++; return *this; }\n\t\tinline Iterator& operator--() { glyph--; return *this; }'),
            ('\t\tstd::function<Iterator(Iterator start, Iterator end, Color& color)> customTokenizer;',
             '\t\tstd::function<Iterator(Iterator start, Iterator end, Color& color)> customTokenizer;\n\t\t// CyCode: bounded context-aware lexical highlighting, after strings/comments.\n\t\tstd::function<Iterator(Iterator lineStart, Iterator start, Iterator end, Color& color)> contextTokenizer;'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\t// CyCode: lexical state stays separate from display-only semantic overlays.\n\tColor GetTokenColor(DocPos position);\n\tColor GetDisplayColor(DocPos position);\n\tstruct SemanticToken { size_t line, start, end; Color color; };\n\tvoid SetSemanticTokens(const std::vector<SemanticToken>& tokens);\n\tvoid ClearSemanticTokens() { semanticVersion = ~size_t(0); }'),
        ],
        'TextEditor.cpp': [
            ('\tconst auto transaction = at(--undoIndex);', '\tconst auto transaction = at(--undoIndex);\n\tauto exact = config; exact.insertSpacesOnTabs = false;'),
            ('\tconst auto transaction = at(undoIndex++);', '\tconst auto transaction = at(undoIndex++);\n\tauto exact = config; exact.insertSpacesOnTabs = false;'),
            ('\t\t} else {\n\t\t\tdocument.insertText(config, action->start, action->text);', '\t\t} else {\n\t\t\tdocument.insertText(transaction->preserveTabs ? exact : config, action->start, action->text);'),
            ('\t\tif (action->type == Action::Type::insertText) {\n\t\t\tdocument.insertText(config, action->start, action->text);', '\t\tif (action->type == Action::Type::insertText) {\n\t\t\tdocument.insertText(transaction->preserveTabs ? exact : config, action->start, action->text);'),
            ('#include <set>', '#include <set>\n#include <stdexcept>'),
            ('void TextEditor::reset() {', 'void TextEditor::reset() {\n\tClearSemanticTokens();'),
            ('void TextEditor::SetLanguage(const Language* language) {', 'void TextEditor::SetLanguage(const Language* language) {\n\tClearSemanticTokens();'),
            ('palette.get(glyph.color), codepoint);', 'palette.get(semanticVersion == transactions.getVersion() && glyph.semanticColor != Color::count ? glyph.semanticColor : glyph.color), codepoint);'),
            ('\t\tcolor == TextEditor::Color::knownIdentifier;',
             '\t\tcolor == TextEditor::Color::knownIdentifier ||\n\t\tcolor == TextEditor::Color::declaration ||\n\t\tcolor == TextEditor::Color::function ||\n\t\tcolor == TextEditor::Color::macro ||\n\t\tcolor == TextEditor::Color::attribute;'),
            ('state.inIdentifier = color == Color::identifier || color == Color::knownIdentifier;',
             'state.inIdentifier = isIdentifier(color);'),
            ('\t\t\t\t// handle custom tokenizer (if we have one)\n\t\t\t\tif (language->customTokenizer',
             '\t\t\t\t// CyCode context tokenizer preserves the built-in comment/string state machine.\n\t\t\t\tif (language->contextTokenizer && (tokenEnd = language->contextTokenizer(Iterator(line.data()), tokenStart, lineEnd, color)) != tokenStart) {\n\t\t\t\t\tconst auto size = tokenEnd - tokenStart;\n\t\t\t\t\tsetColor(glyph, glyph + size, color); glyph += size;\n\t\t\t\t} else if (language->customTokenizer'),
        ],
    }
    destination.mkdir(parents=True, exist_ok=True)
    for name, replacements in edits.items():
        content = (source/name).read_text('utf-8')
        for before, after in replacements:
            if content.count(before) != 1:
                raise RuntimeError(f'Pinned editor patch anchor changed: {name}: {before[:65]!r}')
            content = content.replace(before, after)
        if name == 'TextEditor.cpp':
            content += '''
void TextEditor::ReplaceTextPreservingTabs(const std::string_view& text) {
    if (config.readOnly) return;
    const bool policy=config.insertSpacesOnTabs;
    const auto version=transactions.getVersion();
    config.insertSpacesOnTabs=false;
    try { replaceTextInCurrentCursor(text); }
    catch (...) { config.insertSpacesOnTabs=policy; throw; }
    config.insertSpacesOnTabs=policy;
    if (version!=transactions.getVersion() && !transactions.empty()) transactions.back()->preserveTabs=true;
}
TextEditor::Color TextEditor::GetTokenColor(DocPos position) {
    colorizer.update(config, document);
    if (position.line >= document.size() || position.index >= document[position.line].size()) return Color::text;
    return document[position.line][position.index].color;
}
TextEditor::Color TextEditor::GetDisplayColor(DocPos position) {
    const auto lexical = GetTokenColor(position);
    if (position.line >= document.size() || position.index >= document[position.line].size()) return lexical;
    const auto semantic = document[position.line][position.index].semanticColor;
    return semanticVersion == transactions.getVersion() && semantic != Color::count ? semantic : lexical;
}
void TextEditor::SetSemanticTokens(const std::vector<SemanticToken>& tokens) {
    if (tokens.size() > 50000) throw std::runtime_error("Too many semantic display spans");
    size_t previousLine=0, previousEnd=0; bool first=true;
    for (const auto &token : tokens) {
        if (token.line >= document.size() || token.start >= token.end || token.end > document[token.line].size() ||
            token.color < Color::text || token.color >= Color::count ||
            (!first && (token.line < previousLine || (token.line == previousLine && token.start < previousEnd))))
            throw std::runtime_error("Invalid semantic display span");
        previousLine=token.line; previousEnd=token.end; first=false;
    }
    colorizer.update(config, document);
    for (auto &line : document) for (auto &glyph : line) glyph.semanticColor=Color::count;
    for (const auto &token : tokens) for (size_t i=token.start;i<token.end;++i) {
        auto &glyph=document[token.line][i];
        if (token.color != Color::inactive && (glyph.color == Color::comment || glyph.color == Color::string)) continue;
        if (token.color == Color::declaration && glyph.color == Color::keyword) continue;
        glyph.semanticColor=token.color;
    }
    semanticVersion=transactions.getVersion();
}
'''
        path = destination/name
        data = content.encode('utf-8')
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.destination)

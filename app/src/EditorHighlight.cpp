#include "cycode/EditorLanguage.h"
#include <nlohmann/json.hpp>
namespace cycode {
size_t applySemanticHighlights(TextEditor &editor,const nlohmann::json &tokens,bool cpp) {
    using Color=TextEditor::Color; std::vector<TextEditor::SemanticToken> spans;
    for (const auto &token : tokens) {
        const auto type=token.at("type").get<std::string>(); Color role=Color::count;
        if (type=="type" || type=="class" || type=="struct" || type=="enum" || type=="interface" || type=="typeParameter" || type=="namespace" || type=="concept") role=Color::declaration;
        else if (type=="function" || type=="method") role=Color::function;
        else if (type=="macro") role=Color::macro;
        else if (type=="parameter") role=Color::parameter;
        else if (type=="property" || type=="event") role=Color::field;
        else if (type=="variable") role=Color::identifier;
        else if (type=="enumMember") role=Color::enumerator;
        else if (type=="keyword" || type=="modifier") role=Color::keyword;
        else if (type=="decorator") role=Color::attribute;
        else if (type=="string" || type=="regexp") role=Color::string;
        else if (type=="number") role=Color::number;
        else if (type=="comment") role=cpp ? Color::inactive : Color::comment;
        if (role==Color::count) continue; // Preserve lexical colors for dependent/unknown names.
        for (const auto &modifier : token.at("modifiers")) if (modifier=="constructorOrDestructor") role=Color::declaration;
        spans.push_back({token.at("line"),token.at("glyphStart"),token.at("glyphEnd"),role});
    }
    editor.SetSemanticTokens(spans); return spans.size();
}
}

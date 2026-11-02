#pragma once
#include <TextEditor.h>
#include <string>
#include <nlohmann/json_fwd.hpp>
namespace cycode {
// Fast lexical fallback: no project evaluation or language server is required.
const TextEditor::Language *editorLanguage(const std::string &path, bool unreal = false);
size_t applySemanticHighlights(TextEditor &editor,const nlohmann::json &tokens,bool cpp);
}

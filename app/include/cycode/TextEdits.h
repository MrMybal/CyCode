#pragma once
#include "cycode/LanguageClient.h"
namespace cycode {
size_t utf16Offset(const std::string &text, const Json &position);
struct EditedText { std::string text; Json edits; };
EditedText applyTextEdits(const std::string &text, const Json &edits, const std::string &expectedOldName = {});
bool containsPath(const std::filesystem::path &root, const std::filesystem::path &path);
} // namespace cycode

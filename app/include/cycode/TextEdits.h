#pragma once
#include "cycode/LanguageClient.h"
#include <vector>
namespace cycode {
size_t utf16Offset(const std::string &text, const Json &position);
// Line-start table for converting many LSP positions in one text. Construction is
// one pass; each lookup costs only the length of the addressed line prefix, with
// the same validation and errors as utf16Offset. The text must outlive the index.
class TextLineIndex {
  public:
    explicit TextLineIndex(const std::string &text);
    size_t offset(const Json &position) const;
  private:
    const std::string &text_;
    std::vector<size_t> starts_;
};
struct EditedText { std::string text; Json edits; };
EditedText applyTextEdits(const std::string &text, const Json &edits, const std::string &expectedOldName = {});
bool containsPath(const std::filesystem::path &root, const std::filesystem::path &path);
} // namespace cycode

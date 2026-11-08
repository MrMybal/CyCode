#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace cycode {
inline constexpr size_t maxTextFileBytes = 4 * 1024 * 1024;
std::string readTextFileBytes(const std::filesystem::path &path);
void validateEditorText(const std::string &text);
std::string normalizeEditorText(const std::string &text);
std::string encodeEditorText(const std::string &text, bool bom, bool crlf, bool crOnly);
// A missing baseline means create exclusively. An existing baseline must still
// match disk immediately before committing the sibling temporary file.
void writeTextFileAtomic(const std::filesystem::path &path, const std::string &bytes,
                         const std::optional<std::string> &baseline);
} // namespace cycode

#include "cycode/TextFile.h"
#include "cycode/AtomicFile.h"
#include "cycode/Error.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace cycode {
std::string readTextFileBytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read file");
    in.seekg(0, std::ios::end);
    const auto length = in.tellg();
    if (length < 0 || length > static_cast<std::streamoff>(maxTextFileBytes))
        throw std::runtime_error("Editor limit: 4 MiB per file");
    in.seekg(0);
    std::string text(static_cast<size_t>(length), '\0');
    in.read(text.data(), length);
    if ((!in && length) || in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("File changed during read");
    return text;
}
void validateEditorText(const std::string &text) {
    if (text.find('\0') != std::string::npos)
        throw std::runtime_error("Binary files cannot be edited as text");
    if (text.size() > maxTextFileBytes)
        throw std::runtime_error("Editor limit: 4 MiB per file");
    (void)nlohmann::json(text).dump();
}
std::string normalizeEditorText(const std::string &text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            result += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        } else
            result += text[i];
    }
    return result;
}
std::string encodeEditorText(const std::string &text, bool bom, bool crlf, bool crOnly) {
    validateEditorText(text);
    std::string result = bom ? "\xef\xbb\xbf" : "";
    result.reserve(text.size() + result.size());
    for (char c : text) {
        if (c == '\n' && crlf)
            result += '\r';
        result += (c == '\n' && crOnly) ? '\r' : c;
    }
    if (result.size() > maxTextFileBytes)
        throw std::runtime_error("Editor limit: 4 MiB per file");
    return result;
}
void writeTextFileAtomic(const std::filesystem::path &path, const std::string &bytes,
                         const std::optional<std::string> &baseline) {
    if (bytes.size() > maxTextFileBytes)
        throw std::runtime_error("Editor limit: 4 MiB per file");
    AtomicFileOptions options;
    options.publication =
        baseline ? AtomicPublication::ReplacePreservingMetadata : AtomicPublication::CreateExclusive;
    writeFileAtomic(path, bytes, options, [&] {
        if (baseline) {
            if (readTextFileBytes(path) != *baseline)
                throw NativeError(ErrorCode::DiskConflict, "DiskConflict: file changed outside the editor");
            using P = std::filesystem::perms;
            const auto permissions = std::filesystem::status(path).permissions();
            if ((permissions & (P::owner_write | P::group_write | P::others_write)) == P::none)
                throw NativeError(ErrorCode::ReadOnly, "File is read-only");
        } else if (std::filesystem::exists(path)) {
            throw std::runtime_error("File already exists");
        }
    });
}
} // namespace cycode

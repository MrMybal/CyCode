#pragma once
#include "cycode/TextFile.h"
#include <filesystem>
#include <string>
namespace cycode::text {
class Document {
  public:
    static Document open(const std::filesystem::path &path,const std::string &encoding="auto");
    void replace(const std::string &text);
    void save();
    void saveAs(const std::filesystem::path &path, const std::optional<std::string> &baseline);
    bool changedOnDisk() const;
    bool dirty() const { return text_!=saved_ || encoding_!=savedEncoding_ || lineEnding()!=savedLineEnding_; }
    bool untitled() const { return path_.empty(); }
    const std::filesystem::path &path() const { return path_; }
    const std::string &text() const { return text_; }
    const std::string &savedText() const { return saved_; }
    const std::string &encodingId() const { return encoding_; }
    const char *lineEnding() const { return crlf_ ? "CRLF" : crOnly_ ? "CR" : "LF"; }
    const char *encoding() const;
    void format(const std::string &encoding,const std::string &lineEnding);
  private:
    std::filesystem::path path_;
    std::string text_, saved_, disk_;
    std::string encoding_="utf-8",savedEncoding_="utf-8",savedLineEnding_="LF";
    bool crlf_=false, crOnly_=false;
};
bool sameFile(const std::filesystem::path &a, const std::filesystem::path &b);
} // namespace cycode::text

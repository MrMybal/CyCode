#pragma once
#include "cycode/TextFile.h"
#include "cycode/text/DocumentState.h"
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
    bool dirty() const { return bufferHasChanges(state_); }
    bool untitled() const { return path_.empty(); }
    const std::filesystem::path &path() const { return path_; }
    const std::string &text() const { return state_.text; }
    const std::string &savedText() const { return state_.saved; }
    const std::string &encodingId() const { return state_.encoding; }
    const char *lineEnding() const { return bufferLineEnding(state_); }
    const char *encoding() const;
    void format(const std::string &encoding,const std::string &lineEnding);
  private:
    std::filesystem::path path_;
    BufferState state_;
};
bool sameFile(const std::filesystem::path &a, const std::filesystem::path &b);
} // namespace cycode::text

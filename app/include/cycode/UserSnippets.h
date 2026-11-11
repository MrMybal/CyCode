#pragma once
#include "cycode/SnippetCatalogue.h"
#include <atomic>
#include <mutex>
#include <optional>
namespace cycode {
constexpr bool userSnippetRevision(std::string_view value) {
    if(value=="absent")return true;
    if(value.size()!=64)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
constexpr bool userSnippetBytes(size_t bytes) {return bytes<=SnippetCatalogueLimits::bytes;}
// Fixed installation-owned preferences. Empty directory disables all file access.
class UserSnippets {
  public:
    explicit UserSnippets(std::filesystem::path directory = {});
    nlohmann::json snapshot();
    nlohmann::json draft(std::string_view literalSource,const std::string &language);
    nlohmann::json save(const std::string &text,const std::string &expectedRevision);
    uint64_t generation() const {return generation_.load();}
  private:
    void checkPath() const;
    nlohmann::json read();
    std::filesystem::path directory_,file_;
    std::optional<std::string> baseline_;
    std::string lastRevision_;
    std::atomic<uint64_t> generation_=0;
    std::mutex mutex_;
};
} // namespace cycode

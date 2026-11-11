#include "cycode/UserSnippets.h"
#include "cycode/NativeFileLease.h"
#include "cycode/TextFile.h"
#include <monocypher.h>
#include <array>
#include <cwchar>
#include <stdexcept>
namespace cycode {
using Json=nlohmann::json;
namespace {
std::string revision(const std::optional<std::string> &bytes) {
    if(!bytes)return "absent";
    std::array<uint8_t,32> hash{};
    crypto_blake2b(hash.data(),hash.size(),reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size());
    std::string value;value.reserve(64);
    for(auto c:hash){value+="0123456789abcdef"[c>>4];value+="0123456789abcdef"[c&15];}
    return value;
}
bool samePath(const std::filesystem::path &a,const std::filesystem::path &b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(),b.c_str())==0;
#else
    return a==b;
#endif
}
}
UserSnippets::UserSnippets(std::filesystem::path directory) : directory_(std::move(directory)) {
    if(directory_.empty())return;
    if(!directory_.is_absolute())throw std::runtime_error("User snippets need an application-owned absolute directory");
    directory_=directory_.lexically_normal();file_=directory_/"user.code-snippets";
}
void UserSnippets::checkPath() const {
    std::error_code error;const auto status=std::filesystem::symlink_status(file_,error);
    if(!samePath(std::filesystem::weakly_canonical(directory_),directory_) ||
        (error && error!=std::errc::no_such_file_or_directory) || std::filesystem::is_symlink(status))
        throw std::runtime_error("User snippet storage cannot traverse redirected paths");
}
Json UserSnippets::read() {
    Json state={{"available",!file_.empty()},{"editable",false},{"valid",false},
        {"revision",file_.empty()?"disabled":""},{"generation",generation_.load()},
        {"text",""},{"error",""}};
    baseline_.reset();
    if(file_.empty())return state;
    try {
        checkPath();
        std::error_code error;const auto status=std::filesystem::symlink_status(file_,error);
        if(error && error!=std::errc::no_such_file_or_directory)throw std::runtime_error("Cannot inspect user snippet storage");
        if(!error && std::filesystem::exists(status)) {
            if(!std::filesystem::is_regular_file(status))throw std::runtime_error("User snippet storage must be a regular file");
            baseline_=readSnippetCatalogue(file_);validateEditorText(*baseline_);
        }
        state["text"]=baseline_.value_or("{\n}\n");state["editable"]=true;
        state["revision"]=revision(baseline_);
        try {(void)snippetCatalogueItems(state.at("text").get_ref<const std::string&>(),"");state["valid"]=true;}
        catch(const std::exception &e){state["error"]=e.what();}
    } catch(const std::exception &e) {state["error"]=e.what();baseline_.reset();}
    const auto observed=state.at("revision").get<std::string>();
    if(observed!=lastRevision_){lastRevision_=observed;++generation_;}
    state["generation"]=generation_.load();return state;
}
Json UserSnippets::snapshot() {std::lock_guard lock(mutex_);return read();}
Json UserSnippets::draft(std::string_view source,const std::string &language) {
    const auto body=captureSnippetBody(source);
    if(!body)throw std::runtime_error("Selected code is empty or its escaped snippet body exceeds 8 KiB");
    std::lock_guard lock(mutex_);const auto state=read();
    if(state.at("valid")!=true || state.at("editable")!=true)
        throw std::runtime_error("Open Edit user snippets and repair the personal catalogue before capturing code");
    auto definitions=Json::parse(state.at("text").get_ref<const std::string&>());
    std::string name="Selection snippet";
    for(size_t suffix=2;definitions.contains(name) && suffix<=SnippetCatalogueLimits::definitions+1;++suffix)
        name="Selection snippet "+std::to_string(suffix);
    if(definitions.contains(name) || definitions.size()>=SnippetCatalogueLimits::definitions)
        throw std::runtime_error("Personal snippet catalogue already contains 256 definitions");
    definitions[name]={{"prefix","snippet"},{"body",body->text()},{"scope",language}};
    auto text=definitions.dump(2)+"\n";
    validateEditorText(text);(void)snippetCatalogueItems(text,"");
    return {{"catalogue",state},{"draft",std::move(text)},{"name",std::move(name)}};
}
Json UserSnippets::save(const std::string &text,const std::string &expectedRevision) {
    if(!userSnippetBytes(text.size()) || !userSnippetRevision(expectedRevision))
        throw std::runtime_error("Invalid user snippet size or revision");
    validateEditorText(text);(void)snippetCatalogueItems(text,"");
    std::lock_guard lock(mutex_);
    if(file_.empty())throw std::runtime_error("User snippet storage is not configured in this host");
    checkPath();std::filesystem::create_directories(directory_);checkPath();
    NativeFileLease lease(directory_/"user.code-snippets.lock");
    if(!lease.acquired())throw std::runtime_error("User snippets are being saved by another instance; retry");
    checkPath();const auto current=read();
    if(!current.at("editable").get<bool>() || current.at("revision")!=expectedRevision)
        throw std::runtime_error("User snippets changed or are unreadable; reload before saving");
    writeTextFileAtomic(file_,text,baseline_); // Valid draft can repair readable malformed JSON.
    const auto saved=read();
    if(saved.at("valid")!=true || saved.at("revision")!=revision(std::optional<std::string>(text)))
        throw std::runtime_error("User snippets were written but could not be confirmed; reload before retrying");
    return saved;
}
} // namespace cycode

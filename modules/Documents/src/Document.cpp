#include "cycode/Error.h"
#include "cycode/text/Document.h"
#include "cycode/text/Encoding.h"
#include <stdexcept>
namespace cycode::text {
Document Document::open(const std::filesystem::path &path,const std::string &encoding) {
    Document doc;doc.path_=std::filesystem::canonical(path);
    if(!std::filesystem::is_regular_file(doc.path_))throw std::runtime_error("Choose a text file");
    doc.state_=decodeBuffer(readTextFileBytes(doc.path_),encoding);return doc;
}
const char *Document::encoding() const {return encodingLabel(state_.encoding);}
void Document::format(const std::string &encoding,const std::string &lineEnding){formatBuffer(state_,encoding,lineEnding);}
void Document::replace(const std::string &text){replaceBuffer(state_,text);}
bool Document::changedOnDisk() const {
    if(untitled())return false;
    try{return readTextFileBytes(path_)!=state_.disk;}catch(...){return true;}
}
void Document::save() {
    if(untitled())throw std::runtime_error("Choose a file name first");
    if(changedOnDisk())throw NativeError(ErrorCode::DiskConflict,"DiskConflict: file changed outside the editor");
    if(!dirty())return;
    const auto bytes=encodeBuffer(state_);writeTextFileAtomic(path_,bytes,state_.disk);markBufferSaved(state_,bytes);
}
void Document::saveAs(const std::filesystem::path &path,const std::optional<std::string> &baseline) {
    const auto full=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    if(!untitled() && sameFile(full,path_)){save();return;}
    // Preserve original mixed newlines byte-for-byte on a no-op Save As.
    const auto bytes=!dirty() && !untitled()?state_.disk:encodeBuffer(state_);
    writeTextFileAtomic(full,bytes,baseline);path_=full;markBufferSaved(state_,bytes);
}
bool sameFile(const std::filesystem::path &a, const std::filesystem::path &b) {
    if (a.empty() || b.empty()) return false;
    std::error_code ec;
    if (std::filesystem::equivalent(a,b,ec) && !ec) return true;
#ifdef _WIN32
    return _wcsicmp(a.lexically_normal().c_str(),b.lexically_normal().c_str())==0;
#else
    return a.lexically_normal()==b.lexically_normal();
#endif
}
} // namespace cycode::text

#include "cycode/text/Document.h"
#include "cycode/text/Encoding.h"
#include <stdexcept>
namespace cycode::text {
Document Document::open(const std::filesystem::path &path,const std::string &encoding) {
    Document doc;
    doc.path_=std::filesystem::canonical(path);
    if (!std::filesystem::is_regular_file(doc.path_)) throw std::runtime_error("Choose a text file");
    doc.disk_=readTextFileBytes(doc.path_);
    const auto decoded=decodeText(doc.disk_,encoding);auto text=decoded.text;
    doc.encoding_=doc.savedEncoding_=decoded.encoding;
    doc.crlf_=text.find("\r\n")!=std::string::npos;
    doc.crOnly_=text.find('\r')!=std::string::npos && text.find('\n')==std::string::npos;
    doc.text_=normalizeEditorText(text); doc.saved_=doc.text_;
    doc.savedLineEnding_=doc.lineEnding();
    return doc;
}
const char *Document::encoding() const { return encodingLabel(encoding_); }
void Document::format(const std::string &encoding,const std::string &lineEnding) {
    if (!lineEnding.empty() && lineEnding!="LF" && lineEnding!="CRLF" && lineEnding!="CR") throw std::runtime_error("Unsupported line ending");
    if (!encoding.empty()) { encodeText(text_,encoding);encoding_=encoding; }
    if (!lineEnding.empty()) { crlf_=lineEnding=="CRLF";crOnly_=lineEnding=="CR"; }
}
void Document::replace(const std::string &text) { validateEditorText(text); text_=normalizeEditorText(text); }
bool Document::changedOnDisk() const {
    if (untitled()) return false;
    try { return readTextFileBytes(path_)!=disk_; } catch (...) { return true; }
}
void Document::save() {
    if (untitled()) throw std::runtime_error("Choose a file name first");
    if (changedOnDisk()) throw std::runtime_error("DiskConflict: file changed outside the editor");
    if (!dirty()) return;
    const auto bytes=encodeText(encodeEditorText(text_,false,crlf_,crOnly_),encoding_);
    writeTextFileAtomic(path_,bytes,disk_);
    disk_=bytes; saved_=text_;savedEncoding_=encoding_;savedLineEnding_=lineEnding();
}
void Document::saveAs(const std::filesystem::path &path, const std::optional<std::string> &baseline) {
    const auto full=std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    if (!untitled() && sameFile(full,path_)) { save(); return; }
    // A no-op Save As retains mixed line endings byte for byte.
    const auto bytes=!dirty() && !untitled() ? disk_ : encodeText(encodeEditorText(text_,false,crlf_,crOnly_),encoding_);
    writeTextFileAtomic(full,bytes,baseline);
    path_=full; disk_=bytes; saved_=text_;savedEncoding_=encoding_;savedLineEnding_=lineEnding();
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

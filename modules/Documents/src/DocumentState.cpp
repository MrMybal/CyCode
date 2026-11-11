#include "cycode/text/DocumentState.h"
#include "cycode/text/Encoding.h"
#include "cycode/TextFile.h"
#include <stdexcept>
namespace cycode::text {
const char *bufferLineEnding(const BufferState &s) noexcept {return s.crlf?"CRLF":s.crOnly?"CR":"LF";}
std::string bufferEncoding(const BufferState &s) {
    return s.encoding=="utf-8" || s.encoding=="utf-8-bom" ? (s.bom?"utf-8-bom":"utf-8") : s.encoding;
}
bool bufferHasChanges(const BufferState &s) {
    return s.text!=s.saved || bufferEncoding(s)!=s.savedEncoding || bufferLineEnding(s)!=s.savedLineEnding;
}
BufferState decodeBuffer(const std::string &bytes,const std::string &encoding) {
    const auto decoded=decodeText(bytes,encoding);
    BufferState s;s.disk=bytes;s.encoding=s.savedEncoding=decoded.encoding;s.bom=decoded.encoding=="utf-8-bom";
    s.crlf=decoded.text.find("\r\n")!=std::string::npos;
    s.crOnly=decoded.text.find('\r')!=std::string::npos && decoded.text.find('\n')==std::string::npos;
    s.text=normalizeEditorText(decoded.text);s.saved=s.text;s.savedLineEnding=bufferLineEnding(s);return s;
}
std::string encodeBuffer(const BufferState &s) {
    return encodeText(encodeEditorText(s.text,false,s.crlf,s.crOnly),bufferEncoding(s));
}
void replaceBuffer(BufferState &s,const std::string &text) {
    validateEditorText(text);s.text=normalizeEditorText(text);s.dirty=bufferHasChanges(s);
}
void formatBuffer(BufferState &s,const std::string &encoding,const std::string &lineEnding) {
    if(!lineEnding.empty() && lineEnding!="LF" && lineEnding!="CRLF" && lineEnding!="CR")throw std::runtime_error("Unsupported line ending");
    if(!encoding.empty()) {(void)encodeText(s.text,encoding);s.encoding=encoding;s.bom=encoding=="utf-8-bom";}
    if(!lineEnding.empty()){s.crlf=lineEnding=="CRLF";s.crOnly=lineEnding=="CR";}
    s.dirty=bufferHasChanges(s);
}
void markBufferSaved(BufferState &s,const std::string &bytes) {
    s.encoding=bufferEncoding(s);s.savedEncoding=s.encoding;s.savedLineEnding=bufferLineEnding(s);
    s.disk=bytes;s.saved=s.text;s.dirty=false;s.diskExisted=true;s.diskConflict.clear();
}
} // namespace cycode::text

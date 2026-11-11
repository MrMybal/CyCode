#pragma once
#include <cstdint>
#include <string>
namespace cycode::text {
// Canonical native buffer/baseline record. Project and text-session owners retain
// their path policy, identity/revision issuance and transaction boundaries.
struct BufferState {
    uint64_t identity=0, revision=1;
    std::string text,saved,disk,diskConflict;
    bool bom=false,crlf=false,crOnly=false,dirty=false,diskExisted=true;
    std::string encoding="utf-8",savedEncoding="utf-8",savedLineEnding="LF";
};
const char *bufferLineEnding(const BufferState &) noexcept;
std::string bufferEncoding(const BufferState &);
bool bufferHasChanges(const BufferState &);
BufferState decodeBuffer(const std::string &bytes,const std::string &encoding="auto");
std::string encodeBuffer(const BufferState &);
void replaceBuffer(BufferState &,const std::string &text);
void formatBuffer(BufferState &,const std::string &encoding,const std::string &lineEnding);
void markBufferSaved(BufferState &,const std::string &bytes);
} // namespace cycode::text

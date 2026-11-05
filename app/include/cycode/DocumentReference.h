#pragma once
#include <string>
namespace cycode {
// An issued in-memory document reference is never a filesystem path.
bool isUntitledDocument(const std::string &);
void validateUntitledName(const std::string &);
std::string untitledDocumentName(const std::string &);
std::string newUntitledReference(const std::string &);
} // namespace cycode

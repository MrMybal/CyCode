#pragma once
#include <string>
namespace cycode::text {
struct DecodedText { std::string text,encoding; };
DecodedText decodeText(const std::string &bytes,const std::string &encoding="auto");
std::string encodeText(const std::string &text,const std::string &encoding);
const char *encodingLabel(const std::string &encoding);
}

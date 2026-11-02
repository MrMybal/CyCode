#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// Public columns are UTF-16; glyph offsets are Unicode-codepoint indexes.
nlohmann::json decodeHighlights(const std::string &text,const nlohmann::json &reply);
}

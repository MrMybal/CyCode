#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace cycode {
struct SnippetRange { size_t start,end;std::vector<uint32_t> parents; };
struct SnippetField { uint32_t index;std::vector<SnippetRange> ranges;std::vector<std::string> choices; };
struct Snippet { std::string text;std::vector<SnippetField> fields;size_t finalByte=0; };
// Bounded LSP placeholders, mirrors, nested defaults and choices. No variables,
// transforms or executable expressions are evaluated.
std::optional<Snippet> expandSnippet(const std::string &source);
void indentSnippet(Snippet &,const std::string &indent);
nlohmann::json snippetMetadata(const Snippet &,size_t documentByteOffset);
}

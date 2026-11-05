#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace cycode {
nlohmann::json collectOccurrences(const nlohmann::json &response, size_t limit,
                                  const std::string &sourceText);
// Rows are sorted native results. Kind mask bits: Text=1, Read=2, Write=4.
int adjacentOccurrence(const nlohmann::json &rows, const nlohmann::json &position, bool previous,
                       unsigned kinds = 7);
// Preflight the complete reported selection, without changing editor state.
nlohmann::json occurrenceSelections(const nlohmann::json &rows, unsigned kinds = 7);
} // namespace cycode

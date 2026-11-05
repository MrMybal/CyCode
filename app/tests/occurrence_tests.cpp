#include "cycode/OccurrenceCatalogue.h"
#include <stdexcept>
using namespace cycode;
namespace {
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Json point(int column) {
    return {{"line", 0}, {"character", column}};
}
Json span(int from, int to) {
    return {{"start", point(from)}, {"end", point(to)}};
}
Json row(int from, int to, int kind = 1) {
    return {{"range", span(from, to)}, {"kind", kind}};
}
template <class F> void rejects(F fn) {
    bool bad = false;
    try {
        fn();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, "Invalid occurrence selection accepted");
}
} // namespace
void occurrenceContracts() {
    check(collectOccurrences(nullptr, 4096, "").at("occurrences").empty(),
          "Null highlight response was lost");
    const std::string text = "a 🛠 value value\n";
    auto report = collectOccurrences(
        Json::array(
            {row(11, 16, 2), row(5, 10), row(5, 10, 3), row(3, 4, 2), row(0, 1, 9), {{"range", span(0, 1)}}}),
        4096, text);
    const auto &rows = report.at("occurrences");
    check(rows.size() == 3 && rows.at(0).at("kind") == 1 && rows.at(1).at("kind") == 3 &&
              report.at("invalid") == 2,
          "Classification, sorting, deduplication or UTF-16 validation failed");
    check(adjacentOccurrence(rows, point(5), false) == 2 && adjacentOccurrence(rows, point(5), true) == 0,
          "Adjacent occurrence did not use cursor position");
    check(adjacentOccurrence(rows, point(16), false) == 0 && adjacentOccurrence(rows, point(0), true) == 2,
          "Cyclic occurrence navigation failed");
    check(adjacentOccurrence(rows, point(0), false, 4) == 1 &&
              adjacentOccurrence(rows, point(0), false, 0) == -1,
          "Kind mask ignored");
    check(occurrenceSelections(rows, 4) == Json::array({span(5, 10)}), "Write selection was not exact");
    check(collectOccurrences(Json::array({row(0, 1), row(5, 10)}), 1, text).at("truncated") == true,
          "Row budget not reported");
    rejects([&] { collectOccurrences(Json::object(), 10, text); });
    rejects([&] { occurrenceSelections(Json::array({row(0, 2), row(1, 3)})); });
    rejects([&] { occurrenceSelections(Json::array({row(0, 0)})); });
    rejects([&] { occurrenceSelections(rows, 0); });
    Json many = Json::array();
    for (int i = 0; i < 129; ++i)
        many.push_back(row(i * 2, i * 2 + 1));
    rejects([&] { occurrenceSelections(many); });
    many.erase(many.end() - 1);
    check(occurrenceSelections(many).size() == 128, "Exact editor limit rejected");
    Json invalid = Json::array();
    for (int i = 0; i < 8193; ++i)
        invalid.push_back(row(0, 1, 9));
    const auto bounded = collectOccurrences(invalid, 4096, text);
    check(bounded.at("invalid") == 8192 && bounded.at("truncated") == true,
          "Malformed provider work was unbounded");
}

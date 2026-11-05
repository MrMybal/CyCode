#include "cycode/Highlighting.h"
#include "cycode/SemanticTokens.h"
#include <stdexcept>

namespace {
using Json = nlohmann::json;
using Tokens = cycode::SemanticTokens;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F operation, const char *message) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
} // namespace
void semanticTokenContracts() {
    const Json full = {{"resultId", "opaque/base"}, {"data", {2, 5, 3, 0, 3, 0, 5, 4, 1, 0, 3, 2, 7, 2, 0}}};
    const auto base = Tokens::decode(full);
    const auto updated = Tokens::decode({{"resultId", "opaque/next"},
                                         {"edits",
                                          {{{"start", 8}, {"deleteCount", 1}, {"data", {2}}},
                                           {{"start", 0}, {"deleteCount", 1}, {"data", {3}}}}}},
                                        base);
    auto expected = full.at("data");
    expected[0] = 3;
    expected[8] = 2;
    check(updated->result().at("data") == expected, "Unsorted integer-offset delta reconstruction");
    check(base->result().at("data") == full.at("data"), "Delta changed its immutable base");
    check(updated->resultId == "opaque/next", "Delta result ID was not retained");
    check(!updated->result().contains("resultId"), "Opaque result ID leaked into normalized data");
    const auto unchanged = Tokens::decode({{"resultId", "again"}, {"edits", Json::array()}}, updated);
    check(unchanged->words == updated->words, "Empty delta cleared tokens");
    const auto removed = Tokens::decode({{"edits", {{{"start", 5}, {"deleteCount", 5}}}}}, base);
    check(removed->words.size() == 10 && removed->words[5] == 3, "Deletion-only delta");
    const auto replaced = Tokens::decode({{"data", {0, 0, 3, 0, 0}}}, updated);
    check(replaced->words.size() == 5 && !replaced->resultId, "Full response to a delta request");
    check(Tokens::decode(nullptr, base)->words.empty(), "Null response did not clear tokens");
    const auto empty = Tokens::decode({{"resultId", ""}, {"data", Json::array()}});
    check(empty->resultId.has_value(), "An empty opaque result ID is valid");
    const auto inserted = Tokens::decode({{"edits",
                                           {{{"start", 0}, {"deleteCount", 0}, {"data", {0, 0, 3}}},
                                            {{"start", 0}, {"deleteCount", 0}, {"data", {0, 0}}}}}},
                                         empty);
    check(inserted->words == std::vector<uint32_t>({0, 0, 3, 0, 0}),
          "Same-position insertions lost their server ordering");
    const auto one = Tokens::decode({{"resultId", "utf16"}, {"data", {0, 3, 1, 0, 0}}});
    const auto moved = Tokens::decode({{"edits", {{{"start", 1}, {"deleteCount", 1}, {"data", {4}}}}}}, one);
    const auto colors = cycode::decodeHighlights(
        "a\xF0\x9F\x98\x80 b", {{"supported", true},
                                {"generation", 9},
                                {"legend", {{"tokenTypes", {"variable"}}, {"tokenModifiers", Json::array()}}},
                                {"result", moved->result()}});
    check(colors.at("tokens").at(0).at("glyphStart") == 3 && colors.at("tokens").at(0).at("character") == 4,
          "Reconstructed delta bypassed Unicode-aware highlighting");
    rejects([&] { Tokens::decode({{"edits", Json::array()}}); }, "Delta accepted without a base");
    rejects([&] { Tokens::decode({{"data", {0, 0, 3, 0}}}); }, "Incomplete full token accepted");
    rejects([&] { Tokens::decode({{"data", {0, -1, 3, 0, 0}}}); }, "Negative integer accepted");
    rejects([&] { Tokens::decode({{"data", {0, 0.5, 3, 0, 0}}}); }, "Fraction accepted");
    rejects([&] { Tokens::decode({{"data", {0, true, 3, 0, 0}}}); }, "Boolean accepted");
    rejects([&] { Tokens::decode({{"data", {0, UINT64_MAX, 3, 0, 0}}}); }, "Integer overflow accepted");
    rejects([&] { Tokens::decode({{"data", Json::array()}, {"edits", Json::array()}}); },
            "Ambiguous full/delta response accepted");
    rejects([&] { Tokens::decode({{"resultId", std::string(4097, 'x')}, {"data", Json::array()}}); },
            "Unbounded opaque result ID accepted");
    rejects([&] { Tokens::decode({{"edits", {{{"start", 16}, {"deleteCount", 0}}}}}, base); },
            "Delta beyond base accepted");
    rejects([&] { Tokens::decode({{"edits", {{{"start", 14}, {"deleteCount", 2}}}}}, base); },
            "Deletion beyond base accepted");
    const Json overlap = {{"edits", Json::array({Json{{"start", 4}, {"deleteCount", 3}},
                                                 Json{{"start", 5}, {"deleteCount", 2}}})}};
    rejects([&] { Tokens::decode(overlap, base); }, "Overlapping delta accepted");
    rejects([&] { Tokens::decode({{"edits", {{{"start", 0}, {"deleteCount", 1}}}}}, base); },
            "Incomplete reconstructed token accepted");
    Json many = Json::array();
    for (size_t i = 0; i <= Tokens::maxEdits; ++i)
        many.push_back({{"start", 0}, {"deleteCount", 0}});
    rejects([&] { Tokens::decode({{"edits", many}}, base); }, "Unbounded edit count accepted");
    Json large = std::vector<uint32_t>(Tokens::maxWords, 0);
    const auto maximum = Tokens::decode({{"resultId", "large"}, {"data", large}});
    rejects(
        [&] {
            Tokens::decode({{"edits", {{{"start", 0}, {"deleteCount", 0}, {"data", {0, 0, 1, 0, 0}}}}}},
                           maximum);
        },
        "Unbounded reconstructed array accepted");
    large.push_back(0);
    rejects([&] { Tokens::decode({{"data", large}}); }, "Unbounded full array accepted");
    Tokens cache;
    for (size_t i = 0; i < Tokens::maxEntries; ++i)
        cache.remember(std::to_string(i), base);
    check(cache.find("0") == base, "Cache lookup changed its snapshot");
    cache.remember("next", updated);
    check(cache.size() == Tokens::maxEntries && !cache.find("1") && cache.find("0"),
          "Entry budget did not evict the least recently used document");
    cache.remember("0", replaced);
    check(!cache.find("0"), "Response without result ID kept a reusable base");
    cache.clear();
    for (int i = 0; i < 5; ++i)
        cache.remember(std::to_string(i), maximum);
    check(cache.words() == Tokens::maxCachedWords && cache.size() == 4 && !cache.find("0"),
          "Global integer budget was not enforced");
    const auto held = cache.find("1");
    cache.erase("1");
    check(held == maximum && !cache.find("1"), "In-flight base was invalidated by eviction");
    cache.clear();
    check(cache.size() == 0 && cache.words() == 0 && !cache.find("2"),
          "Close/restart/refresh cache reset retained a base");
}

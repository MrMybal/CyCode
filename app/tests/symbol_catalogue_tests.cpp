#include "cycode/SymbolCatalogue.h"
#include <stdexcept>
using namespace cycode;
namespace {
using Json = nlohmann::json;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
Json range(int a, int b, int x = 0, int y = 0) {
    return {{"start", {{"line", a}, {"character", x}}}, {"end", {{"line", b}, {"character", y}}}};
}
Json symbol(const std::string &name, int kind, int a, int b) {
    return {{"name", name}, {"kind", kind}, {"range", range(a, b)}, {"selectionRange", range(a, a)}};
}
} // namespace
void symbolCatalogueContracts() {
    const auto resolver = [](const std::string &, const Json &selection, const Json &) {
        return Json{{"path", "main.cpp"}, {"range", selection}};
    };
    auto child = symbol("Run", 6, 2, 4);
    child["tags"] = Json::array({1});
    auto parent = symbol("Widget", 5, 0, 8);
    parent["children"] = Json::array({child});
    const auto report = collectSymbols(Json::array({parent}), "file:///main.cpp", 100, false, resolver);
    const auto &rows = report.at("symbols");
    check(rows.size() == 2 && rows.at(0).at("node") == 1 && rows.at(1).at("parent") == 1 &&
              rows.at(1).at("depth") == 1 && rows.at(1).at("container") == "Widget" &&
              rows.at(1).at("deprecated"),
          "Server hierarchy/deprecation was flattened or lost");
    check(enclosingSymbol(rows, "main.cpp", {{"line", 3}, {"character", 0}}) == 2,
          "Caret scope did not select the innermost symbol");
    check(enclosingSymbol(rows, "main.cpp", {{"line", 4}, {"character", 0}}) == 1 &&
              enclosingSymbol(rows, "main.cpp", {{"line", 8}, {"character", 0}}) == 0 &&
              enclosingSymbol(rows, "other.cpp", {{"line", 3}, {"character", 0}}) == 0,
          "Scope containment ignored half-open ranges or source identity");
    check(collectSymbols(Json::array({parent, parent}), "file:///main.cpp", 100, false, resolver)
                  .at("symbols")
                  .size() == 2,
          "Duplicate symbol trees were emitted twice");
    auto types = collectSymbols(Json::array({parent}), "file:///main.cpp", 100, true, resolver);
    check(types.at("symbols").size() == 1, "Types-only search retained methods");
    auto wrapper = symbol("Scope", 3, 0, 10);
    wrapper["children"] = Json::array({parent});
    types = collectSymbols(Json::array({wrapper}), "file:///main.cpp", 100, true, resolver);
    check(types.at("symbols").at(0).at("parent") == 0 && types.at("symbols").at(0).at("depth") == 0 &&
              types.at("symbols").at(0).at("container") == "Scope",
          "Filtered parent produced dangling hierarchy IDs");
    Json flat = {
        {"name", "field"}, {"kind", 8}, {"location", {{"uri", "file:///main.cpp"}, {"range", range(5, 5)}}}};
    check(collectSymbols(Json::array({flat}), "", 100, false, resolver).at("symbols").at(0).at("fullRange") ==
              range(5, 5),
          "Flat SymbolInformation response stopped working");
    Json pending = {{"name", "Lazy"},
                    {"kind", 5},
                    {"containerName", "Scope"},
                    {"location", {{"uri", "file:///main.cpp"}}},
                    {"data", {{"opaque", 17}}}};
    int called = 0;
    const auto deferred = [&](const std::string &uri, const Json &raw) {
        ++called;
        check(uri == "file:///main.cpp" && raw == pending, "Lazy symbol server payload was changed");
        return Json{{"path", "main.cpp"}, {"unresolved", true}, {"resolveIndex", 0}};
    };
    const auto lazy = collectSymbols(Json::array({pending, pending}), "", 100, false, resolver, deferred);
    check(lazy.at("symbols").size() == 1 && lazy.at("invalid") == 0 && called == 1 &&
              !lazy.at("symbols").at(0).contains("range") &&
              !lazy.at("symbols").at(0).contains("fullRange") &&
              lazy.at("symbols").at(0).at("unresolved") == true,
          "Lazy symbols were hidden, duplicated or given an invented range");
    const auto documentLazy =
        collectSymbols(Json::array({pending}), "file:///main.cpp", 100, false, resolver, deferred);
    check(documentLazy.at("invalid") == 1 && documentLazy.at("symbols").empty() && called == 1,
          "Document symbols accepted unresolved workspace locations");
    auto invalidRange = pending;
    invalidRange["location"]["range"] = nullptr;
    check(collectSymbols(Json::array({invalidRange}), "", 100, false, resolver, deferred).at("invalid") ==
                  1 &&
              called == 1,
          "Malformed ranges bypassed validation through lazy resolution");
    check(collectSymbols(
              Json::array({pending}), "", 100, false, resolver,
              [](const auto &, const auto &) {
                  return Json();
              }).at("outsideWorkspace") == 1,
          "Lazy symbols bypassed source access filtering");
    auto malformed = parent;
    malformed["selectionRange"] = range(20, 20);
    const auto repaired = collectSymbols(
        Json::array({malformed, 4,
                     Json{{"name", "URI only"}, {"kind", 5}, {"location", {{"uri", "file:///main.cpp"}}}}}),
        "file:///main.cpp", 100, false, resolver);
    check(repaired.at("invalid") == 3 && repaired.at("symbols").size() == 1 &&
              repaired.at("symbols").at(0).at("parent") == 0,
          "Malformed rows hid valid descendants or escaped counts");
    auto unavailable = collectSymbols(Json::array({flat}), "", 100, false,
                                      [](const auto &, const auto &, const auto &) { return Json(); });
    check(unavailable.at("outsideWorkspace") == 1 && unavailable.at("invalid") == 0,
          "Unavailable paths and malformed symbols were conflated");
    Json large = Json::array();
    for (int i = 0; i < 1200; ++i)
        large.push_back(symbol("entry" + std::to_string(i), 13, i, i));
    const auto bounded = collectSymbols(large, "file:///main.cpp", 1000, false, resolver);
    check(bounded.at("symbols").size() == 1000 && bounded.at("truncated"),
          "Symbol row limit was not reported");
    auto deep = symbol("leaf", 5, 0, 1);
    for (int i = 0; i < 70; ++i) {
        auto outer = symbol("scope", 5, 0, 1);
        outer["children"] = Json::array({deep});
        deep = std::move(outer);
    }
    check(collectSymbols(Json::array({deep}), "file:///main.cpp", 1000, false, resolver).at("truncated"),
          "Deep symbol trees escaped the nesting budget");
    SymbolTextBounds text("é🛠x\nlast\n");
    check(text.accepts(range(0, 0, 0, 4)) && text.accepts(range(0, 0, 3, 4)) && text.accepts(range(2, 2)),
          "Valid Unicode/end-of-file ranges were rejected");
    check(!text.accepts(range(0, 0, 2, 3)) && !text.accepts(range(1, 1, 0, 5)) && !text.accepts(range(3, 3)),
          "Surrogate split/out-of-file range accepted");
    bool rejected = false;
    try {
        collectSymbols(Json::object(), "", 100, false, resolver);
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid server envelope became an empty successful response");
}

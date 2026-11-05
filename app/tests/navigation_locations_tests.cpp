#include "cycode/NavigationLocations.h"
#include <stdexcept>
using namespace cycode;
namespace {
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Json range(int a, int b) {
    return {{"start", {{"line", 0}, {"character", a}}}, {"end", {{"line", 0}, {"character", b}}}};
}
} // namespace
void navigationLocationContracts() {
    for (const auto *method : {"definition", "declaration", "typeDefinition", "implementation", "references"})
        check(locationRequest(std::string("language.") + method) == std::string("textDocument/") + method,
              "Incorrect navigation method routing");
    check(!locationRequest("language.complete"), "Non-navigation method entered location routing");
    const auto resolve = [](const std::string &uri, const Json &selection, const Json &) {
        return Json{{"path", uri.substr(8)}, {"range", selection}};
    };
    const Json single = {{"uri", "file:///main.cpp"}, {"range", range(0, 3)}};
    const auto one = collectLocations(single, 100, "int value;\n", resolve);
    check(one.at("locations").size() == 1 && one.at("locations").at(0).at("fullRange") == range(0, 3),
          "Single Location response was lost");
    check(collectLocations(nullptr, 100, "", resolve).at("locations").empty(),
          "Null location response failed");
    const Json link = {{"targetUri", "file:///other.cpp"},
                       {"targetSelectionRange", range(3, 6)},
                       {"targetRange", range(0, 10)},
                       {"originSelectionRange", range(0, 3)}};
    const auto report = collectLocations(Json::array({link, single, link}), 100, "int value;\n", resolve);
    check(report.at("locations").size() == 2 && report.at("invalid") == 0 &&
              report.at("locations").at(1).at("originRange") == range(0, 3) &&
              report.at("locations").at(1).at("range") == range(3, 6),
          "Link extents/origin, de-duplication or deterministic ordering failed");
    auto bad = link;
    bad["targetSelectionRange"] = range(9, 11);
    auto reversed = single;
    reversed["range"] = range(3, 0);
    auto negative = single;
    negative["range"] = range(-1, 0);
    auto missing = link;
    missing.erase("targetRange");
    auto origin = link;
    origin["originSelectionRange"] = range(0, 20);
    auto surrogate = link;
    surrogate["originSelectionRange"] = range(1, 2);
    const auto malformed = collectLocations(
        Json::array({bad, reversed, negative, missing, origin, surrogate, 17}), 100, "é🛠x\n", resolve);
    check(malformed.at("invalid") == 7 && malformed.at("locations").empty(),
          "Invalid target/origin/UTF-16 locations escaped validation");
    check(collectLocations(single, 100, "", [](const auto &, const auto &, const auto &) { return Json(); })
                  .at("outsideWorkspace") == 1,
          "Unavailable locations were conflated with malformed rows");
    auto controls = single;
    controls["uri"] = std::string("file:///bad\0.cpp", 16);
    check(collectLocations(controls, 100, "", resolve).at("invalid") == 1,
          "NUL URI reached source resolution");
    Json large = Json::array();
    for (int i = 0; i < 5100; ++i)
        large.push_back({{"uri", "file:///main.cpp"}, {"range", range(i, i)}});
    const auto bounded = collectLocations(large, 5000, "", resolve);
    check(bounded.at("truncated") == true && bounded.at("locations").size() == 5000,
          "Navigation row budget failed");
    const auto payload =
        collectLocations(large, 5000, "", [](const auto &, const auto &selection, const auto &) {
            return Json{
                {"path", "main.cpp"}, {"range", selection}, {"displayPath", std::string(128 * 1024, 'x')}};
        });
    check(payload.at("truncated") == true && payload.at("locations").size() < 40,
          "Normalized location payload budget failed");
    Json repeated = Json::array();
    for (int i = 0; i < 16400; ++i)
        repeated.push_back(single);
    check(collectLocations(repeated, 100, "", resolve).at("truncated") == true,
          "Duplicate record visit budget failed");
    bool rejected = false;
    try {
        collectLocations(17, 100, "", resolve);
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid location response envelope was accepted");
}

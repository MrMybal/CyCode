#include "cycode/ChangeAnnotations.h"
#include "cycode/TextEdits.h"
#include <stdexcept>

namespace {
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F run) {
    bool failed = false;
    try {
        run();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Invalid annotation operation was accepted");
}
Json edit(const std::string &id, const std::string &replacement = "new") {
    return {{"range", {{"start", {{"line", 0}, {"character", 4}}}, {"end", {{"line", 0}, {"character", 7}}}}},
            {"newText", replacement},
            {"annotationId", id}};
}
} // namespace
void annotationContracts() {
    using namespace cycode;
    const Json definitions = {
        {"server-group",
         {{"label", "Rename référencé"}, {"description", "Review both files"}, {"needsConfirmation", true}}},
        {"unused", {{"label", "Not effective"}, {"needsConfirmation", true}}}};
    const Json files = {{"a.cpp", Json::array({edit("server-group")})},
                        {"b.hpp", Json::array({edit("server-group")})},
                        {"noop.hpp", Json::array({edit("unused", "old")})}};
    const auto normalized = normalizeChangeAnnotations(files, definitions);
    Json effective = Json::object();
    for (const auto &[path, edits] : normalized.files.items()) {
        const auto result = applyTextEdits("int old;\n", edits);
        effective[path] = result.edits;
        if (path != "noop.hpp")
            check(result.text == "int new;\n", "Annotated text edit changed its replacement");
    }
    const auto annotations = usedChangeAnnotations(normalized.annotations, effective);
    check(annotations.size() == 1 && annotations[0].at("files") == Json::array({"a.cpp", "b.hpp"}),
          "Effective annotations lost file grouping or retained a no-op");
    const auto id = annotations[0].at("id").get<std::string>();
    check(id != "server-group" && effective.at("a.cpp")[0].at("annotationId") == id,
          "Opaque server annotation IDs were not normalized");
    rejects([&] { confirmChangeAnnotations(annotations, Json::array()); });
    rejects([&] { confirmChangeAnnotations(annotations, {"unknown"}); });
    rejects([&] { confirmChangeAnnotations(annotations, {id, id}); });
    rejects([&] { confirmChangeAnnotations(annotations, {{"id", id}}); });
    confirmChangeAnnotations(annotations, {id});
    const auto one = usedChangeAnnotations(annotations, Json{{"a.cpp", effective.at("a.cpp")}});
    check(one[0].at("files") == Json::array({"a.cpp"}), "File preview leaked another annotation target");
    auto optional = annotations;
    optional[0]["needsConfirmation"] = false;
    confirmChangeAnnotations(optional, Json::array());
    check(usedChangeAnnotations(normalized.annotations, Json{{"noop.hpp", effective.at("noop.hpp")}}).empty(),
          "No-op annotation required approval");
    const auto plain = normalizeChangeAnnotations(Json{{"a.cpp", Json::array()}}, Json::object());
    check(plain.annotations.empty(), "Legacy plain edits gained confirmations");
    confirmChangeAnnotations(Json::array(), Json::array());
    rejects([&] { normalizeChangeAnnotations(files, Json::object()); });
    auto invalid = definitions;
    invalid["server-group"]["label"] = "";
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = definitions;
    invalid["server-group"]["needsConfirmation"] = "yes";
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = definitions;
    invalid["server-group"]["description"] = std::string("hidden\0text", 11);
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = definitions;
    invalid["server-group"]["label"] = std::string(2049, 'x');
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = definitions;
    invalid["server-group"]["description"] = std::string(4097, 'x');
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = definitions;
    invalid[std::string(513, 'x')] = {{"label", "Oversized ID"}};
    rejects([&] { normalizeChangeAnnotations(files, invalid); });
    invalid = Json::object();
    for (int i = 0; i < 129; ++i)
        invalid[std::to_string(i)] = {{"label", "Many groups"}};
    rejects([&] { normalizeChangeAnnotations(Json::object(), invalid); });
    invalid = Json::object();
    for (int i = 0; i < 128; ++i)
        invalid[std::to_string(i)] = {{"label", std::string(2048, 'x')},
                                      {"description", std::string(4096, 'x')}};
    rejects([&] { normalizeChangeAnnotations(Json::object(), invalid); });
    const auto emptyId = normalizeChangeAnnotations(Json{{"a.cpp", Json::array({edit("")})}},
                                                    Json{{"", {{"label", "Empty server ID"}}}});
    check(emptyId.annotations.size() == 1, "Valid empty server ID was rejected");
    auto badFiles = files;
    badFiles["a.cpp"][0]["annotationId"] = nullptr;
    rejects([&] { normalizeChangeAnnotations(badFiles, definitions); });
    rejects([&] { usedChangeAnnotations(Json::array(), effective); });
    auto overlapping = normalized.files.at("a.cpp");
    overlapping.push_back(overlapping[0]);
    rejects([&] { applyTextEdits("int old;\n", overlapping); });
    check(files.at("a.cpp")[0].at("annotationId") == "server-group",
          "Annotation normalization changed the server response");
}

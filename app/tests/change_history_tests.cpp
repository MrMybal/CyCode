#include "cycode/Engine.h"
#include "cycode/SnippetCatalogue.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <stdexcept>

namespace cycode {
namespace {
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
template<class F> void refuses(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception &) { rejected = true; }
    require(rejected, "Stale or incomplete native history was accepted");
}
void write(const std::filesystem::path &path, const std::string &text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file) throw std::runtime_error("Cannot write history fixture");
}
void checkProfile(const std::filesystem::path &root, bool paged) {
    std::filesystem::create_directories(root / ".cycode");
    const std::string path = paged ? "history.log" : "history.verse";
    write(root / path, "z");
    write(root / utf8Path(std::string(snippetCataloguePath)),
          Json{{"Insert", {{"prefix", "z"}, {"scope", "verse"}, {"body", "zé🛠"}}}}.dump());
    Engine engine(root); // No compiler or external process is needed by this fixture.
    auto document = [&] {
        return engine.dispatch(paged ? "documents.readPage" : "documents.read", {{"path", path}});
    };
    auto prepare = [&] {
        const auto doc = document();
        if (paged)
            return engine.dispatch("documents.prepareByteEdit", {{"path", path},
                {"expectedRevision", doc.at("revision")}, {"offset", 1}, {"length", 0}, {"text", "é🛠"}});
        const auto completion = engine.dispatch("snippets.complete", {{"path", path},
            {"expectedRevision", doc.at("revision")}, {"position", {{"line", 0}, {"character", 1}}}});
        return engine.dispatch("language.completionPrepare", {{"sessionId", completion.at("sessionId")},
            {"item", 0}, {"expectedRevision", doc.at("revision")}}).at("plan");
    };
    auto apply = [&](const Json &plan) {
        const auto result = engine.dispatch("changes.apply", {{"id", plan.at("id")}});
        const auto &receipt = result.at("commit");
        const auto live = document();
        const auto &row = receipt.at("documents").at(0);
        require(row.at("id") == live.at("id") && row.at("revision") == live.at("revision") &&
                row.at("dirty") == live.at("dirty") && row.at("path") == path &&
                row.at("bytes") == live.at("text").get<std::string>().size() &&
                row.at("storage") == (paged ? "paged" : "buffer"), "Commit receipt is not the committed document");
        require(receipt.at("transaction") == plan.at("id"), "Receipt lost transaction identity");
        return result;
    };
    auto historyPlan = [&](const std::string &direction) {
        const auto state = engine.dispatch("changes.historyStatus");
        const auto doc = document();
        return engine.dispatch("changes.historyPrepare", {{"id", state.at(direction).at("id")},
            {"direction", direction}, {"expectedHistoryRevision", state.at("revision")},
            {"documents", Json::array({{{"path", path}, {"expectedDocumentId", doc.at("id")},
                                       {"expectedRevision", doc.at("revision")}}})}});
    };
    const auto first = apply(prepare());
    const auto second = apply(prepare());
    require(document().at("text") == "zé🛠é🛠", "History fixture did not edit the actual native profile");
    require(engine.dispatch("changes.status", {{"id", first.at("id")}}).at("commit") == first.at("commit"),
            "Later edits changed an earlier commit receipt");
    for (int i = 0; i < 10; ++i) prepare();
    require(engine.dispatch("changes.status", {{"id", first.at("id")}}).at("state") == "applied",
            "Preview eviction removed a retained Undo transaction");
    const auto state = engine.dispatch("changes.historyStatus");
    refuses([&] { engine.dispatch("changes.historyPrepare", {{"id", second.at("id")},
        {"direction", "undo"}, {"expectedHistoryRevision", state.at("revision")}, {"documents", Json::array()}}); });
    const auto undoSecond = historyPlan("undo");
    const auto twin = historyPlan("undo");
    require(engine.dispatch("changes.historyStatus") == state, "Preparing history mutated live history");
    const auto preview = engine.dispatch("changes.preview", {{"id", undoSecond.at("id")},
        {"path", path}, {"length", 4096}});
    require(preview.at("before") == "zé🛠é🛠" && preview.at("after") == "zé🛠",
            "History preview does not restore its exact snapshot");
    require(apply(undoSecond).at("commit").at("action") == "undo", "Undo action missing from receipt");
    refuses([&] { apply(twin); });
    // Legacy API must also work after the causal native restoration of B.
    const auto undoFirst = engine.dispatch("changes.undo", {{"id", first.at("id")}});
    require(undoFirst.at("commit").at("action") == "undo" && document().at("text") == "z",
            "Second Undo rejected the native-restored preimage");
    require(apply(historyPlan("redo")).at("commit").at("action") == "redo", "Redo receipt missing");
    apply(historyPlan("redo"));
    require(document().at("text") == "zé🛠é🛠", "Two-step Redo lost a native transaction");
    const auto diskConflict = historyPlan("undo");
    write(root / path, "external");
    refuses([&] { apply(diskConflict); });
    require(document().at("text") == "zé🛠é🛠" && readTextFileBytes(root / path) == "external",
            "Disk conflict partially committed history");
    write(root / path, "z");
    apply(historyPlan("undo"));
    const auto abandoned = historyPlan("redo");
    apply(prepare());
    require(engine.dispatch("changes.historyStatus").at("redoCount") == 0, "New edit retained an abandoned Redo branch");
    refuses([&] { apply(abandoned); });
    const auto doc = document();
    engine.dispatch(paged ? "documents.closePaged" : "documents.close", {{"path", path},
        {"expectedRevision", doc.at("revision")}, {"discard", true}});
    const auto reopened = document();
    require(reopened.at("id") != doc.at("id"), "Reopen did not replace the document identity");
    const auto closed = engine.dispatch("changes.historyStatus");
    require(closed.at("undoCount") == 0 && closed.at("redoCount") == 0 && closed.at("dropped") > 0,
            "Close/reopen retained stale native history");
    refuses([&] { engine.dispatch("changes.undo", {{"id", first.at("id")}}); });
    if (paged) {
        for (int i = 0; i < 260; ++i) apply(prepare());
        const auto bounded = engine.dispatch("changes.historyStatus");
        require(bounded.at("retentionLimit") == 256 && bounded.at("undoCount") == 256 &&
                bounded.at("dropped") >= 4, "Native history did not report its retention bound");
        for (int i = 0; i < 3; ++i) apply(historyPlan("undo"));
        for (int i = 0; i < 3; ++i) apply(historyPlan("redo"));
    }
    require(readTextFileBytes(root / path) == "z", "Native history saved workspace text implicitly");
}
} // namespace
void changeHistoryContracts(const std::filesystem::path &root) {
    checkProfile(root / "history-buffer", false);
    checkProfile(root / "history-paged", true);
}
} // namespace cycode
